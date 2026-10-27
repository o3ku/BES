// DlgProgress.h —— 后台任务进度（对应原 TProgressForm）
#pragma once

#include <QDialog>
#include <cstdint>

class QLabel;
class QProgressBar;
class QPushButton;

class DlgProgress : public QDialog {
    Q_OBJECT
public:
    explicit DlgProgress(QWidget* parent, const QString& title);

    void SetText(const QString& text);
    void SetProgress(uint64_t done, uint64_t total);
    bool Cancelled() const { return m_cancelled; }
    void Finish();

protected:
    // TProgressForm 的 OnClose = FormClose：直接关窗等同「取消」
    void closeEvent(QCloseEvent* e) override;

private:
    QLabel*       m_label = nullptr;
    QProgressBar* m_bar = nullptr;
    QPushButton*  m_cancel = nullptr;
    bool          m_cancelled = false;
};
