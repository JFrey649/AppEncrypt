#pragma once

#include <QDialog>

class QLabel;
class QProgressBar;

namespace appencrypt {

class ProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProgressDialog(const QString& title, QWidget* parent = nullptr);
    void setProgress(int percent);
    void setStatusText(const QString& text);

private:
    QLabel* statusLabel_ = nullptr;
    QProgressBar* bar_ = nullptr;
};

} // namespace appencrypt
