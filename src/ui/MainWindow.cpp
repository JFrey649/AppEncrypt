#include "ui/MainWindow.h"
#include "core/AppConfig.h"
#include "core/Database.h"
#include "core/EncryptService.h"
#include "core/ShellRegistration.h"
#include "ui/PasswordDialog.h"
#include "ui/ProgressDialog.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>

namespace fs = std::filesystem;

namespace appencrypt {

namespace {

static bool initDatabase(Database& db) {
    if (!db.open()) {
        return false;
    }
    return db.initializeSchema();
}

static QString pickExeFile(QWidget* parent, const QString& title) {
    return QFileDialog::getOpenFileName(parent, title, QString(), QStringLiteral("可执行文件 (*.exe)"));
}

static bool isExePath(const QString& path) {
    return path.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) && QFileInfo::exists(path);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("AppEncrypt"));
    setAcceptDrops(true);
    setupUi();
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    hintLabel_ = new QLabel(
        QStringLiteral("选择 exe 文件进行加密/解密，或将 exe 拖拽到本窗口。\n"
                       "配置：安装目录 config.ini；右键菜单需注册（见下方）"),
        central);
    hintLabel_->setWordWrap(true);
    layout->addWidget(hintLabel_);

    shellStatusLabel_ = new QLabel(central);
    shellStatusLabel_->setWordWrap(true);
    layout->addWidget(shellStatusLabel_);

    auto* row1 = new QHBoxLayout();
    row1->setSpacing(8);
    encryptBtn_ = new QPushButton(QStringLiteral("选择并加密"), central);
    decryptBtn_ = new QPushButton(QStringLiteral("选择并解密"), central);
    encryptBtn_->setMinimumHeight(32);
    decryptBtn_->setMinimumHeight(32);
    row1->addWidget(encryptBtn_);
    row1->addWidget(decryptBtn_);
    layout->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    row2->setSpacing(8);
    changePwBtn_ = new QPushButton(QStringLiteral("更改密码"), central);
    restoreBtn_ = new QPushButton(QStringLiteral("卸载还原（全部解密）"), central);
    changePwBtn_->setMinimumHeight(32);
    restoreBtn_->setMinimumHeight(32);
    row2->addWidget(changePwBtn_);
    row2->addWidget(restoreBtn_);
    layout->addLayout(row2);

    auto* row3 = new QHBoxLayout();
    row3->setSpacing(8);
    registerShellBtn_ = new QPushButton(QStringLiteral("注册右键菜单（当前用户）"), central);
    unregisterShellBtn_ = new QPushButton(QStringLiteral("移除右键菜单"), central);
    registerShellBtn_->setMinimumHeight(32);
    unregisterShellBtn_->setMinimumHeight(32);
    row3->addWidget(registerShellBtn_);
    row3->addWidget(unregisterShellBtn_);
    layout->addLayout(row3);

    layout->addStretch();

    setCentralWidget(central);
    setMinimumSize(520, 300);
    refreshShellStatus();

    connect(encryptBtn_, &QPushButton::clicked, this, [this]() {
        const auto path = pickExeFile(this, QStringLiteral("选择要加密的 exe"));
        if (!path.isEmpty()) {
            runEncrypt(path);
        }
    });
    connect(decryptBtn_, &QPushButton::clicked, this, [this]() {
        const auto path = pickExeFile(this, QStringLiteral("选择要解密的 exe"));
        if (!path.isEmpty()) {
            runDecrypt(path);
        }
    });
    connect(changePwBtn_, &QPushButton::clicked, this, [this]() {
        const auto path = pickExeFile(this, QStringLiteral("选择已加密的 exe"));
        if (!path.isEmpty()) {
            runChangePassword(path);
        }
    });
    connect(restoreBtn_, &QPushButton::clicked, this, [this] { runUninstallRestore(); });
    connect(registerShellBtn_, &QPushButton::clicked, this, [this] { runRegisterShell(false); });
    connect(unregisterShellBtn_, &QPushButton::clicked, this, [this] { runUnregisterShell(false); });
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }
    const QString path = urls.first().toLocalFile();
    if (!isExePath(path)) {
        QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QStringLiteral("请拖拽 .exe 文件"));
        return;
    }

    QMessageBox choice(this);
    choice.setWindowTitle(QStringLiteral("AppEncrypt"));
    choice.setText(QStringLiteral("对 %1 执行什么操作？").arg(path));
    auto* encBtn = choice.addButton(QStringLiteral("加密"), QMessageBox::AcceptRole);
    auto* decBtn = choice.addButton(QStringLiteral("解密"), QMessageBox::ActionRole);
    choice.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    choice.exec();
    if (choice.clickedButton() == encBtn) {
        runEncrypt(path);
    } else if (choice.clickedButton() == decBtn) {
        runDecrypt(path);
    }
    event->acceptProposedAction();
}

void MainWindow::runEncrypt(const QString& path) {
    PasswordDialog dlg(PasswordDialogMode::Encrypt, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    Database db(EncryptService::databasePath());
    if (!initDatabase(db)) {
        QMessageBox::critical(this, QStringLiteral("AppEncrypt"),
                              QStringLiteral("无法打开数据库：%1").arg(QString::fromStdString(db.lastError())));
        return;
    }

    ProgressDialog progress(QStringLiteral("正在加密"), this);
    progress.show();
    QApplication::processEvents();

    EncryptService service(db);
    const auto result = service.encryptExe(
        path.toStdString(),
        dlg.password().toStdString(),
        AppConfig::instance().stubTemplatePath(),
        [&](int p) {
            progress.setProgress(p);
            QApplication::processEvents();
        });
    progress.close();

    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    } else {
        QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    }
}

void MainWindow::runDecrypt(const QString& path) {
    PasswordDialog dlg(PasswordDialogMode::Verify, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    Database db(EncryptService::databasePath());
    if (!initDatabase(db)) {
        QMessageBox::critical(this, QStringLiteral("AppEncrypt"),
                              QStringLiteral("无法打开数据库：%1").arg(QString::fromStdString(db.lastError())));
        return;
    }

    EncryptService service(db);
    const auto result = service.decryptExe(path.toStdString(), dlg.password().toStdString());
    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    } else {
        QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    }
}

void MainWindow::runChangePassword(const QString& path) {
    PasswordDialog dlg(PasswordDialogMode::ChangePassword, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    Database db(EncryptService::databasePath());
    if (!initDatabase(db)) {
        QMessageBox::critical(this, QStringLiteral("AppEncrypt"),
                              QStringLiteral("无法打开数据库：%1").arg(QString::fromStdString(db.lastError())));
        return;
    }

    EncryptService service(db);
    const auto result = service.changePassword(
        path.toStdString(),
        dlg.oldPassword().toStdString(),
        dlg.password().toStdString());
    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    } else {
        QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    }
}

void MainWindow::runUninstallRestore() {
    const auto ret = QMessageBox::question(
        this,
        QStringLiteral("AppEncrypt"),
        QStringLiteral("将还原所有已加密程序并移除保护，是否继续？"));
    if (ret != QMessageBox::Yes) {
        return;
    }

    Database db(EncryptService::databasePath());
    if (!initDatabase(db)) {
        QMessageBox::critical(this, QStringLiteral("AppEncrypt"),
                              QStringLiteral("无法打开数据库：%1").arg(QString::fromStdString(db.lastError())));
        return;
    }

    EncryptService service(db);
    const auto result = service.restoreAllForUninstall();
    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    } else {
        QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
    }
}

void MainWindow::refreshShellStatus() {
    const bool userReg = ShellRegistration::isRegistered(false);
    const bool sysReg = ShellRegistration::isRegistered(true);
    QString status = QStringLiteral("右键菜单：");
    if (userReg) {
        status += QStringLiteral("当前用户已注册");
    } else {
        status += QStringLiteral("当前用户未注册");
    }
    if (sysReg) {
        status += QStringLiteral("；系统级已注册");
    }
    shellStatusLabel_->setText(status);
}

void MainWindow::runRegisterShell(bool systemWide) {
    const auto result =
        ShellRegistration::registerContextMenu(AppConfig::instance().mainExePath(), systemWide);
    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        refreshShellStatus();
        return;
    }
    QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
}

void MainWindow::runUnregisterShell(bool systemWide) {
    const auto result = ShellRegistration::unregisterContextMenu(systemWide);
    if (result.success) {
        QMessageBox::information(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
        refreshShellStatus();
        return;
    }
    QMessageBox::warning(this, QStringLiteral("AppEncrypt"), QString::fromStdString(result.message));
}

} // namespace appencrypt
