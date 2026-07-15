#pragma once

#include <string>

namespace appencrypt {

struct ShellRegistrationResult {
    bool success = false;
    std::string message;
};

class ShellRegistration {
public:
    /** per-user: HKCU\Software\Classes\exefile\shell（无需管理员） */
    static ShellRegistrationResult registerContextMenu(const std::string& mainExePath, bool systemWide = false);

    static ShellRegistrationResult unregisterContextMenu(bool systemWide = false);

    static bool isRegistered(bool systemWide = false);
};

} // namespace appencrypt
