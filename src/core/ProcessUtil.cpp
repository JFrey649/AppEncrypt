#include "core/ProcessUtil.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <shellapi.h>

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

std::wstring quoteArg(const std::wstring& arg) {
    return L"\"" + arg + L"\"";
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::string describeWin32Error(DWORD code) {
    switch (code) {
    case ERROR_FILE_NOT_FOUND:
        return "找不到指定的文件";
    case ERROR_PATH_NOT_FOUND:
        return "找不到指定的路径";
    case ERROR_ACCESS_DENIED:
        return "访问被拒绝，请检查权限或安全软件拦截";
    case ERROR_ELEVATION_REQUIRED:
        return "该程序需要管理员权限才能运行";
    case ERROR_BAD_EXE_FORMAT:
        return "不是有效的 Windows 可执行文件（可能架构不匹配）";
    default:
        break;
    }

    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (len == 0 || !buffer) {
        return "未知错误";
    }
    std::wstring msg(buffer, len);
    LocalFree(buffer);
    while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' ')) {
        msg.pop_back();
    }
    const auto utf8 = wideToUtf8(msg);
    return utf8.empty() ? "未知错误" : utf8;
}

fs::path pathFromUtf8(const std::string& text) {
    return fs::u8path(text);
}

bool launchWithShellExecute(const fs::path& hostPath,
                            const std::vector<std::string>& args,
                            const fs::path& workDir,
                            bool asAdmin,
                            DWORD& exitCode) {
    std::wstring params;
    for (const auto& arg : args) {
        if (!params.empty()) {
            params += L' ';
        }
        params += quoteArg(pathFromUtf8(arg).wstring());
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.hwnd = nullptr;
    sei.lpVerb = asAdmin ? L"runas" : L"open";
    sei.lpFile = hostPath.wstring().c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.lpDirectory = workDir.wstring().c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        return false;
    }
    if (!sei.hProcess) {
        exitCode = 0;
        return true;
    }
    WaitForSingleObject(sei.hProcess, INFINITE);
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);
    return true;
}

} // namespace

bool ProcessUtil::isExeRunning(const std::string& exePath) {
    const fs::path target = fs::absolute(pathFromUtf8(exePath));
    const std::wstring targetName = target.filename().wstring();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool running = false;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, targetName.c_str()) != 0) {
                continue;
            }
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (!proc) {
                continue;
            }
            wchar_t pathBuf[MAX_PATH];
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(proc, 0, pathBuf, &size)) {
                if (fs::equivalent(fs::path(pathBuf), target)) {
                    running = true;
                }
            }
            CloseHandle(proc);
            if (running) {
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return running;
}

LaunchResult ProcessUtil::launchAndWait(const fs::path& exePath,
                                        const std::vector<std::string>& args,
                                        const fs::path& workingDirectory) {
    LaunchResult result;
    const fs::path hostPath = fs::absolute(exePath);
    if (!fs::exists(hostPath)) {
        result.message = "找不到宿主程序: " + wideToUtf8(hostPath.wstring());
        return result;
    }

    std::wstring cmdLine = quoteArg(hostPath.wstring());
    for (const auto& arg : args) {
        cmdLine += L" ";
        cmdLine += quoteArg(pathFromUtf8(arg).wstring());
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');

    const fs::path workDir = fs::absolute(workingDirectory);
    if (!CreateProcessW(nullptr,
                        mutableCmd.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        workDir.wstring().c_str(),
                        &si,
                        &pi)) {
        const DWORD err = GetLastError();
        if (err == ERROR_ELEVATION_REQUIRED &&
            launchWithShellExecute(hostPath, args, workDir, true, result.exitCode)) {
            result.success = true;
            result.message = "已启动";
            return result;
        }
        result.message = "启动宿主程序失败 (错误码 " + std::to_string(err) + "): " + describeWin32Error(err);
        return result;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &result.exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    result.success = true;
    result.message = "已启动";
    return result;
}

} // namespace appencrypt
