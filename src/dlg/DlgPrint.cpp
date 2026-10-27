// DlgPrint.cpp
#include "dlg/DlgPrint.h"

#include <algorithm>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QVBoxLayout>

DlgPrint::DlgPrint(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("打印"));
    resize(380, 300);

    m_printer = new QPrinter(QPrinter::HighResolution);

    auto* vbox = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_edLR = new QLineEdit(this);
    m_edLR->setObjectName(QStringLiteral("marginLR"));
    m_edLR->setText(QString::number(m_left));
    m_edLR->setToolTip(QStringLiteral("单位：毫米（左右相同）"));
    m_edTB = new QLineEdit(this);
    m_edTB->setObjectName(QStringLiteral("marginTB"));
    m_edTB->setText(QString::number(m_top));
    m_edTB->setToolTip(QStringLiteral("单位：毫米（上下相同）"));
    m_edRange = new QLineEdit(this);
    m_edRange->setObjectName(QStringLiteral("pageRange"));
    m_edRange->setPlaceholderText(QStringLiteral("如 1-3；留空表示全部"));

    form->addRow(QStringLiteral("左右边距(毫米)"), m_edLR);
    form->addRow(QStringLiteral("上下边距(毫米)"), m_edTB);
    form->addRow(QStringLiteral("页面范围"), m_edRange);
    m_paper = new QLabel(this);
    m_paper->setObjectName(QStringLiteral("paperLabel"));
    m_paper->setWordWrap(true);
    form->addRow(QStringLiteral("纸张"), m_paper);
    UpdatePaperLabel();

    vbox->addLayout(form);
    vbox->addStretch(1);

    auto* hBtn = new QHBoxLayout;
    auto* btnSetup = new QPushButton(QStringLiteral("打印设置(&S)..."), this);
    btnSetup->setObjectName(QStringLiteral("printSetupButton"));
    auto* btnOk = new QPushButton(QStringLiteral("打印"), this);
    btnOk->setObjectName(QStringLiteral("printButton"));
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnCancel->setObjectName(QStringLiteral("cancelButton"));
    hBtn->addWidget(btnSetup);
    hBtn->addStretch(1);
    hBtn->addWidget(btnOk);
    hBtn->addWidget(btnCancel);
    vbox->addLayout(hBtn);

    connect(btnSetup, &QPushButton::clicked, this, [this]{
        QPrintDialog dlg(m_printer, this);
        dlg.setWindowTitle(QStringLiteral("打印设置"));
        if (dlg.exec() == QDialog::Accepted)
            UpdatePaperLabel();
    });
    connect(btnOk, &QPushButton::clicked, this, [this]{
        ReadControls();
        accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

DlgPrint::~DlgPrint()
{
    delete m_printer;
}

void DlgPrint::UpdatePaperLabel()
{
    if (!m_paper || !m_printer) return;
    const QString printer = m_printer->printerName();
    const QString paper = m_printer->pageLayout().pageSize().name();
    m_paper->setText(QStringLiteral("%1  /  %2")
        .arg(printer.isEmpty() ? QStringLiteral("默认打印机") : printer)
        .arg(paper.isEmpty() ? QStringLiteral("默认纸张") : paper));
}

void DlgPrint::ReadControls()
{
    const int lr = std::max(0, m_edLR->text().toInt());
    const int tb = std::max(0, m_edTB->text().toInt());
    m_left = m_right = lr;
    m_top = m_bottom = tb;

    m_first = 1;
    m_last = 0;
    const QString text = m_edRange->text().trimmed();
    if (text.isEmpty())
        return;
    const int dash = text.indexOf(QLatin1Char('-'));
    if (dash < 0) {
        const int page = text.toInt();
        if (page > 0)
            m_first = m_last = page;
        return;
    }
    const QString from = text.left(dash).trimmed();
    const QString to = text.mid(dash + 1).trimmed();
    if (!from.isEmpty())
        m_first = std::max(1, from.toInt());
    if (!to.isEmpty())
        m_last = std::max(1, to.toInt());
    if (m_last > 0 && m_last < m_first)
        std::swap(m_first, m_last);
}
