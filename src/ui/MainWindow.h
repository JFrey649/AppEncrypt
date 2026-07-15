#pragma once

#include <QMainWindow>

class QPushButton;
class QLabel;

namespace appencrypt {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void setupUi();
    void runEncrypt(const QString& path);
    void runDecrypt(const QString& path);
    void runChangePassword(const QString& path);
    void runUninstallRestore();

    void runRegisterShell(bool systemWide);
    void runUnregisterShell(bool systemWide);
    void refreshShellStatus();

    QLabel* hintLabel_ = nullptr;
    QLabel* shellStatusLabel_ = nullptr;
    QPushButton* encryptBtn_ = nullptr;
    QPushButton* decryptBtn_ = nullptr;
    QPushButton* changePwBtn_ = nullptr;
    QPushButton* restoreBtn_ = nullptr;
    QPushButton* registerShellBtn_ = nullptr;
    QPushButton* unregisterShellBtn_ = nullptr;
};

} // namespace appencrypt
