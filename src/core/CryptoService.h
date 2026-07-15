#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace appencrypt {

struct DerivedKeys {
    std::vector<uint8_t> encryptionKey; // 32 bytes
    std::vector<uint8_t> passwordVerifier; // 32 bytes
};

class CryptoService {
public:
    static constexpr int kPbkdf2Iterations = 100000;
    static constexpr size_t kKeySize = 32;
    static constexpr size_t kSaltSize = 16;
    static constexpr size_t kNonceSize = 12;
    static constexpr size_t kTagSize = 16;

    static std::vector<uint8_t> generateSalt();
    static std::vector<uint8_t> generateDek();
    static DerivedKeys deriveKeys(const std::string& password, const std::vector<uint8_t>& salt);
    static std::vector<uint8_t> dpapiProtect(const std::vector<uint8_t>& plain);
    static std::vector<uint8_t> dpapiUnprotect(const std::vector<uint8_t>& cipher);
    static bool verifyPassword(const std::string& password,
                               const std::vector<uint8_t>& salt,
                               const std::vector<uint8_t>& verifier);

    static std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext,
                                        const std::vector<uint8_t>& key,
                                        std::vector<uint8_t>& nonceOut,
                                        std::vector<uint8_t>& tagOut);

    static std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext,
                                        const std::vector<uint8_t>& key,
                                        const std::vector<uint8_t>& nonce,
                                        const std::vector<uint8_t>& tag);
};

} // namespace appencrypt
