// DlgProgress.cpp
#include "dlg/DlgProgress.h"

#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>

DlgProgress::DlgProgress(QWidget* parent, const QString& title)
    : QDialog(parent)
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    // TProgressForm：BorderStyle bsSingle + BorderIcons []（不可缩放、无系统按钮）
    setWindowFlags(Qt::Dialog);
    // ClientWidth 302 / ClientHeight 86
    setFixedSize(302, 86);

    m_label = new QLabel(QStringLiteral("正在处理..."), this);
    m_label->setObjectName(QStringLiteral("progressInfo"));
    m_label->setGeometry(9, 22, 201, 13);        // DFM ProgressInfo（AutoSize = False）
    m_bar = new QProgressBar(this);
    m_bar->setObjectName(QStringLiteral("progressBar"));
    m_bar->setGeometry(8, 48, 202, 17);          // DFM ProgressBar
    m_bar->setRange(0, 256);                     // DFM Min 0 / Max 256
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    m_cancel->setGeometry(217, 47, 75, 23);      // DFM CancelBtn
    m_cancel->setDefault(true);

    // Position = poDesktopCenter
    if (QScreen* scr = screen())
        move(scr->geometry().center() - QPoint(width() / 2, height() / 2));

    connect(m_cancel, &QPushButton::clicked, this, [this]{
        m_cancelled = true;
        m_label->setText(QStringLiteral("正在取消..."));
    });
}

void DlgProgress::SetText(const QString& text)
{
    m_label->setText(text);
}

void DlgProgress::SetProgress(uint64_t done, uint64_t total)
{
    // 原版刻度固定 0..256，按百分比映射
    const uint64_t v = total ? done * 256 / total : 0;
    m_bar->setValue(int(v > 256 ? 256 : v));
    SetText(QStringLiteral("已完成：%1 / %2")
                .arg(static_cast<qulonglong>(done))
                .arg(static_cast<qulonglong>(total)));
    QApplication::processEvents();
}

void DlgProgress::Finish()
{
    if (m_cancelled)
        reject();
    else
        accept();
}

void DlgProgress::closeEvent(QCloseEvent* e)
{
    m_cancelled = true;
    QDialog::closeEvent(e);
}
