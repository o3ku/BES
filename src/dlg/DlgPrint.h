// DlgPrint.h —— 打印设置（左右/上下边距按毫米，页面范围）
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPrinter;

class DlgPrint : public QDialog {
    Q_OBJECT
public:
    explicit DlgPrint(QWidget* parent = nullptr);
    ~DlgPrint() override;

    int Left() const   { return m_left; }
    int Right() const  { return m_right; }
    int Top() const    { return m_top; }
    int Bottom() const { return m_bottom; }
    int First() const  { return m_first; }
    int Last() const   { return m_last; }   // 0 = 直到最后一页

    QPrinter* Printer() const { return m_printer; }

private:
    void UpdatePaperLabel();
    void ReadControls();

    // 原版两个边距输入框默认 10，单位为毫米
    int m_left = 10, m_right = 10, m_top = 10, m_bottom = 10;
    int m_first = 1, m_last = 0;

    QPrinter* m_printer = nullptr;
    QLabel*   m_paper = nullptr;
    QLineEdit* m_edLR = nullptr;
    QLineEdit* m_edTB = nullptr;
    QLineEdit* m_edRange = nullptr;
};
