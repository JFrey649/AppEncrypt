#include "core/AppConfig.h"

#include <Windows.h>
#include <ShlObj.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    return s.substr(start);
}

std::string expandEnvVars(const std::string& input) {
    if (input.find('%') == std::string::npos) {
        return input;
    }
    const int size = ExpandEnvironmentStringsA(input.c_str(), nullptr, 0);
    if (size <= 0) {
        return input;
    }
    std::string out(static_cast<size_t>(size), '\0');
    ExpandEnvironmentStringsA(input.c_str(), out.data(), size);
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

std::string readMainExeFromIniFile(const std::string& iniPath) {
    std::ifstream in(iniPath);
    if (!in) {
        return {};
    }
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, pos));
        const std::string val = trim(line.substr(pos + 1));
        if (section == "Paths" && key == "MainExe") {
            return expandEnvVars(val);
        }
    }
    return {};
}

} // namespace

AppConfig& AppConfig::instance() {
    static AppConfig cfg;
    return cfg;
}

std::string AppConfig::detectInstallDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path().string();
}

std::string AppConfig::globalConfigPath() {
    wchar_t programData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programData))) {
        return (fs::path(programData) / "AppEncrypt" / "config.ini").string();
    }
    return (fs::path(detectInstallDirectory()) / "config.ini").string();
}

void AppConfig::applyDefaults() {
    installDir_ = detectInstallDirectory();
    logDir_ = globalConfigPath();
    logDir_ = (fs::path(logDir_).parent_path() / "logs").string();
    configPath_ = (fs::path(installDir_) / "config.ini").string();
}

std::string AppConfig::resolvePath(const std::string& value, const std::string& baseDir) const {
    if (value.empty() || value == "auto") {
        return baseDir;
    }
    const auto expanded = expandEnvVars(value);
    fs::path p(expanded);
    if (p.is_relative()) {
        return (fs::path(baseDir) / p).lexically_normal().string();
    }
    return p.lexically_normal().string();
}

bool AppConfig::load() {
    applyDefaults();

    std::ifstream in(configPath_);
    if (!in) {
        loaded_ = true;
        return save();
    }

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, pos));
        const std::string val = trim(line.substr(pos + 1));

        if (section == "Paths") {
            if (key == "InstallDir" && val != "auto") {
                installDir_ = resolvePath(val, installDir_);
            } else if (key == "Database") {
                databaseRel_ = val;
            } else if (key == "StubTemplate") {
                stubRel_ = val;
            } else if (key == "LogDir") {
                logDir_ = val;
            }
        } else if (section == "Security") {
            if (key == "MaxFailedAttempts") {
                maxFailedAttempts_ = std::stoi(val);
            } else if (key == "LockoutMinutes") {
                lockoutMinutes_ = std::stoi(val);
            }
        }
    }

    logDir_ = resolvePath(logDir_, installDir_);
    loaded_ = true;
    return true;
}

bool AppConfig::save() {
    fs::create_directories(fs::path(configPath_).parent_path());
    fs::create_directories(logDir_);

    std::ofstream out(configPath_, std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "; AppEncrypt 配置文件\n"
        << "; 修改后重启 AppEncrypt 生效\n\n"
        << "[Paths]\n"
        << "InstallDir=auto\n"
        << "MainExe=" << mainExePath() << "\n"
        << "Database=" << databaseRel_ << "\n"
        << "StubTemplate=" << stubRel_ << "\n"
        << "LogDir=%ProgramData%\\AppEncrypt\\logs\n\n"
        << "[Security]\n"
        << "MaxFailedAttempts=" << maxFailedAttempts_ << "\n"
        << "LockoutMinutes=" << lockoutMinutes_ << "\n";

    // 同步一份到 ProgramData，供已加密 Stub 在备用路径查找
    const auto globalPath = globalConfigPath();
    fs::create_directories(fs::path(globalPath).parent_path());
    std::ofstream globalOut(globalPath, std::ios::trunc);
    if (globalOut) {
        globalOut << "[Paths]\nMainExe=" << mainExePath() << "\n";
    }
    return static_cast<bool>(out);
}

std::string AppConfig::installDirectory() const {
    return installDir_;
}

std::string AppConfig::databasePath() const {
    return resolvePath(databaseRel_, installDir_);
}

std::string AppConfig::stubTemplatePath() const {
    return resolvePath(stubRel_, installDir_);
}

std::string AppConfig::mainExePath() const {
    return (fs::path(installDir_) / "AppEncrypt.exe").string();
}

std::string AppConfig::logDirectory() const {
    return logDir_;
}

std::string AppConfig::configFilePath() const {
    return configPath_;
}

std::string AppConfig::readGlobalMainExePath() {
    return readMainExeFromIniFile(globalConfigPath());
}

} // namespace appencrypt
