#include "cli/CommandLineParser.h"
#include "core/AppConfig.h"
#include "core/Database.h"
#include "core/EncryptService.h"
#include "core/ShellRegistration.h"
#include "ui/MainWindow.h"
#include "ui/PasswordDialog.h"
#include "ui/ProgressDialog.h"

#include <QApplication>
#include <QMessageBox>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static bool initDatabase(appencrypt::Database& db) {
    if (!db.open()) {
        return false;
    }
    return db.initializeSchema();
}

static int runUninstallRestore(const appencrypt::ParsedCommand& cmd) {
    const std::string dbPath = appencrypt::EncryptService::databasePath();
    appencrypt::Database db(dbPath);
    if (!initDatabase(db)) {
        if (!cmd.quiet) {
            QMessageBox::critical(nullptr, QStringLiteral("AppEncrypt"),
                                  QStringLiteral("无法打开数据库：%1")
                                      .arg(QString::fromStdString(db.lastError())));
        } else {
            std::cerr << "无法打开数据库: " << db.lastError() << std::endl;
        }
        return 1;
    }
    appencrypt::EncryptService service(db);
    const auto result = service.restoreAllForUninstall();
    if (result.success) {
        if (!cmd.quiet) {
            QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                   QString::fromStdString(result.message));
        }
        return 0;
    }
    if (!cmd.quiet) {
        QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    } else {
        std::cerr << result.message << std::endl;
    }
    return 1;
}

static int runWithGui(const appencrypt::ParsedCommand& cmd) {
    const std::string dbPath = appencrypt::EncryptService::databasePath();
    appencrypt::Database db(dbPath);
    if (!initDatabase(db)) {
        QMessageBox::critical(nullptr, QStringLiteral("AppEncrypt"),
                              QStringLiteral("无法打开数据库：%1").arg(QString::fromStdString(db.lastError())));
        return 1;
    }

    appencrypt::EncryptService service(db);
    const fs::path target = fs::path(cmd.targetPath);

    switch (cmd.action) {
    case appencrypt::ParsedCommand::Action::Encrypt: {
        appencrypt::PasswordDialog dlg(appencrypt::PasswordDialogMode::Encrypt);
        if (dlg.exec() != QDialog::Accepted) {
            return 0;
        }
        appencrypt::ProgressDialog progress(QStringLiteral("正在加密"));
        progress.show();
        QApplication::processEvents();

        const auto result = service.encryptExe(
            target.string(),
            dlg.password().toStdString(),
            appencrypt::AppConfig::instance().stubTemplatePath(),
            [&](int p) {
                progress.setProgress(p);
                QApplication::processEvents();
            });
        progress.close();
        if (result.success) {
            QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                   QString::fromStdString(result.message));
            return 0;
        }
        QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        return 1;
    }
    case appencrypt::ParsedCommand::Action::Decrypt: {
        appencrypt::PasswordDialog dlg(appencrypt::PasswordDialogMode::Verify);
        if (dlg.exec() != QDialog::Accepted) {
            return 0;
        }
        const auto result = service.decryptExe(target.string(), dlg.password().toStdString());
        if (result.success) {
            QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                   QString::fromStdString(result.message));
            return 0;
        }
        QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        return 1;
    }
    case appencrypt::ParsedCommand::Action::ChangePassword: {
        appencrypt::PasswordDialog dlg(appencrypt::PasswordDialogMode::ChangePassword);
        if (dlg.exec() != QDialog::Accepted) {
            return 0;
        }
        const auto result = service.changePassword(
            target.string(),
            dlg.oldPassword().toStdString(),
            dlg.password().toStdString());
        if (result.success) {
            QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                   QString::fromStdString(result.message));
            return 0;
        }
        QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        return 1;
    }
    case appencrypt::ParsedCommand::Action::Unlock: {
        appencrypt::PasswordDialog dlg(appencrypt::PasswordDialogMode::Verify);
        if (dlg.exec() != QDialog::Accepted) {
            return 1;
        }
        std::vector<std::string> hostArgs = cmd.hostArgs;
        const auto result =
            service.unlockAndLaunch(target.string(), dlg.password().toStdString(), hostArgs);
        if (result.success) {
            return 0;
        }
        QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        return 1;
    }
    default:
        break;
    }
    return 1;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AppEncrypt"));

    app.setQuitOnLastWindowClosed(false);

    appencrypt::AppConfig::instance().load();

    const auto cmd = appencrypt::CommandLineParser::parse(argc, argv);
    if (cmd.action == appencrypt::ParsedCommand::Action::Help) {
        if (!cmd.quiet) {
            QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                     QString::fromStdString(appencrypt::CommandLineParser::helpText()));
        } else {
            std::cout << appencrypt::CommandLineParser::helpText();
        }
        return 0;
    }
    if (cmd.action == appencrypt::ParsedCommand::Action::Gui) {
        appencrypt::MainWindow window;
        window.show();
        return app.exec();
    }
    if (cmd.action == appencrypt::ParsedCommand::Action::RegisterShell) {
        const auto result = appencrypt::ShellRegistration::registerContextMenu(
            appencrypt::AppConfig::instance().mainExePath(), cmd.systemWideShell);
        if (result.success) {
            if (!cmd.quiet) {
                QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                       QString::fromStdString(result.message));
            }
            return 0;
        }
        if (!cmd.quiet) {
            const QString hint = cmd.systemWideShell
                ? QStringLiteral("%1\n\n请以管理员身份运行。")
                : QStringLiteral("%1");
            QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"),
                                 hint.arg(QString::fromStdString(result.message)));
        } else {
            std::cerr << result.message << std::endl;
        }
        return 1;
    }
    if (cmd.action == appencrypt::ParsedCommand::Action::UnregisterShell) {
        const auto result = appencrypt::ShellRegistration::unregisterContextMenu(cmd.systemWideShell);
        if (result.success) {
            if (!cmd.quiet) {
                QMessageBox::information(nullptr, QStringLiteral("AppEncrypt"),
                                       QString::fromStdString(result.message));
            }
            return 0;
        }
        if (!cmd.quiet) {
            QMessageBox::warning(nullptr, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        } else {
            std::cerr << result.message << std::endl;
        }
        return 1;
    }
    if (cmd.action == appencrypt::ParsedCommand::Action::UninstallRestore) {
        return runUninstallRestore(cmd);
    }

    return runWithGui(cmd);
}
