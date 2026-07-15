#include "ui/ProgressDialog.h"

#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>

namespace appencrypt {

ProgressDialog::ProgressDialog(const QString& title, QWidget* parent) : QDialog(parent) {
    setWindowTitle(title);
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowCloseButtonHint, false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    statusLabel_ = new QLabel(QStringLiteral("处理中..."), this);
    statusLabel_->setWordWrap(true);

    bar_ = new QProgressBar(this);
    bar_->setRange(0, 100);
    bar_->setTextVisible(true);
    bar_->setMinimumWidth(320);

    layout->addWidget(statusLabel_);
    layout->addWidget(bar_);

    setMinimumWidth(380);
    adjustSize();
}

void ProgressDialog::setProgress(int percent) {
    bar_->setValue(percent);
}

void ProgressDialog::setStatusText(const QString& text) {
    statusLabel_->setText(text);
}

} // namespace appencrypt
