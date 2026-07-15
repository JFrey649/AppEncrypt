#pragma once

#include <string>

namespace appencrypt {

class AppConfig {
public:
    static AppConfig& instance();

    bool load();
    bool save();

    std::string installDirectory() const;
    std::string databasePath() const;
    std::string stubTemplatePath() const;
    std::string mainExePath() const;
    std::string logDirectory() const;
    std::string configFilePath() const;

    int maxFailedAttempts() const { return maxFailedAttempts_; }
    int lockoutMinutes() const { return lockoutMinutes_; }

    /** 全局备用配置路径（供 Stub 在旧版尾部块无 launcher 路径时读取） */
    static std::string globalConfigPath();
    static std::string readGlobalMainExePath();

private:
    AppConfig() = default;
    void applyDefaults();
    std::string resolvePath(const std::string& value, const std::string& baseDir) const;
    static std::string detectInstallDirectory();

    std::string configPath_;
    std::string installDir_;
    std::string databaseRel_ = "appencrypt.db";
    std::string stubRel_ = "AppEncryptStub.exe";
    std::string logDir_;
    int maxFailedAttempts_ = 5;
    int lockoutMinutes_ = 5;
    bool loaded_ = false;
};

} // namespace appencrypt
