#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace appencrypt {

struct EncryptedFileRecord {
    std::string id;
    std::string originalPath;
    std::string vaultPath;
    int64_t fileSize = 0;
    std::string fileHash;
    std::vector<uint8_t> salt;
    std::vector<uint8_t> passwordVerifier;
    std::vector<uint8_t> passwordWrappedDek;
    std::vector<uint8_t> dpapiWrappedDek;
    int cipherVersion = 1;
    std::string status;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

class Database {
public:
    explicit Database(const std::string& dbPath);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open();
    bool initializeSchema();
    bool upsertRecord(const EncryptedFileRecord& record);
    std::optional<EncryptedFileRecord> findByPath(const std::string& path);
    std::optional<EncryptedFileRecord> findById(const std::string& id);
    std::vector<EncryptedFileRecord> listActiveRecords();
    bool removeRecord(const std::string& id);

    std::string lastError() const { return lastError_; }

private:
    std::string dbPath_;
    sqlite3* db_ = nullptr;
    std::string lastError_;
};

} // namespace appencrypt
