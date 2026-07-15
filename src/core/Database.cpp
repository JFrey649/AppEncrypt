#include "core/Database.h"

#include <sqlite3.h>

#include <chrono>

namespace appencrypt {

namespace {

int64_t nowUnix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

Database::Database(const std::string& dbPath) : dbPath_(dbPath) {}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::open() {
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    return true;
}

bool Database::initializeSchema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS encrypted_files ("
        "  id TEXT PRIMARY KEY,"
        "  original_path TEXT NOT NULL UNIQUE,"
        "  vault_path TEXT NOT NULL,"
        "  file_size INTEGER NOT NULL,"
        "  file_hash TEXT NOT NULL,"
        "  salt BLOB NOT NULL,"
        "  password_verifier BLOB NOT NULL,"
        "  password_wrapped_dek BLOB NOT NULL,"
        "  dpapi_wrapped_dek BLOB NOT NULL,"
        "  cipher_version INTEGER NOT NULL,"
        "  status TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL,"
        "  updated_at INTEGER NOT NULL"
        ");";

    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        lastError_ = err ? err : "schema init failed";
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::upsertRecord(const EncryptedFileRecord& r) {
    const char* sql =
        "INSERT INTO encrypted_files (id, original_path, vault_path, file_size, file_hash,"
        " salt, password_verifier, password_wrapped_dek, dpapi_wrapped_dek, cipher_version, status, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(original_path) DO UPDATE SET"
        " vault_path=excluded.vault_path, file_size=excluded.file_size, file_hash=excluded.file_hash,"
        " salt=excluded.salt, password_verifier=excluded.password_verifier,"
        " password_wrapped_dek=excluded.password_wrapped_dek, dpapi_wrapped_dek=excluded.dpapi_wrapped_dek,"
        " cipher_version=excluded.cipher_version, status=excluded.status, updated_at=excluded.updated_at;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }

    sqlite3_bind_text(stmt, 1, r.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, r.originalPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, r.vaultPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, r.fileSize);
    sqlite3_bind_text(stmt, 5, r.fileHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 6, r.salt.data(), static_cast<int>(r.salt.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 7, r.passwordVerifier.data(), static_cast<int>(r.passwordVerifier.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 8, r.passwordWrappedDek.data(), static_cast<int>(r.passwordWrappedDek.size()), SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 9, r.dpapiWrappedDek.data(), static_cast<int>(r.dpapiWrappedDek.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, r.cipherVersion);
    sqlite3_bind_text(stmt, 11, r.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 12, r.createdAt);
    sqlite3_bind_int64(stmt, 13, r.updatedAt);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        lastError_ = sqlite3_errmsg(db_);
    }
    sqlite3_finalize(stmt);
    return ok;
}

static EncryptedFileRecord readRecord(sqlite3_stmt* stmt) {
    EncryptedFileRecord r;
    r.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    r.originalPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    r.vaultPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    r.fileSize = sqlite3_column_int64(stmt, 3);
    r.fileHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

    const auto* salt = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 5));
    const int saltLen = sqlite3_column_bytes(stmt, 5);
    r.salt.assign(salt, salt + saltLen);

    const auto* verifier = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 6));
    const int verifierLen = sqlite3_column_bytes(stmt, 6);
    r.passwordVerifier.assign(verifier, verifier + verifierLen);

    const auto* pwdWrap = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 7));
    const int pwdWrapLen = sqlite3_column_bytes(stmt, 7);
    r.passwordWrappedDek.assign(pwdWrap, pwdWrap + pwdWrapLen);

    const auto* dpapiWrap = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, 8));
    const int dpapiWrapLen = sqlite3_column_bytes(stmt, 8);
    r.dpapiWrappedDek.assign(dpapiWrap, dpapiWrap + dpapiWrapLen);

    r.cipherVersion = sqlite3_column_int(stmt, 9);
    r.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    r.createdAt = sqlite3_column_int64(stmt, 11);
    r.updatedAt = sqlite3_column_int64(stmt, 12);
    return r;
}

std::optional<EncryptedFileRecord> Database::findByPath(const std::string& path) {
    const char* sql = "SELECT id, original_path, vault_path, file_size, file_hash,"
                      " salt, password_verifier, password_wrapped_dek, dpapi_wrapped_dek,"
                      " cipher_version, status, created_at, updated_at"
                      " FROM encrypted_files WHERE original_path = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<EncryptedFileRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRecord(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<EncryptedFileRecord> Database::findById(const std::string& id) {
    const char* sql = "SELECT id, original_path, vault_path, file_size, file_hash,"
                      " salt, password_verifier, password_wrapped_dek, dpapi_wrapped_dek,"
                      " cipher_version, status, created_at, updated_at"
                      " FROM encrypted_files WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<EncryptedFileRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = readRecord(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<EncryptedFileRecord> Database::listActiveRecords() {
    const char* sql = "SELECT id, original_path, vault_path, file_size, file_hash,"
                      " salt, password_verifier, password_wrapped_dek, dpapi_wrapped_dek,"
                      " cipher_version, status, created_at, updated_at"
                      " FROM encrypted_files WHERE status = 'active';";
    sqlite3_stmt* stmt = nullptr;
    std::vector<EncryptedFileRecord> rows;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return rows;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rows.push_back(readRecord(stmt));
    }
    sqlite3_finalize(stmt);
    return rows;
}

bool Database::removeRecord(const std::string& id) {
    const char* sql = "DELETE FROM encrypted_files WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) {
        lastError_ = sqlite3_errmsg(db_);
    }
    sqlite3_finalize(stmt);
    return ok;
}

} // namespace appencrypt
