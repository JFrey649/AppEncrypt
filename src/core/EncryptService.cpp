#include "core/EncryptService.h"

#include "core/AppConfig.h"
#include "core/CryptoService.h"
#include "core/FileLayout.h"
#include "core/ProcessUtil.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

struct VaultPayloadHeader {
    uint8_t nonce[12];
    uint8_t tag[16];
};

std::vector<uint8_t> wrapDek(const std::vector<uint8_t>& dek, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> tag;
    const auto wrapped = CryptoService::encrypt(dek, key, nonce, tag);
    std::vector<uint8_t> blob(sizeof(VaultPayloadHeader) + wrapped.size());
    VaultPayloadHeader header{};
    std::memcpy(header.nonce, nonce.data(), nonce.size());
    std::memcpy(header.tag, tag.data(), tag.size());
    std::memcpy(blob.data(), &header, sizeof(header));
    std::memcpy(blob.data() + sizeof(header), wrapped.data(), wrapped.size());
    return blob;
}

std::vector<uint8_t> unwrapDek(const std::vector<uint8_t>& blob, const std::vector<uint8_t>& key) {
    if (blob.size() <= sizeof(VaultPayloadHeader)) {
        throw std::runtime_error("wrapped dek corrupted");
    }
    VaultPayloadHeader header{};
    std::memcpy(&header, blob.data(), sizeof(header));
    std::vector<uint8_t> cipher(blob.begin() + sizeof(header), blob.end());
    std::vector<uint8_t> nonce(header.nonce, header.nonce + CryptoService::kNonceSize);
    std::vector<uint8_t> tag(header.tag, header.tag + CryptoService::kTagSize);
    return CryptoService::decrypt(cipher, key, nonce, tag);
}

void writeVault(const std::string& path, const std::vector<uint8_t>& plain, const std::vector<uint8_t>& dek) {
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> tag;
    const auto cipher = CryptoService::encrypt(plain, dek, nonce, tag);
    VaultPayloadHeader header{};
    std::memcpy(header.nonce, nonce.data(), nonce.size());
    std::memcpy(header.tag, tag.data(), tag.size());
    std::vector<uint8_t> vault(sizeof(VaultPayloadHeader) + cipher.size());
    std::memcpy(vault.data(), &header, sizeof(header));
    std::memcpy(vault.data() + sizeof(header), cipher.data(), cipher.size());
    FileLayout::writeFileBytes(path, vault);
}

std::vector<uint8_t> readVaultPlain(const std::string& vaultPath, const std::vector<uint8_t>& dek) {
    const auto vaultBytes = FileLayout::readFileBytes(vaultPath);
    if (vaultBytes.size() <= sizeof(VaultPayloadHeader)) {
        throw std::runtime_error("密文包损坏");
    }
    VaultPayloadHeader header{};
    std::memcpy(&header, vaultBytes.data(), sizeof(header));
    std::vector<uint8_t> cipher(vaultBytes.begin() + sizeof(header), vaultBytes.end());
    std::vector<uint8_t> nonce(header.nonce, header.nonce + CryptoService::kNonceSize);
    std::vector<uint8_t> tag(header.tag, header.tag + CryptoService::kTagSize);
    return CryptoService::decrypt(cipher, dek, nonce, tag);
}

std::string getModuleDirectory() {
    return AppConfig::instance().installDirectory();
}

} // namespace

EncryptService::EncryptService(Database& db) : db_(db) {}

std::string EncryptService::installDirectory() {
    return AppConfig::instance().installDirectory();
}

std::string EncryptService::databasePath() {
    return AppConfig::instance().databasePath();
}

ServiceResult EncryptService::encryptExe(const std::string& exePath,
                                         const std::string& password,
                                         const std::string& stubTemplatePath,
                                         const std::function<void(int percent)>& progress) {
    ServiceResult result;
    if (password.size() < 6) {
        result.message = "密码长度至少 6 位";
        return result;
    }
    if (!fs::exists(exePath)) {
        result.message = "文件不存在";
        return result;
    }
    if (FileLayout::isEncryptedStub(exePath)) {
        result.message = "该程序已加密，请使用更改密码或解密";
        return result;
    }
    if (ProcessUtil::isExeRunning(exePath)) {
        result.message = "程序正在运行，请先关闭后再加密";
        return result;
    }
    if (!fs::exists(stubTemplatePath)) {
        result.message = "找不到启动器模板 AppEncryptStub.exe";
        return result;
    }

    try {
        if (progress) {
            progress(10);
        }
        const auto plain = FileLayout::readFileBytes(exePath);
        const auto salt = CryptoService::generateSalt();
        const auto keys = CryptoService::deriveKeys(password, salt);
        const auto dek = CryptoService::generateDek();

        const std::string vaultPath = FileLayout::vaultPathFor(exePath);
        const std::string tempExe = exePath + ".encrypting";
        const std::string tempVault = vaultPath + ".tmp";

        writeVault(tempVault, plain, dek);

        if (progress) {
            progress(50);
        }

        fs::copy_file(stubTemplatePath, tempExe, fs::copy_options::overwrite_existing);

        const auto recordId = FileLayout::makeRecordId();
        const fs::path vaultRel = fs::path(exePath).filename().string() + kVaultExtension;
        StubTailConfig tail{recordId, vaultRel.string(), AppConfig::instance().mainExePath()};
        if (!FileLayout::appendStubTail(tempExe, tail)) {
            throw std::runtime_error("写入启动器配置失败");
        }

        fs::rename(tempVault, vaultPath);
        fs::rename(tempExe, exePath);

        EncryptedFileRecord record;
        record.id = recordId;
        record.originalPath = fs::absolute(exePath).string();
        record.vaultPath = fs::absolute(vaultPath).string();
        record.fileSize = static_cast<int64_t>(plain.size());
        record.fileHash = FileLayout::sha256Hex(plain);
        record.salt = salt;
        record.passwordVerifier = keys.passwordVerifier;
        record.passwordWrappedDek = wrapDek(dek, keys.encryptionKey);
        record.dpapiWrappedDek = CryptoService::dpapiProtect(dek);
        record.cipherVersion = 1;
        record.status = "active";
        record.createdAt = nowUnix();
        record.updatedAt = record.createdAt;

        if (!db_.upsertRecord(record)) {
            throw std::runtime_error(db_.lastError());
        }

        if (progress) {
            progress(100);
        }
        result.success = true;
        result.message = "加密成功";
    } catch (const std::exception& ex) {
        fs::remove(exePath + ".encrypting");
        fs::remove(FileLayout::vaultPathFor(exePath) + ".tmp");
        result.message = ex.what();
    }
    return result;
}

ServiceResult EncryptService::decryptExe(const std::string& exePath,
                                         const std::string& password,
                                         const std::function<void(int percent)>& progress) {
    ServiceResult result;
    try {
        const auto absPath = fs::absolute(exePath).string();
        auto record = db_.findByPath(absPath);
        if (!record) {
            result.message = "未找到加密记录";
            return result;
        }
        if (!CryptoService::verifyPassword(password, record->salt, record->passwordVerifier)) {
            result.message = "密码错误";
            return result;
        }

        if (progress) {
            progress(20);
        }
        const auto keys = CryptoService::deriveKeys(password, record->salt);
        const auto dek = unwrapDek(record->passwordWrappedDek, keys.encryptionKey);
        const auto plain = readVaultPlain(record->vaultPath, dek);

        const std::string tempRestore = exePath + ".decrypting";
        FileLayout::writeFileBytes(tempRestore, plain);
        fs::remove(record->vaultPath);
        fs::rename(tempRestore, exePath);
        db_.removeRecord(record->id);

        if (progress) {
            progress(100);
        }
        result.success = true;
        result.message = "解密成功";
    } catch (const std::exception& ex) {
        result.message = ex.what();
    }
    return result;
}

ServiceResult EncryptService::changePassword(const std::string& exePath,
                                             const std::string& oldPassword,
                                             const std::string& newPassword) {
    ServiceResult result;
    if (newPassword.size() < 6) {
        result.message = "新密码长度至少 6 位";
        return result;
    }

    try {
        const auto absPath = fs::absolute(exePath).string();
        auto record = db_.findByPath(absPath);
        if (!record) {
            result.message = "未找到加密记录";
            return result;
        }
        if (!CryptoService::verifyPassword(oldPassword, record->salt, record->passwordVerifier)) {
            result.message = "旧密码错误";
            return result;
        }

        const auto oldKeys = CryptoService::deriveKeys(oldPassword, record->salt);
        const auto dek = unwrapDek(record->passwordWrappedDek, oldKeys.encryptionKey);

        const auto newSalt = CryptoService::generateSalt();
        const auto newKeys = CryptoService::deriveKeys(newPassword, newSalt);

        record->salt = newSalt;
        record->passwordVerifier = newKeys.passwordVerifier;
        record->passwordWrappedDek = wrapDek(dek, newKeys.encryptionKey);
        record->dpapiWrappedDek = CryptoService::dpapiProtect(dek);
        record->updatedAt = nowUnix();

        if (!db_.upsertRecord(*record)) {
            throw std::runtime_error(db_.lastError());
        }

        result.success = true;
        result.message = "密码已更改";
    } catch (const std::exception& ex) {
        result.message = ex.what();
    }
    return result;
}

ServiceResult EncryptService::unlockAndLaunch(const std::string& stubPath,
                                              const std::string& password,
                                              const std::vector<std::string>& hostArgs) {
    ServiceResult result;
    try {
        const fs::path absStub = fs::absolute(fs::u8path(stubPath));
        auto record = db_.findByPath(absStub.u8string());
        if (!record) {
            result.message = "未找到加密记录，可能已移动或数据损坏";
            return result;
        }
        if (!CryptoService::verifyPassword(password, record->salt, record->passwordVerifier)) {
            result.message = "密码错误";
            return result;
        }

        const auto keys = CryptoService::deriveKeys(password, record->salt);
        const auto dek = unwrapDek(record->passwordWrappedDek, keys.encryptionKey);
        const auto plain = readVaultPlain(record->vaultPath, dek);

        // 解密后的 exe 必须放在原程序同目录，Windows 才会从该目录加载 side-by-side DLL。
        const fs::path hostDir = absStub.parent_path();
        const fs::path hostPath = hostDir / (record->id + ".appencrypt.run.exe");
        FileLayout::writeFileBytes(hostPath, plain);

        const auto launch = ProcessUtil::launchAndWait(hostPath, hostArgs, hostDir);
        std::error_code ec;
        fs::remove(hostPath, ec);

        if (!launch.success) {
            result.message = launch.message;
            return result;
        }

        result.success = true;
        result.message = launch.message;
    } catch (const std::exception& ex) {
        result.message = ex.what();
    }
    return result;
}

ServiceResult EncryptService::restoreAllForUninstall() {
    ServiceResult result;
    int okCount = 0;
    int failCount = 0;
    std::string failMsg;

    const auto records = db_.listActiveRecords();
    for (const auto& record : records) {
        try {
            const auto dek = CryptoService::dpapiUnprotect(record.dpapiWrappedDek);
            const auto plain = readVaultPlain(record.vaultPath, dek);

            const std::string tempRestore = record.originalPath + ".restoring";
            FileLayout::writeFileBytes(tempRestore, plain);
            fs::remove(record.vaultPath);
            fs::rename(tempRestore, record.originalPath);
            db_.removeRecord(record.id);
            ++okCount;
        } catch (const std::exception& ex) {
            ++failCount;
            failMsg += record.originalPath + ": " + ex.what() + "\n";
        }
    }

    result.success = failCount == 0;
    result.message = "已还原 " + std::to_string(okCount) + " 个程序";
    if (failCount > 0) {
        result.message += "，失败 " + std::to_string(failCount) + " 个\n" + failMsg;
    }
    return result;
}

} // namespace appencrypt
