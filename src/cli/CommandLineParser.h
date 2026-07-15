#pragma once

#include <string>
#include <vector>

namespace appencrypt {

struct ParsedCommand {
    enum class Action {
        None,
        Gui,
        Encrypt,
        Decrypt,
        ChangePassword,
        Unlock,
        UninstallRestore,
        RegisterShell,
        UnregisterShell,
        Help,
    };

    Action action = Action::None;
    std::string targetPath;
    std::vector<std::string> hostArgs;
    bool systemWideShell = false;
    bool quiet = false;
};

class CommandLineParser {
public:
    static ParsedCommand parse(int argc, char* argv[]);
    static std::string helpText();
};

} // namespace appencrypt
