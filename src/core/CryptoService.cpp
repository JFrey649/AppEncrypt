#include "core/CryptoService.h"

#include <Windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <random>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace appencrypt {

namespace {

void throwOnError(NTSTATUS status, const char* msg) {
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error(msg);
    }
}

std::vector<uint8_t> pbkdf2Sha256(const std::string& password,
                                  const std::vector<uint8_t>& salt,
                                  int iterations,
                                  size_t outLen) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    throwOnError(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG),
                 "BCryptOpenAlgorithmProvider failed");

    std::vector<uint8_t> out(outLen);
    const auto status = BCryptDeriveKeyPBKDF2(
        alg,
        reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
        static_cast<ULONG>(password.size()),
        const_cast<PUCHAR>(salt.data()),
        static_cast<ULONG>(salt.size()),
        static_cast<ULONGLONG>(iterations),
        out.data(),
        static_cast<ULONG>(out.size()),
        0);

    BCryptCloseAlgorithmProvider(alg, 0);
    throwOnError(status, "BCryptDeriveKeyPBKDF2 failed");
    return out;
}

} // namespace

std::vector<uint8_t> CryptoService::generateDek() {
    return generateSalt(); // 32 bytes random
}

std::vector<uint8_t> CryptoService::dpapiProtect(const std::vector<uint8_t>& plain) {
    DATA_BLOB in{};
    in.pbData = const_cast<BYTE*>(plain.data());
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"AppEncrypt DEK", nullptr, nullptr, nullptr, 0, &out)) {
        throw std::runtime_error("CryptProtectData failed");
    }
    std::vector<uint8_t> result(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::vector<uint8_t> CryptoService::dpapiUnprotect(const std::vector<uint8_t>& cipher) {
    DATA_BLOB in{};
    in.pbData = const_cast<BYTE*>(cipher.data());
    in.cbData = static_cast<DWORD>(cipher.size());
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        throw std::runtime_error("CryptUnprotectData failed");
    }
    std::vector<uint8_t> result(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::vector<uint8_t> CryptoService::generateSalt() {
    std::vector<uint8_t> salt(kSaltSize);
    throwOnError(BCryptGenRandom(nullptr, salt.data(), static_cast<ULONG>(salt.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG),
                 "BCryptGenRandom failed");
    return salt;
}

DerivedKeys CryptoService::deriveKeys(const std::string& password, const std::vector<uint8_t>& salt) {
    auto material = pbkdf2Sha256(password, salt, kPbkdf2Iterations, kKeySize * 2);
    DerivedKeys keys;
    keys.encryptionKey.assign(material.begin(), material.begin() + kKeySize);
    keys.passwordVerifier.assign(material.begin() + kKeySize, material.end());
    return keys;
}

bool CryptoService::verifyPassword(const std::string& password,
                                   const std::vector<uint8_t>& salt,
                                   const std::vector<uint8_t>& verifier) {
    const auto keys = deriveKeys(password, salt);
    return keys.passwordVerifier == verifier;
}

std::vector<uint8_t> CryptoService::encrypt(const std::vector<uint8_t>& plaintext,
                                            const std::vector<uint8_t>& key,
                                            std::vector<uint8_t>& nonceOut,
                                            std::vector<uint8_t>& tagOut) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;

    throwOnError(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0),
                 "BCryptOpenAlgorithmProvider AES failed");
    throwOnError(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                   sizeof(BCRYPT_CHAIN_MODE_GCM), 0),
                 "BCryptSetProperty GCM failed");

    throwOnError(BCryptGenerateSymmetricKey(alg, &keyHandle, nullptr, 0,
                                            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0),
                 "BCryptGenerateSymmetricKey failed");

    nonceOut = generateSalt();
    if (nonceOut.size() > kNonceSize) {
        nonceOut.resize(kNonceSize);
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    tagOut.assign(kTagSize, 0);
    authInfo.pbNonce = nonceOut.data();
    authInfo.cbNonce = static_cast<ULONG>(nonceOut.size());
    authInfo.pbTag = tagOut.data();
    authInfo.cbTag = static_cast<ULONG>(tagOut.size());

    std::vector<uint8_t> ciphertext(plaintext.size());
    ULONG produced = 0;
    throwOnError(BCryptEncrypt(keyHandle,
                               const_cast<PUCHAR>(plaintext.data()),
                               static_cast<ULONG>(plaintext.size()),
                               &authInfo,
                               nullptr, 0,
                               ciphertext.data(),
                               static_cast<ULONG>(ciphertext.size()),
                               &produced,
                               0),
                 "BCryptEncrypt failed");

    BCryptDestroyKey(keyHandle);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ciphertext;
}

std::vector<uint8_t> CryptoService::decrypt(const std::vector<uint8_t>& ciphertext,
                                            const std::vector<uint8_t>& key,
                                            const std::vector<uint8_t>& nonce,
                                            const std::vector<uint8_t>& tag) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;

    throwOnError(BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0),
                 "BCryptOpenAlgorithmProvider AES failed");
    throwOnError(BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                   sizeof(BCRYPT_CHAIN_MODE_GCM), 0),
                 "BCryptSetProperty GCM failed");

    throwOnError(BCryptGenerateSymmetricKey(alg, &keyHandle, nullptr, 0,
                                            const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0),
                 "BCryptGenerateSymmetricKey failed");

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = const_cast<PUCHAR>(nonce.data());
    authInfo.cbNonce = static_cast<ULONG>(nonce.size());
    authInfo.pbTag = const_cast<PUCHAR>(tag.data());
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    std::vector<uint8_t> plaintext(ciphertext.size());
    ULONG produced = 0;
    const auto status = BCryptDecrypt(keyHandle,
                                      const_cast<PUCHAR>(ciphertext.data()),
                                      static_cast<ULONG>(ciphertext.size()),
                                      &authInfo,
                                      nullptr, 0,
                                      plaintext.data(),
                                      static_cast<ULONG>(plaintext.size()),
                                      &produced,
                                      0);

    BCryptDestroyKey(keyHandle);
    BCryptCloseAlgorithmProvider(alg, 0);

    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("decryption failed: wrong password or corrupted data");
    }
    plaintext.resize(produced);
    return plaintext;
}

} // namespace appencrypt
