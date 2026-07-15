#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace appencrypt {

inline constexpr char kMagic[] = "APPENC";
inline constexpr uint8_t kMagicVersion = 2;
inline constexpr char kVaultExtension[] = ".appencrypt";

struct StubTailConfig {
    std::string recordId;
    std::string vaultRelativePath;
    std::string launcherExePath; // v2：加密时写入，Stub 无需读注册表
};

class FileLayout {
public:
    static std::string vaultPathFor(const std::string& exePath);
    static std::string makeRecordId();

    static bool appendStubTail(const std::string& stubPath, const StubTailConfig& config);
    static std::optional<StubTailConfig> readStubTail(const std::string& stubPath);
    static bool isEncryptedStub(const std::string& stubPath);

    static std::vector<uint8_t> readFileBytes(const std::string& path);
    static void writeFileBytes(const std::string& path, const std::vector<uint8_t>& data);
    static void writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data);
    static std::string sha256Hex(const std::vector<uint8_t>& data);
};

} // namespace appencrypt
