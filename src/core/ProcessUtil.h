#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace appencrypt {

struct LaunchResult {
    bool success = false;
    std::string message;
    DWORD exitCode = 1;
};

class ProcessUtil {
public:
    /** 检查指定 exe 路径对应进程是否正在运行 */
    static bool isExeRunning(const std::string& exePath);

    /** 启动 exe 并等待其退出；exe 所在目录用于 DLL 搜索，workingDirectory 用于 CWD */
    static LaunchResult launchAndWait(const std::filesystem::path& exePath,
                                      const std::vector<std::string>& args,
                                      const std::filesystem::path& workingDirectory);
};

} // namespace appencrypt
