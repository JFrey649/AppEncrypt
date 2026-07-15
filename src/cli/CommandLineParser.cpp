#include "cli/CommandLineParser.h"

#include <algorithm>
#include <cctype>

namespace appencrypt {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool looksLikeExePath(const std::string& path) {
    if (path.size() < 5) {
        return false;
    }
    const auto lower = toLower(path);
    return lower.rfind(".exe") == lower.size() - 4;
}

void parseCommonFlags(int argc, char* argv[], ParsedCommand& cmd, int startIndex = 2) {
    for (int i = startIndex; i < argc; ++i) {
        const auto arg = toLower(argv[i]);
        if (arg == "--system") {
            cmd.systemWideShell = true;
        } else if (arg == "--quiet" || arg == "-q") {
            cmd.quiet = true;
        }
    }
}

} // namespace

std::string CommandLineParser::helpText() {
    return
        "AppEncrypt - Windows exe encryption tool\n\n"
        "Usage:\n"
        "  AppEncrypt.exe encrypt   <exe_path>\n"
        "  AppEncrypt.exe decrypt   <exe_path>\n"
        "  AppEncrypt.exe changepw  <exe_path>\n"
        "  AppEncrypt.exe unlock    <stub_path> [host_args...]\n"
        "  AppEncrypt.exe uninstall-restore\n"
        "  AppEncrypt.exe register-shell   [--system] [--quiet]\n"
        "  AppEncrypt.exe unregister-shell [--system] [--quiet]\n"
        "  AppEncrypt.exe                 (open GUI)\n"
        "  AppEncrypt.exe <file.exe>      (encrypt via SendTo)\n";
}

ParsedCommand CommandLineParser::parse(int argc, char* argv[]) {
    ParsedCommand cmd;
    if (argc < 2) {
        cmd.action = ParsedCommand::Action::Gui;
        return cmd;
    }

    const std::string verb = toLower(argv[1]);

    if (verb == "register-shell") {
        cmd.action = ParsedCommand::Action::RegisterShell;
        parseCommonFlags(argc, argv, cmd);
        return cmd;
    }
    if (verb == "unregister-shell") {
        cmd.action = ParsedCommand::Action::UnregisterShell;
        parseCommonFlags(argc, argv, cmd);
        return cmd;
    }
    if (verb == "uninstall-restore") {
        cmd.action = ParsedCommand::Action::UninstallRestore;
        parseCommonFlags(argc, argv, cmd, 2);
        return cmd;
    }
    if (verb == "-h" || verb == "--help") {
        cmd.action = ParsedCommand::Action::Help;
        return cmd;
    }

    if (argc == 2) {
        if (looksLikeExePath(argv[1])) {
            cmd.action = ParsedCommand::Action::Encrypt;
            cmd.targetPath = argv[1];
            return cmd;
        }
        cmd.action = ParsedCommand::Action::Help;
        return cmd;
    }

    if (verb == "encrypt") {
        cmd.action = ParsedCommand::Action::Encrypt;
    } else if (verb == "decrypt") {
        cmd.action = ParsedCommand::Action::Decrypt;
    } else if (verb == "changepw") {
        cmd.action = ParsedCommand::Action::ChangePassword;
    } else if (verb == "unlock") {
        cmd.action = ParsedCommand::Action::Unlock;
    } else {
        cmd.action = ParsedCommand::Action::Help;
        return cmd;
    }

    if (argc < 3) {
        cmd.action = ParsedCommand::Action::Help;
        return cmd;
    }

    cmd.targetPath = argv[2];
    if (cmd.action == ParsedCommand::Action::Unlock && argc > 3) {
        cmd.hostArgs.assign(argv + 3, argv + argc);
    }
    return cmd;
}

} // namespace appencrypt
