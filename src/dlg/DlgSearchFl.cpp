// DlgSearchFl.cpp
#include "dlg/DlgSearchFl.h"
#include "dlg/DlgProgress.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace bes;

DlgSearchFl::DlgSearchFl(QWidget* parent, const FileMap& map, uint64_t currentBit,
                         uint64_t frameLenBits)
    : QDialog(parent), m_map(&map), m_currentBit(currentBit),
      m_frameLenBits(frameLenBits)
{
    setWindowTitle(QStringLiteral("搜索帧长"));
    resize(420, 260);

    auto* vbox = new QVBoxLayout(this);
    vbox->addWidget(new QLabel(
        QStringLiteral("以当前帧起点为基准，按检测行数统计全0/1列，\n"
                       "找到同步列数不少于要求的帧长。"), this));

    auto* gbRange = new QGroupBox(QStringLiteral("搜索范围"), this);
    auto* vRange = new QVBoxLayout(gbRange);
    m_rbHead = new QRadioButton(QStringLiteral("从头开始(&F)"), gbRange);
    m_rbCur = new QRadioButton(QStringLiteral("从当前帧长开始(&C)"), gbRange);
    m_rbNext = new QRadioButton(QStringLiteral("从下一帧长开始(&N)"), gbRange);
    m_rbHead->setChecked(true);
    auto* bg = new QButtonGroup(this);
    bg->addButton(m_rbHead);
    bg->addButton(m_rbCur);
    bg->addButton(m_rbNext);
    vRange->addWidget(m_rbHead);
    vRange->addWidget(m_rbCur);
    vRange->addWidget(m_rbNext);
    vbox->addWidget(gbRange);

    auto* gbParam = new QGroupBox(QStringLiteral("参数设置"), this);
    auto* fParam = new QFormLayout(gbParam);
    m_edRows = new QLineEdit(QStringLiteral("30"), gbParam);
    m_edRows->setObjectName(QStringLiteral("rowCountEdit"));
    m_edRows->setValidator(new QIntValidator(1, 1000000, m_edRows));
    m_edRows->setMaximumWidth(120);
    m_edCols = new QLineEdit(QStringLiteral("2"), gbParam);
    m_edCols->setObjectName(QStringLiteral("colCountEdit"));
    m_edCols->setValidator(new QIntValidator(1, 99999999, m_edCols));
    m_edCols->setMaximumWidth(120);
    fParam->addRow(QStringLiteral("检测行数"), m_edRows);
    fParam->addRow(QStringLiteral("最少同步列数"), m_edCols);
    vbox->addWidget(gbParam);

    m_lblResult = new QLabel(this);
    m_lblResult->setObjectName(QStringLiteral("resultLabel"));
    m_lblResult->setWordWrap(true);
    vbox->addWidget(m_lblResult);
    vbox->addStretch(1);

    auto* hBtn = new QHBoxLayout;
    m_btnSearch = new QPushButton(QStringLiteral("搜索"), this);
    m_btnSearch->setObjectName(QStringLiteral("searchButton"));
    auto* btnClose = new QPushButton(QStringLiteral("关闭"), this);
    m_btnSearch->setDefault(true);
    hBtn->addStretch(1);
    hBtn->addWidget(m_btnSearch);
    hBtn->addWidget(btnClose);
    vbox->addLayout(hBtn);

    connect(m_btnSearch, &QPushButton::clicked, this, &DlgSearchFl::OnSearch);
    connect(btnClose, &QPushButton::clicked, this, [this]{
        m_stop = true;
        reject();
    });
}

void DlgSearchFl::OnSearch()
{
    int rows = m_edRows->text().toInt();
    if (rows < 10) rows = 10;          // 与原版一致：检测行数下限 10
    int cols = m_edCols->text().toInt();
    if (cols < 1) cols = 1;
    m_edRows->setText(QString::number(rows));
    m_edCols->setText(QString::number(cols));

    const uint64_t base = m_frameLenBits > 0 ? m_frameLenBits : 2;
    uint64_t startLen = 2;
    if (m_rbCur->isChecked())
        startLen = base;
    else if (m_rbNext->isChecked())
        startLen = base + 1;

    FrameLengthSpec spec;
    spec.startLenBits = startLen;
    spec.originBit = m_currentBit;
    spec.rowCount = rows;
    spec.minSyncCols = cols;

    m_stop = false;
    m_foundLen = 0;
    m_syncCols = 0;
    m_lblResult->setText(QStringLiteral("正在搜索…"));
    m_btnSearch->setEnabled(false);

    DlgProgress prog(this, QStringLiteral("搜索帧长"));
    prog.show();

    SearchEngine eng;
    FrameLengthResult result;
    eng.FindFrameLength(*m_map, spec, result,
        [this, &prog](uint64_t done, uint64_t total, bool finished) {
            if (prog.Cancelled()) m_stop = true;
            if (!finished) prog.SetProgress(done, total);
        }, m_stop);

    prog.Finish();
    prog.close();
    m_btnSearch->setEnabled(true);

    if (m_stop) {
        m_lblResult->setText(QStringLiteral("搜索已中止。"));
        return;
    }
    if (result.frameLenBits > 0) {
        m_foundLen = result.frameLenBits;
        m_syncCols = result.syncColumns;
        m_lblResult->setText(QStringLiteral("帧长%1，共找到%2个全0/1列")
                                 .arg(static_cast<qulonglong>(m_foundLen))
                                 .arg(static_cast<qulonglong>(m_syncCols)));
        emit FrameLengthFound(static_cast<quint64>(m_foundLen),
                              static_cast<quint64>(m_syncCols));
    } else {
        m_lblResult->setText(QStringLiteral("未找到同步列数不少于 %1 的帧长。").arg(cols));
    }
}
