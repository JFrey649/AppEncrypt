#include <Windows.h>
#include <ShlObj.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[] = "APPENC";

struct StubTailConfig {
    std::string recordId;
    std::string vaultRelativePath;
    std::string launcherExePath;
};

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

std::string readMainExeFromGlobalConfig() {
    wchar_t programData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programData))) {
        return {};
    }
    const auto iniPath = (fs::path(programData) / "AppEncrypt" / "config.ini").string();
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

std::optional<StubTailConfig> readStubTail(const std::string& stubPath) {
    std::ifstream in(stubPath, std::ios::binary | std::ios::ate);
    if (!in) {
        return std::nullopt;
    }
    const auto fileSize = in.tellg();
    if (fileSize < 20) {
        return std::nullopt;
    }
    const auto scanSize = static_cast<std::streamoff>(std::min<int64_t>(fileSize, 4096));
    in.seekg(fileSize - scanSize);
    std::vector<char> buffer(static_cast<size_t>(scanSize));
    in.read(buffer.data(), scanSize);
    if (!in) {
        return std::nullopt;
    }

    for (int i = static_cast<int>(buffer.size()) - 7; i >= 0; --i) {
        if (std::memcmp(buffer.data() + i, kMagic, 6) != 0) {
            continue;
        }
        size_t offset = static_cast<size_t>(i) + 6;
        if (offset >= buffer.size()) {
            continue;
        }
        const uint8_t version = static_cast<uint8_t>(buffer[offset++]);
        if (version != 1 && version != 2) {
            continue;
        }
        if (offset + 8 > buffer.size()) {
            continue;
        }
        uint32_t idLen = 0;
        std::memcpy(&idLen, buffer.data() + offset, 4);
        offset += 4;
        if (offset + idLen + 4 > buffer.size()) {
            continue;
        }
        StubTailConfig cfg;
        cfg.recordId.assign(buffer.data() + offset, idLen);
        offset += idLen;
        uint32_t vaultLen = 0;
        std::memcpy(&vaultLen, buffer.data() + offset, 4);
        offset += 4;
        if (offset + vaultLen > buffer.size()) {
            continue;
        }
        cfg.vaultRelativePath.assign(buffer.data() + offset, vaultLen);
        offset += vaultLen;
        if (version >= 2) {
            if (offset + 4 > buffer.size()) {
                continue;
            }
            uint32_t launcherLen = 0;
            std::memcpy(&launcherLen, buffer.data() + offset, 4);
            offset += 4;
            if (offset + launcherLen > buffer.size()) {
                continue;
            }
            cfg.launcherExePath.assign(buffer.data() + offset, launcherLen);
        }
        return cfg;
    }
    return std::nullopt;
}

std::wstring quoteArg(const std::wstring& arg) {
    return L"\"" + arg + L"\"";
}

std::wstring toWide(const std::string& s) {
    return fs::path(s).wstring();
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(nullptr, selfPath, MAX_PATH);

    const auto tail = readStubTail(fs::path(selfPath).string());
    if (!tail) {
        MessageBoxW(nullptr, L"无效的 AppEncrypt 启动器。", L"AppEncrypt", MB_ICONERROR);
        return 1;
    }

    std::string launcher = tail->launcherExePath;
    if (launcher.empty() || !fs::exists(launcher)) {
        launcher = readMainExeFromGlobalConfig();
    }
    if (launcher.empty() || !fs::exists(launcher)) {
        MessageBoxW(nullptr,
                    L"找不到 AppEncrypt 主程序。\n请确认 config.ini 中 MainExe 路径正确，或重新加密该程序。",
                    L"AppEncrypt",
                    MB_ICONERROR);
        return 1;
    }

    std::wstring cmd = quoteArg(toWide(launcher));
    cmd += L" unlock ";
    cmd += quoteArg(selfPath);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            cmd += L" ";
            cmd += quoteArg(argv[i]);
        }
        LocalFree(argv);
    }

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        MessageBoxW(nullptr, L"无法启动 AppEncrypt 主程序。", L"AppEncrypt", MB_ICONERROR);
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}
