#include "ui/PasswordDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace appencrypt {

namespace {

void styleLineEdit(QLineEdit* edit) {
    edit->setClearButtonEnabled(true);
    edit->setMinimumWidth(260);
}

void applyDialogLayout(QVBoxLayout* root) {
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);
}

void applyFormLayout(QFormLayout* form) {
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);
}

} // namespace

PasswordDialog::PasswordDialog(PasswordDialogMode mode, QWidget* parent)
    : QDialog(parent), mode_(mode) {
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto* root = new QVBoxLayout(this);
    applyDialogLayout(root);

    auto* form = new QFormLayout();
    applyFormLayout(form);

    errorLabel_ = new QLabel(this);
    errorLabel_->setWordWrap(true);
    errorLabel_->setStyleSheet(QStringLiteral("color: #e74c3c; min-height: 18px;"));
    errorLabel_->setMinimumHeight(20);

    switch (mode_) {
    case PasswordDialogMode::Encrypt:
        setWindowTitle(QStringLiteral("加密程序"));
        passwordEdit_ = new QLineEdit(this);
        passwordEdit_->setEchoMode(QLineEdit::Password);
        passwordEdit_->setPlaceholderText(QStringLiteral("至少 6 位"));
        styleLineEdit(passwordEdit_);

        confirmEdit_ = new QLineEdit(this);
        confirmEdit_->setEchoMode(QLineEdit::Password);
        confirmEdit_->setPlaceholderText(QStringLiteral("再次输入密码"));
        styleLineEdit(confirmEdit_);

        form->addRow(QStringLiteral("密码："), passwordEdit_);
        form->addRow(QStringLiteral("确认密码："), confirmEdit_);
        setMinimumWidth(400);
        break;

    case PasswordDialogMode::Verify:
        setWindowTitle(QStringLiteral("输入密码"));
        passwordEdit_ = new QLineEdit(this);
        passwordEdit_->setEchoMode(QLineEdit::Password);
        styleLineEdit(passwordEdit_);

        form->addRow(QStringLiteral("密码："), passwordEdit_);
        setMinimumWidth(380);
        break;

    case PasswordDialogMode::ChangePassword:
        setWindowTitle(QStringLiteral("更改密码"));
        oldPasswordEdit_ = new QLineEdit(this);
        oldPasswordEdit_->setEchoMode(QLineEdit::Password);
        styleLineEdit(oldPasswordEdit_);

        passwordEdit_ = new QLineEdit(this);
        passwordEdit_->setEchoMode(QLineEdit::Password);
        passwordEdit_->setPlaceholderText(QStringLiteral("至少 6 位"));
        styleLineEdit(passwordEdit_);

        confirmEdit_ = new QLineEdit(this);
        confirmEdit_->setEchoMode(QLineEdit::Password);
        confirmEdit_->setPlaceholderText(QStringLiteral("再次输入新密码"));
        styleLineEdit(confirmEdit_);

        form->addRow(QStringLiteral("旧密码："), oldPasswordEdit_);
        form->addRow(QStringLiteral("新密码："), passwordEdit_);
        form->addRow(QStringLiteral("确认新密码："), confirmEdit_);
        setMinimumWidth(420);
        break;
    }

    root->addLayout(form);
    root->addWidget(errorLabel_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &PasswordDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (passwordEdit_) {
        passwordEdit_->setFocus();
    } else if (oldPasswordEdit_) {
        oldPasswordEdit_->setFocus();
    }

    adjustSize();
}

QString PasswordDialog::password() const {
    return passwordEdit_ ? passwordEdit_->text() : QString();
}

QString PasswordDialog::confirmPassword() const {
    return confirmEdit_ ? confirmEdit_->text() : QString();
}

QString PasswordDialog::oldPassword() const {
    return oldPasswordEdit_ ? oldPasswordEdit_->text() : QString();
}

void PasswordDialog::accept() {
    errorLabel_->clear();
    const auto pwd = password();
    if (pwd.size() < 6) {
        errorLabel_->setText(QStringLiteral("密码长度至少 6 位"));
        return;
    }
    if (mode_ == PasswordDialogMode::Encrypt || mode_ == PasswordDialogMode::ChangePassword) {
        if (pwd != confirmPassword()) {
            errorLabel_->setText(QStringLiteral("两次输入的密码不一致"));
            return;
        }
    }
    QDialog::accept();
}

} // namespace appencrypt
