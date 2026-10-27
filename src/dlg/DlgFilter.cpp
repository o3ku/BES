// DlgFilter.cpp —— 帧过滤（对应原版 TFilterForm）
#include "dlg/DlgFilter.h"

#include <algorithm>

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

#include "core/FileMap.h"

using namespace bes;

namespace {

const char* const kOpSymbols[] = { " == ", " > ", " < ", " != " };

QString OpSymbol(CmpOp op)
{
    return QString::fromLatin1(kOpSymbols[std::clamp(static_cast<int>(op), 0, 3)]);
}

QString TypeName(int bits)
{
    switch (bits) {
    case 1:  return QStringLiteral("比特");
    case 8:  return QStringLiteral("字节");
    case 16: return QStringLiteral("双字节");
    default: return QStringLiteral("四字节");
    }
}

int TypeBits(int index)
{
    static const int bits[] = { 1, 8, 16, 32 };
    return bits[std::clamp(index, 0, 3)];
}

// 原版数值输入为“从低字节到高字节”：最左侧一组是最低字节。
bool ParseValueText(const QString& text, uint64_t& value)
{
    value = 0;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts);
    if (parts.size() > 4)
        return false;
    for (int i = 0; i < parts.size(); ++i) {
        const QString& group = parts[i];
        if (group.isEmpty() || group.size() > 2)
            return false;
        for (const QChar& c : group) {
            if (!QStringLiteral("0123456789abcdefABCDEF").contains(c))
                return false;
        }
        bool ok = false;
        const uint32_t byte = group.toUInt(&ok, 16);
        if (!ok)
            return false;
        value |= static_cast<uint64_t>(byte) << (i * 8);
    }
    return true;
}

uint64_t ValueMask(int bits)
{
    switch (bits) {
    case 1:  return 0x1ull;
    case 8:  return 0xFFull;
    case 16: return 0xFFFFull;
    default: return 0xFFFFFFFFull;
    }
}

// 与原版 FormatValue 一致：按低字节在前输出，双字节/四字节以空格分组。
QString FormatValueText(int bits, uint64_t value)
{
    if (bits == 1)
        return (value & 1ull) ? QStringLiteral("1") : QStringLiteral("0");
    const int count = (bits == 8) ? 1 : (bits == 16 ? 2 : 4);
    QStringList groups;
    for (int i = 0; i < count; ++i) {
        groups << QStringLiteral("%1").arg((value >> (i * 8)) & 0xFFu, 2, 16,
                                           QLatin1Char('0')).toUpper();
    }
    return groups.join(QLatin1Char(' '));
}

// 条件列表一行的显示文本（添加/更改与重开回填共用，保证两处文案一致）
QString DescribeCondition(const FilterCondition& c)
{
    switch (c.kind) {
    case FilterKind::FrameLength:
        return QStringLiteral("帧长%1%2比特").arg(OpSymbol(c.frameOp))
                   .arg(c.frameBits);
    case FilterKind::Value:
        return QStringLiteral("%1开始的%2%3%4")
                   .arg(c.valuePos)
                   .arg(TypeName(c.valueBits))
                   .arg(OpSymbol(c.valueOp))
                   .arg(QStringLiteral("0X") + QString::number(c.value, 16).toUpper());
    default:
        return c.crcPass ? QStringLiteral("CRC校验通过")
                         : QStringLiteral("CRC校验失败");
    }
}

} // namespace

DlgFilter::DlgFilter(QWidget* parent, const FileMap& map, const FrameModel& fm,
                     const FrameFilter* existing, const VariableFrameSet* vframes)
    : QDialog(parent), m_map(&map), m_frame(fm), m_vframes(vframes)
{
    setWindowTitle(QStringLiteral("帧过滤"));
    resize(440, 320);

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(9, 9, 9, 9);
    vbox->setSpacing(6);

    // ---- 页签 ----
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("filterTabs"));
    m_tabs->setFixedHeight(120);

    // 帧长过滤：帧长 [判定] [输入] 字节
    auto* pageFrame = new QWidget(m_tabs);
    auto* hFrame = new QHBoxLayout(pageFrame);
    hFrame->setContentsMargins(12, 16, 12, 12);
    hFrame->addWidget(new QLabel(QStringLiteral("帧长"), pageFrame));
    m_frameJudge = new QComboBox(pageFrame);
    m_frameJudge->setObjectName(QStringLiteral("frameJudge"));
    m_frameJudge->addItems({ QStringLiteral("等于"), QStringLiteral("大于"),
                             QStringLiteral("小于"), QStringLiteral("不等于") });
    m_frameJudge->setMinimumWidth(80);
    hFrame->addWidget(m_frameJudge);
    m_frameEdit = new QLineEdit(pageFrame);
    m_frameEdit->setObjectName(QStringLiteral("frameEdit"));
    m_frameEdit->setMaxLength(8);
    m_frameEdit->setMinimumWidth(100);
    hFrame->addWidget(m_frameEdit);
    hFrame->addWidget(new QLabel(QStringLiteral("字节"), pageFrame));
    hFrame->addStretch(1);
    m_tabs->addTab(pageFrame, QStringLiteral("帧长过滤"));

    // 数值过滤：[位置] 开始的 [类型] [判定] [数值]\n提示图片
    auto* pageValue = new QWidget(m_tabs);
    auto* vValue = new QVBoxLayout(pageValue);
    vValue->setContentsMargins(6, 10, 6, 4);
    vValue->setSpacing(3);
    auto* hValue = new QHBoxLayout;
    hValue->setSpacing(5);
    hValue->addWidget(new QLabel(QStringLiteral("位置"), pageValue));
    m_dataPos = new QLineEdit(QStringLiteral("0"), pageValue);
    m_dataPos->setObjectName(QStringLiteral("dataPositionEdit"));
    m_dataPos->setMaxLength(8);
    m_dataPos->setFixedWidth(60);
    hValue->addWidget(m_dataPos);
    hValue->addWidget(new QLabel(QStringLiteral("开始的"), pageValue));
    m_dataType = new QComboBox(pageValue);
    m_dataType->setObjectName(QStringLiteral("dataTypeCombo"));
    m_dataType->addItems({ QStringLiteral("比特"), QStringLiteral("字节"),
                           QStringLiteral("双字节"), QStringLiteral("四字节") });
    hValue->addWidget(m_dataType);
    m_dataJudge = new QComboBox(pageValue);
    m_dataJudge->setObjectName(QStringLiteral("dataJudgeCombo"));
    m_dataJudge->addItems({ QStringLiteral("等于"), QStringLiteral("大于"),
                            QStringLiteral("小于"), QStringLiteral("不等于") });
    m_dataJudge->setMinimumWidth(62);
    hValue->addWidget(m_dataJudge);
    m_dataEdit = new QLineEdit(QStringLiteral("00 00 00 00"), pageValue);
    m_dataEdit->setObjectName(QStringLiteral("dataEdit"));
    m_dataEdit->setMaxLength(11);
    m_dataEdit->setMinimumWidth(84);
    hValue->addWidget(m_dataEdit, 1);
    vValue->addLayout(hValue);
    auto* hint = new QLabel(QStringLiteral(
        "按从低字节到高字节次序输入十六进制，字节间以空格分割\n如：12 34 AB CD"), pageValue);
    hint->setFrameShape(QFrame::Box);
    hint->setStyleSheet(QStringLiteral("color:#333333; background:#ffffff; padding:2px;"));
    hint->setWordWrap(true);
    vValue->addWidget(hint);
    vValue->addStretch(1);
    m_tabs->addTab(pageValue, QStringLiteral("数值过滤"));

    // CRC 过滤
    auto* pageCrc = new QWidget(m_tabs);
    auto* hCrc = new QHBoxLayout(pageCrc);
    hCrc->setContentsMargins(24, 24, 12, 12);
    m_crcOk = new QRadioButton(QStringLiteral("CRC通过"), pageCrc);
    m_crcOk->setObjectName(QStringLiteral("crcOkRadio"));
    m_crcError = new QRadioButton(QStringLiteral("CRC失败"), pageCrc);
    m_crcError->setObjectName(QStringLiteral("crcErrorRadio"));
    m_crcOk->setChecked(true);
    auto* crcGroup = new QButtonGroup(pageCrc);
    crcGroup->addButton(m_crcOk);
    crcGroup->addButton(m_crcError);
    hCrc->addWidget(m_crcOk);
    hCrc->addSpacing(18);
    hCrc->addWidget(m_crcError);
    hCrc->addStretch(1);
    m_tabs->addTab(pageCrc, QStringLiteral("CRC过滤"));

    vbox->addWidget(m_tabs);

    // ---- 条件列表及管理按钮 ----
    auto* hCond = new QHBoxLayout;
    hCond->addWidget(new QLabel(QStringLiteral("过滤条件"), this));
    hCond->addStretch(1);
    auto* btnAdd = new QPushButton(QStringLiteral("添加"), this);
    btnAdd->setObjectName(QStringLiteral("addButton"));
    m_btnEdit = new QPushButton(QStringLiteral("更改"), this);
    m_btnEdit->setObjectName(QStringLiteral("editButton"));
    m_btnDel = new QPushButton(QStringLiteral("删除"), this);
    m_btnDel->setObjectName(QStringLiteral("deleteButton"));
    m_btnClear = new QPushButton(QStringLiteral("清空"), this);
    m_btnClear->setObjectName(QStringLiteral("clearButton"));
    for (QPushButton* b : { btnAdd, m_btnEdit, m_btnDel, m_btnClear })
        b->setFixedWidth(58);
    hCond->addWidget(btnAdd);
    hCond->addWidget(m_btnEdit);
    hCond->addWidget(m_btnDel);
    hCond->addWidget(m_btnClear);
    vbox->addLayout(hCond);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("filterList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setFixedHeight(90);
    vbox->addWidget(m_list);

    // ---- 状态与确定/取消/应用 ----
    auto* hBottom = new QHBoxLayout;
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("filterStatus"));
    m_status->setStyleSheet(QStringLiteral("color:#0000cc; font-weight:bold;"));
    hBottom->addWidget(m_status);
    hBottom->addStretch(1);
    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setObjectName(QStringLiteral("okButton"));
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnCancel->setObjectName(QStringLiteral("cancelButton"));
    m_btnApply = new QPushButton(QStringLiteral("应用"), this);
    m_btnApply->setObjectName(QStringLiteral("applyButton"));
    for (QPushButton* b : { btnOk, btnCancel, m_btnApply })
        b->setFixedWidth(75);
    hBottom->addWidget(btnOk);
    hBottom->addWidget(btnCancel);
    hBottom->addWidget(m_btnApply);
    vbox->addLayout(hBottom);

    connect(btnAdd, &QPushButton::clicked, this, &DlgFilter::OnAdd);
    connect(m_btnEdit, &QPushButton::clicked, this, &DlgFilter::OnEdit);
    connect(m_btnDel, &QPushButton::clicked, this, &DlgFilter::OnDelete);
    connect(m_btnClear, &QPushButton::clicked, this, &DlgFilter::OnClear);
    connect(m_btnApply, &QPushButton::clicked, this, &DlgFilter::OnApply);
    connect(btnOk, &QPushButton::clicked, this, &DlgFilter::OnOk);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged, this, &DlgFilter::OnSelectionChanged);
    connect(m_dataType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DlgFilter::OnDataTypeChanged);
    connect(m_dataEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        const QString upper = text.toUpper();
        if (upper != text)
            m_dataEdit->setText(upper);
    });

    if (m_frame.frameLen > 0)
        m_frameEdit->setText(QString::number(m_frame.frameLen / 8));
    else
        m_frameEdit->setText(QStringLiteral("32"));

    OnDataTypeChanged();

    // 回填已生效的过滤条件：否则重开点「确定」会把上次的条件清掉
    if (existing && !existing->conditions.empty()) {
        m_conds = existing->conditions;
        m_filter = *existing;
        {
            const QSignalBlocker blocker(m_list);
            for (const FilterCondition& c : m_conds)
                m_list->addItem(DescribeCondition(c));
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        LoadRow(0);
        m_needRun = true;
    }

    SyncButtons();
}

void DlgFilter::OnDataTypeChanged()
{
    m_dataJudge->clear();
    m_dataJudge->addItem(QStringLiteral("等于"));
    if (m_dataType->currentIndex() != 0) {
        m_dataJudge->addItem(QStringLiteral("大于"));
        m_dataJudge->addItem(QStringLiteral("小于"));
        m_dataJudge->addItem(QStringLiteral("不等于"));
    }
    m_dataJudge->setCurrentIndex(0);
}

bool DlgFilter::BuildFromControls(FilterCondition& cond, QString& desc)
{
    const int page = m_tabs->currentIndex();
    cond = FilterCondition();

    if (page == 0) {
        bool ok = false;
        const int bytes = m_frameEdit->text().trimmed().toInt(&ok);
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("帧长输入错误！"));
            return false;
        }
        if (bytes <= 0) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("帧长必须大于0！"));
            return false;
        }
        cond.kind = FilterKind::FrameLength;
        cond.frameBits = bytes * 8;
        cond.frameOp = static_cast<CmpOp>(std::max(0, m_frameJudge->currentIndex()));
        desc = QStringLiteral("帧长%1%2比特").arg(OpSymbol(cond.frameOp))
                   .arg(cond.frameBits);
        return true;
    }

    if (page == 1) {
        bool ok = false;
        const int pos = m_dataPos->text().trimmed().toInt(&ok);
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("起始位置输入错误！"));
            return false;
        }
        if (pos < 0) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                                 QStringLiteral("起始位置必须非负！"));
            return false;
        }
        uint64_t value = 0;
        if (!ParseValueText(m_dataEdit->text(), value)) {
            QMessageBox::warning(this, QStringLiteral("错误"),
                QStringLiteral("数值输入错误！请从低字节到高字节依次输入十六进制\n"
                               "如：12 34 AB CD"));
            return false;
        }
        cond.kind = FilterKind::Value;
        cond.valuePos = pos;
        cond.valueBits = TypeBits(m_dataType->currentIndex());
        cond.valueOp = static_cast<CmpOp>(std::max(0, m_dataJudge->currentIndex()));
        cond.value = value & ValueMask(cond.valueBits);
        desc = QStringLiteral("%1开始的%2%3%4")
                   .arg(cond.valuePos)
                   .arg(TypeName(cond.valueBits))
                   .arg(OpSymbol(cond.valueOp))
                   .arg(QStringLiteral("0X") + QString::number(cond.value, 16).toUpper());
        return true;
    }

    cond.kind = FilterKind::Crc;
    cond.crcPass = m_crcOk->isChecked();
    desc = cond.crcPass ? QStringLiteral("CRC校验通过") : QStringLiteral("CRC校验失败");
    return true;
}

void DlgFilter::LoadRow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_conds.size()))
        return;
    const FilterCondition& c = m_conds[static_cast<size_t>(row)];

    if (c.kind == FilterKind::FrameLength) {
        m_tabs->setCurrentIndex(0);
        m_frameJudge->setCurrentIndex(std::clamp(static_cast<int>(c.frameOp), 0, 3));
        m_frameEdit->setText(QString::number(c.frameBits / 8));
    } else if (c.kind == FilterKind::Value) {
        m_tabs->setCurrentIndex(1);
        m_dataPos->setText(QString::number(c.valuePos));
        {
            QSignalBlocker blocker(m_dataType);
            const int typeIndex = (c.valueBits == 1) ? 0 : (c.valueBits == 8 ? 1 :
                                  (c.valueBits == 16 ? 2 : 3));
            m_dataType->setCurrentIndex(typeIndex);
        }
        OnDataTypeChanged();
        m_dataJudge->setCurrentIndex(std::clamp(static_cast<int>(c.valueOp), 0,
                                                m_dataJudge->count() - 1));
        m_dataEdit->setText(FormatValueText(c.valueBits, c.value));
    } else {
        m_tabs->setCurrentIndex(2);
        if (c.crcPass)
            m_crcOk->setChecked(true);
        else
            m_crcError->setChecked(true);
    }
}

void DlgFilter::SyncButtons()
{
    const int count = m_list->count();
    const int row = m_list->currentRow();
    const bool hasSel = (row >= 0 && row < count);
    m_btnEdit->setEnabled(hasSel);
    m_btnDel->setEnabled(hasSel);
    m_btnClear->setEnabled(count > 0);
    m_btnApply->setEnabled(count > 0);
}

void DlgFilter::OnAdd()
{
    FilterCondition cond;
    QString desc;
    if (!BuildFromControls(cond, desc))
        return;

    m_conds.push_back(cond);
    {
        QSignalBlocker blocker(m_list);
        m_list->addItem(desc);
        m_list->setCurrentRow(m_list->count() - 1);
    }
    m_needRun = true;
    SyncButtons();
}

void DlgFilter::OnEdit()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_conds.size()))
        return;

    FilterCondition cond;
    QString desc;
    if (!BuildFromControls(cond, desc))
        return;

    m_conds[static_cast<size_t>(row)] = cond;
    m_list->item(row)->setText(desc);
    m_needRun = true;
}

void DlgFilter::OnDelete()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_conds.size()))
        return;

    m_conds.erase(m_conds.begin() + row);
    {
        QSignalBlocker blocker(m_list);
        delete m_list->takeItem(row);
        if (!m_conds.empty())
            m_list->setCurrentRow(std::min(row, static_cast<int>(m_conds.size()) - 1));
    }
    m_needRun = true;
    SyncButtons();
}

void DlgFilter::OnClear()
{
    m_conds.clear();
    m_list->clear();
    m_needRun = true;
    SyncButtons();
}

void DlgFilter::OnSelectionChanged()
{
    const int row = m_list->currentRow();
    if (row >= 0)
        LoadRow(row);
    SyncButtons();
}

bool DlgFilter::RunFilter()
{
    m_filter = FrameFilter();
    m_filter.conditions = m_conds;
    m_matched.clear();

    if (m_conds.empty()) {
        m_needRun = false;
        m_status->setText(QString());
        return true;
    }
    if (!m_map || !m_map->IsOpen()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("没有打开的数据文件。"));
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    SearchEngine eng;
    std::atomic<bool> stop{false};
    const bool variable = m_frame.bVariable && m_vframes && m_vframes->Size() > 0;
    const bool ok = variable
        ? eng.ApplyFilter(*m_map, *m_vframes, m_filter, m_matched,
            [this](uint64_t done, uint64_t total, bool finished) {
                if (finished || total == 0)
                    return;
                m_status->setText(QStringLiteral("已完成：%1%").arg(done * 100 / total));
                QApplication::processEvents();
            }, stop)
        : eng.ApplyFilter(*m_map, m_frame, m_filter, m_matched,
            [this](uint64_t done, uint64_t total, bool finished) {
                if (finished || total == 0)
                    return;
                m_status->setText(QStringLiteral("已完成：%1%").arg(done * 100 / total));
                QApplication::processEvents();
            }, stop);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        m_status->setText(QString());
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("过滤未完成。"));
        return false;
    }
    m_needRun = false;
    m_status->setText(QStringLiteral("已完成：100%"));
    emit FilterApplied();
    return true;
}

void DlgFilter::OnApply()
{
    RunFilter();
}

void DlgFilter::OnOk()
{
    if (!m_conds.empty() && m_needRun && !RunFilter())
        return;
    if (m_conds.empty()) {
        m_filter = FrameFilter();
        m_matched.clear();
    }
    accept();
}
