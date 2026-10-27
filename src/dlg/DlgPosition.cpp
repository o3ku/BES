// DlgPosition.cpp
#include "dlg/DlgPosition.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using namespace bes;

DlgPosition::DlgPosition(QWidget* parent, const FrameModel& frame,
                         const BookmarkSet& marks, int bitsPerCell,
                         bool byteUnit)
    : QDialog(parent), m_frame(frame), m_marks(marks),
      m_bitsPerCell(bitsPerCell > 0 ? bitsPerCell : 8),
      m_byteUnit(byteUnit)
{
    setWindowTitle(QStringLiteral("数据定位"));
    resize(360, 260);

    auto* vbox = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_cboMethod = new QComboBox(this);
    m_cboMethod->addItems({
        QStringLiteral("通过文件绝对起点(比特)"),
        QStringLiteral("通过当前相对起点"),
        QStringLiteral("通过行列"),
        QStringLiteral("通过书签"),
    });
    m_edPos = new QLineEdit(this);
    m_edCol = new QLineEdit(this);
    m_edCol->setEnabled(false);
    form->addRow(QStringLiteral("定位方式"), m_cboMethod);
    form->addRow(QStringLiteral("位置"), m_edPos);
    form->addRow(QStringLiteral("列"), m_edCol);
    vbox->addLayout(form);

    m_lbMarks = new QListWidget(this);
    for (const auto& kv : m_marks.All())
        m_lbMarks->addItem(QStringLiteral("第%1比特").arg(kv.first));
    m_lbMarks->setEnabled(false);
    vbox->addWidget(m_lbMarks, 1);

    auto* hBtn = new QHBoxLayout;
    auto* btnApply = new QPushButton(QStringLiteral("应用"), this);
    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    hBtn->addWidget(btnApply);
    hBtn->addStretch(1);
    hBtn->addWidget(btnOk);
    hBtn->addWidget(btnCancel);
    vbox->addLayout(hBtn);

    connect(m_cboMethod, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DlgPosition::OnMethodChanged);
    connect(m_lbMarks, &QListWidget::itemDoubleClicked,
            this, &DlgPosition::OnBookmarkSelected);
    connect(btnApply, &QPushButton::clicked, this, &DlgPosition::OnApply);
    connect(btnOk, &QPushButton::clicked, this, [this]{
        uint64_t bit = 0;
        if (!ComputeResult(bit)) return;
        m_resultBit = bit;
        m_frame.startBit = bit;
        accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

bool DlgPosition::ComputeResult(uint64_t& out) const
{
    const int method = m_cboMethod->currentIndex();
    const uint64_t v = m_edPos->text().toULongLong();
    switch (method) {
    case 1:
        out = m_frame.startBit + v * (m_byteUnit ? 8 : 1);
        return true;
    case 2: {
        const uint64_t rowStep = m_frame.step > 0 ? m_frame.step
                                                  : (m_frame.frameLen ? m_frame.frameLen : 8);
        out = m_frame.startBit + v * rowStep +
              m_edCol->text().toULongLong() * static_cast<uint64_t>(m_bitsPerCell);
        return true;
    }
    case 3: {
        if (QListWidgetItem* it = m_lbMarks->currentItem()) {
            const int idx = m_lbMarks->row(it);
            auto iter = m_marks.All().begin();
            std::advance(iter, idx);
            out = iter->first;
            return true;
        }
        out = m_frame.startBit;
        return true;
    }
    default:
        out = v;
        return true;
    }
}

void DlgPosition::OnApply()
{
    uint64_t bit = 0;
    if (!ComputeResult(bit)) return;
    m_resultBit = bit;
    m_frame.startBit = bit;
    emit Applied(bit);
}

void DlgPosition::OnBookmarkSelected(QListWidgetItem* item)
{
    if (item) m_lbMarks->setCurrentItem(item);
    OnApply();
    accept();
}

void DlgPosition::OnMethodChanged(int idx)
{
    m_edCol->setEnabled(idx == 2);
    m_lbMarks->setEnabled(idx == 3);
}
