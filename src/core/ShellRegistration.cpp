#include "core/ShellRegistration.h"

#include <Windows.h>
#include <ShlObj.h>

#include <filesystem>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

constexpr wchar_t kMenuEncrypt[] = L"AppEncrypt.Encrypt";
constexpr wchar_t kMenuDecrypt[] = L"AppEncrypt.Decrypt";
constexpr wchar_t kMenuChangePw[] = L"AppEncrypt.ChangePassword";

struct MenuDef {
    const wchar_t* keyName;
    const wchar_t* label;
    const wchar_t* verb;
};

const MenuDef kMenus[] = {
    {kMenuEncrypt, L"使用 AppEncrypt 加密", L"encrypt"},
    {kMenuDecrypt, L"使用 AppEncrypt 解密", L"decrypt"},
    {kMenuChangePw, L"更改 AppEncrypt 密码", L"changepw"},
};

std::wstring toWide(const std::string& s) {
    return fs::path(s).wstring();
}

std::wstring shellRoot(bool systemWide) {
    if (systemWide) {
        return L"Software\\Classes\\exefile\\shell";
    }
    return L"Software\\Classes\\exefile\\shell";
}

HKEY rootKey(bool systemWide) {
    return systemWide ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

bool setString(HKEY parent, const wchar_t* subKey, const wchar_t* valueName, const std::wstring& data) {
    HKEY key = nullptr;
    const auto status = RegCreateKeyExW(
        parent, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    const auto setStatus = RegSetValueExW(
        key,
        valueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(data.c_str()),
        static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return setStatus == ERROR_SUCCESS;
}

bool deleteTree(HKEY root, const std::wstring& subKey) {
    const auto status = RegDeleteTreeW(root, subKey.c_str());
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

std::wstring buildCommand(const std::wstring& exePath, const wchar_t* verb) {
    std::wstring cmd = L"\"";
    cmd += exePath;
    cmd += L"\" ";
    cmd += verb;
    cmd += L" \"%1\"";
    return cmd;
}

ShellRegistrationResult registerOne(HKEY root, const std::wstring& basePath, const MenuDef& menu,
                                  const std::wstring& exePath, const std::wstring& icon) {
    ShellRegistrationResult result;
    const std::wstring menuPath = basePath + L"\\" + menu.keyName;

    if (!setString(root, menuPath.c_str(), nullptr, menu.label)) {
        result.message = "无法创建菜单项";
        return result;
    }
    if (!setString(root, (menuPath + L"\\command").c_str(), nullptr, buildCommand(exePath, menu.verb))) {
        result.message = "无法创建菜单命令";
        return result;
    }

    HKEY menuKey = nullptr;
    if (RegOpenKeyExW(root, menuPath.c_str(), 0, KEY_WRITE, &menuKey) == ERROR_SUCCESS) {
        const std::wstring single = L"Single";
        RegSetValueExW(menuKey,
                       L"MultiSelectModel",
                       0,
                       REG_SZ,
                       reinterpret_cast<const BYTE*>(single.c_str()),
                       static_cast<DWORD>((single.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(menuKey,
                       L"Icon",
                       0,
                       REG_SZ,
                       reinterpret_cast<const BYTE*>(icon.c_str()),
                       static_cast<DWORD>((icon.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(menuKey);
    }

    result.success = true;
    return result;
}

} // namespace

ShellRegistrationResult ShellRegistration::registerContextMenu(const std::string& mainExePath, bool systemWide) {
    ShellRegistrationResult result;
    if (!fs::exists(mainExePath)) {
        result.message = "找不到主程序: " + mainExePath;
        return result;
    }

    const std::wstring exePath = toWide(fs::absolute(mainExePath).string());
    const std::wstring icon = exePath + L",0";
    const std::wstring basePath = shellRoot(systemWide);
    HKEY root = rootKey(systemWide);

    for (const auto& menu : kMenus) {
        const auto one = registerOne(root, basePath, menu, exePath, icon);
        if (!one.success) {
            result.message = one.message;
            unregisterContextMenu(systemWide);
            return result;
        }
    }

    result.success = true;
    result.message = systemWide ? "已注册系统级 exe 右键菜单（所有用户）"
                                : "已注册当前用户 exe 右键菜单";
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return result;
}

ShellRegistrationResult ShellRegistration::unregisterContextMenu(bool systemWide) {
    ShellRegistrationResult result;
    HKEY root = rootKey(systemWide);
    const std::wstring basePath = shellRoot(systemWide);

    for (const auto& menu : kMenus) {
        const std::wstring menuPath = basePath + L"\\" + menu.keyName;
        deleteTree(root, menuPath);
    }

    result.success = true;
    result.message = systemWide ? "已移除系统级右键菜单" : "已移除当前用户右键菜单";
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return result;
}

bool ShellRegistration::isRegistered(bool systemWide) {
    HKEY root = rootKey(systemWide);
    const std::wstring keyPath = shellRoot(systemWide) + L"\\" + kMenuEncrypt;

    HKEY key = nullptr;
    const auto status = RegOpenKeyExW(root, keyPath.c_str(), 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    RegCloseKey(key);
    return true;
}

} // namespace appencrypt
