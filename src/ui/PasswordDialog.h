#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;

namespace appencrypt {

enum class PasswordDialogMode {
    Encrypt,
    Verify,
    ChangePassword,
};

class PasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit PasswordDialog(PasswordDialogMode mode, QWidget* parent = nullptr);

    QString password() const;
    QString confirmPassword() const;
    QString oldPassword() const;

private:
    PasswordDialogMode mode_;
    QLineEdit* passwordEdit_ = nullptr;
    QLineEdit* confirmEdit_ = nullptr;
    QLineEdit* oldPasswordEdit_ = nullptr;
    QLabel* errorLabel_ = nullptr;

    void accept() override;
};

} // namespace appencrypt
