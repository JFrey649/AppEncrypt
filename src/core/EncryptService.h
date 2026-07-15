#pragma once

#include "core/Database.h"

#include <functional>
#include <string>

namespace appencrypt {

struct ServiceResult {
    bool success = false;
    std::string message;
};

class EncryptService {
public:
    explicit EncryptService(Database& db);

    ServiceResult encryptExe(const std::string& exePath,
                           const std::string& password,
                           const std::string& stubTemplatePath,
                           const std::function<void(int percent)>& progress = {});

    ServiceResult decryptExe(const std::string& exePath,
                             const std::string& password,
                             const std::function<void(int percent)>& progress = {});

    ServiceResult changePassword(const std::string& exePath,
                                 const std::string& oldPassword,
                                 const std::string& newPassword);

    ServiceResult unlockAndLaunch(const std::string& stubPath,
                                  const std::string& password,
                                  const std::vector<std::string>& hostArgs);

    ServiceResult restoreAllForUninstall();

    static std::string installDirectory();
    static std::string databasePath();

private:
    Database& db_;
};

} // namespace appencrypt
