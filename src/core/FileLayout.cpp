#include "core/FileLayout.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

std::string randomUuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    const char* hex = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);
    for (int i = 0; i < 32; ++i) {
        uuid.push_back(hex[dist(gen)]);
        if (i == 7 || i == 11 || i == 15 || i == 19) {
            uuid.push_back('-');
        }
    }
    return uuid;
}

} // namespace

std::string FileLayout::vaultPathFor(const std::string& exePath) {
    return exePath + kVaultExtension;
}

std::string FileLayout::makeRecordId() {
    return randomUuid();
}

bool FileLayout::appendStubTail(const std::string& stubPath, const StubTailConfig& config) {
    std::ofstream out(stubPath, std::ios::binary | std::ios::app);
    if (!out) {
        return false;
    }

    const uint32_t idLen = static_cast<uint32_t>(config.recordId.size());
    const uint32_t vaultLen = static_cast<uint32_t>(config.vaultRelativePath.size());
    const uint32_t launcherLen = static_cast<uint32_t>(config.launcherExePath.size());

    out.write(kMagic, 6);
    const auto version = kMagicVersion;
    out.write(reinterpret_cast<const char*>(&version), 1);
    out.write(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
    out.write(config.recordId.data(), static_cast<std::streamsize>(idLen));
    out.write(reinterpret_cast<const char*>(&vaultLen), sizeof(vaultLen));
    out.write(config.vaultRelativePath.data(), static_cast<std::streamsize>(vaultLen));
    out.write(reinterpret_cast<const char*>(&launcherLen), sizeof(launcherLen));
    out.write(config.launcherExePath.data(), static_cast<std::streamsize>(launcherLen));
    return static_cast<bool>(out);
}

std::optional<StubTailConfig> FileLayout::readStubTail(const std::string& stubPath) {
    std::ifstream in(stubPath, std::ios::binary | std::ios::ate);
    if (!in) {
        return std::nullopt;
    }

    const auto fileSize = in.tellg();
    if (fileSize < 20) {
        return std::nullopt;
    }

    // Scan backwards for magic within last 4KB
    const auto scanSize = static_cast<std::streamoff>(std::min<int64_t>(fileSize, 4096));
    in.seekg(fileSize - scanSize);

    std::vector<char> buffer(static_cast<size_t>(scanSize));
    in.read(buffer.data(), scanSize);
    if (!in) {
        return std::nullopt;
    }

    for (int i = static_cast<int>(buffer.size()) - 7; i >= 0; --i) {
        if (std::memcmp(buffer.data() + i, kMagic, 6) != 0) {
            continue;
        }
        size_t offset = static_cast<size_t>(i) + 6;
        if (offset >= buffer.size()) {
            continue;
        }
        const uint8_t version = static_cast<uint8_t>(buffer[offset++]);
        if (version != 1 && version != 2) {
            continue;
        }
        if (offset + 8 > buffer.size()) {
            continue;
        }
        uint32_t idLen = 0;
        std::memcpy(&idLen, buffer.data() + offset, 4);
        offset += 4;
        if (offset + idLen + 4 > buffer.size()) {
            continue;
        }
        StubTailConfig cfg;
        cfg.recordId.assign(buffer.data() + offset, idLen);
        offset += idLen;
        uint32_t vaultLen = 0;
        std::memcpy(&vaultLen, buffer.data() + offset, 4);
        offset += 4;
        if (offset + vaultLen > buffer.size()) {
            continue;
        }
        cfg.vaultRelativePath.assign(buffer.data() + offset, vaultLen);
        offset += vaultLen;
        if (version >= 2) {
            if (offset + 4 > buffer.size()) {
                continue;
            }
            uint32_t launcherLen = 0;
            std::memcpy(&launcherLen, buffer.data() + offset, 4);
            offset += 4;
            if (offset + launcherLen > buffer.size()) {
                continue;
            }
            cfg.launcherExePath.assign(buffer.data() + offset, launcherLen);
        }
        return cfg;
    }
    return std::nullopt;
}

bool FileLayout::isEncryptedStub(const std::string& stubPath) {
    return readStubTail(stubPath).has_value();
}

std::vector<uint8_t> FileLayout::readFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void FileLayout::writeFileBytes(const std::string& path, const std::vector<uint8_t>& data) {
    writeFileBytes(fs::u8path(path), data);
}

void FileLayout::writeFileBytes(const fs::path& path, const std::vector<uint8_t>& data) {
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot write file: " + path.u8string());
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::string FileLayout::sha256Hex(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD hashObjectSize = 0;
    DWORD hashLength = 0;
    DWORD cbData = 0;

    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjectSize), sizeof(DWORD), &cbData, 0);
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(DWORD), &cbData, 0);

    std::vector<uint8_t> hashObject(hashObjectSize);
    std::vector<uint8_t> hashBytes(hashLength);

    BCryptCreateHash(alg, &hash, hashObject.data(), hashObjectSize, nullptr, 0, 0);
    BCryptHashData(hash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0);
    BCryptFinishHash(hash, hashBytes.data(), hashLength, 0);

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    std::ostringstream oss;
    for (const auto b : hashBytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

} // namespace appencrypt
