// DlgProcess.cpp
#include "dlg/DlgProcess.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// 优先级：原版 PriorityComboBoxChange(0x416590) 把 0x5D7600 起的 5 个 Int32
// {-15,-2,-1,0,+1} 拷到栈上，再按 ItemIndex 取值传给调度对象。
constexpr int kPriority[] = { -15, -2, -1, 0, 1 };

// 数据类型：DFM DataTypeRow 的 16 项描述 + Values（0..14，最后一项 255）
struct TypeItem { const char* name; int value; };
const TypeItem kDataTypes[] = {
    { "比特(7-0)", 0 },
    { "比特(0-7)", 1 },
    { "比特(0-15)", 2 },
    { "比特(0-31)", 3 },
    { "8位无符号整型", 4 },
    { "8位有符号整型", 5 },
    { "16位无符号整型", 6 },
    { "16位有符号整型", 7 },
    { "16位有符号整型(摩托罗拉格式)", 8 },
    { "32位无符号整型", 9 },
    { "32位有符号整型", 10 },
    { "64位有符号整型", 11 },
    { "32位浮点", 12 },
    { "64位浮点", 13 },
    { "字符串", 14 },
    { "未知数据类型", 255 },
};

QString ModeLabel(DlgProcess::Mode m)
{
    switch (m) {
    case DlgProcess::kContinuous: return QStringLiteral("连续选取");
    case DlgProcess::kBlock:      return QStringLiteral("块选取");
    case DlgProcess::kRow:        return QStringLiteral("行选取");
    case DlgProcess::kCol:        return QStringLiteral("列选取");
    default:                      return QStringLiteral("全部数据");
    }
}

QGroupBox* Category(const QString& title, const char* objectName, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    box->setObjectName(QString::fromLatin1(objectName));
    return box;
}

} // namespace

DlgProcess::DlgProcess(QWidget* parent, const QString& comName,
                       const bes::SelectionSet& sel)
    : QDialog(parent), m_comName(comName)
{
    setWindowTitle(comName.isEmpty() ? QStringLiteral("构件数据流处理")
                                     : QStringLiteral("构件数据流处理 - %1").arg(comName));
    resize(434, 448);                       // DFM：Width 434 / Height 448

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(2, 2, 2, 2);   // DFM：BorderWidth 2

    // ------------------------------------------------ 「开始/暂停/停止/复位/退出」
    auto* runBar = new QHBoxLayout;
    m_run   = new QPushButton(QStringLiteral("开始"), this);
    m_pause = new QPushButton(QStringLiteral("暂停"), this);
    m_stop  = new QPushButton(QStringLiteral("停止"), this);
    m_reset = new QPushButton(QStringLiteral("复位"), this);
    m_quit  = new QPushButton(QStringLiteral("退出"), this);
    m_run->setObjectName(QStringLiteral("runButton"));
    m_pause->setObjectName(QStringLiteral("pauseButton"));
    m_stop->setObjectName(QStringLiteral("stopButton"));
    m_reset->setObjectName(QStringLiteral("resetButton"));
    m_quit->setObjectName(QStringLiteral("quitButton"));
    for (QPushButton* b : { m_run, m_pause, m_stop, m_reset, m_quit }) {
        b->setFixedWidth(48);               // DFM：48×22
        b->setFixedHeight(22);
        runBar->addWidget(b);
    }
    // 暂停是 TSpeedButton(AllowAllUp, GroupIndex 1) —— 可反复按下的开关
    m_pause->setCheckable(true);
    runBar->addStretch(1);
    vbox->addLayout(runBar);

    // ------------------------------------------------ 「优先级 / 完成后退出 / 自动打开输出文件」
    auto* optBar = new QHBoxLayout;
    optBar->addWidget(new QLabel(QStringLiteral("优先级"), this));
    m_priority = new QComboBox(this);
    m_priority->setObjectName(QStringLiteral("priorityCombo"));
    m_priority->addItems({ QStringLiteral("极低"), QStringLiteral("最低"),
                           QStringLiteral("较低"), QStringLiteral("正常"),
                           QStringLiteral("高") });
    // FormCreate 0x413A8F：ItemIndex := 3（DFM 未带 ItemIndex，运行期写入）
    m_priority->setCurrentIndex(3);
    optBar->addWidget(m_priority);
    m_autoQuit = new QCheckBox(QStringLiteral("完成后退出"), this);
    m_autoQuit->setObjectName(QStringLiteral("autoQuitCheck"));
    m_autoQuit->setChecked(true);           // DFM：Checked = True
    optBar->addWidget(m_autoQuit);
    m_autoOpen = new QCheckBox(QStringLiteral("自动打开输出文件"), this);
    m_autoOpen->setObjectName(QStringLiteral("autoOpenCheck"));
    m_autoOpen->setChecked(true);           // DFM：Checked = True
    optBar->addWidget(m_autoOpen);
    optBar->addStretch(1);
    vbox->addLayout(optBar);

    // ------------------------------------------------ 数据输入
    auto* inBox = Category(QStringLiteral("数据输入"), "dataInGroup", this);
    auto* inGrid = new QGridLayout(inBox);
    inGrid->addWidget(new QLabel(QStringLiteral("输入选择"), inBox), 0, 0);
    m_mode = new QComboBox(inBox);
    m_mode->setObjectName(QStringLiteral("modeCombo"));
    for (int i = 0; i <= kCol; ++i)
        m_mode->addItem(ModeLabel(static_cast<Mode>(i)));
    // 默认沿用当前选区模式（无选区 → 全部数据）
    int idx = kAll;
    switch (sel.Mode()) {
    case bes::SelectMode::Continuous: idx = kContinuous; break;
    case bes::SelectMode::Rect:       idx = kBlock;      break;
    case bes::SelectMode::Row:        idx = kRow;        break;
    case bes::SelectMode::Col:        idx = kCol;        break;
    default: break;
    }
    m_mode->setCurrentIndex(idx);
    inGrid->addWidget(m_mode, 0, 1);

    m_deselect = new QCheckBox(QStringLiteral("反选"), inBox);
    m_deselect->setObjectName(QStringLiteral("deselectCheck"));
    inGrid->addWidget(m_deselect, 1, 0, 1, 2);

    inGrid->addWidget(new QLabel(QStringLiteral("选取参数"), inBox), 2, 0);
    m_param = new QLineEdit(inBox);
    m_param->setObjectName(QStringLiteral("paramEdit"));
    inGrid->addWidget(m_param, 2, 1);

    inGrid->addWidget(new QLabel(QStringLiteral("数据类型"), inBox), 3, 0);
    m_dataType = new QComboBox(inBox);
    m_dataType->setObjectName(QStringLiteral("dataTypeCombo"));
    for (const auto& t : kDataTypes)
        m_dataType->addItem(QString::fromUtf8(t.name), t.value);
    m_dataType->setCurrentIndex(0);         // DFM：Text = 0
    inGrid->addWidget(m_dataType, 3, 1);
    inGrid->setColumnStretch(1, 1);
    vbox->addWidget(inBox);

    // ------------------------------------------------ 数据输出
    auto* outBox = Category(QStringLiteral("数据输出"), "dataOutGroup", this);
    auto* outGrid = new QGridLayout(outBox);
    outGrid->addWidget(new QLabel(QStringLiteral("输出标志"), outBox), 0, 0);
    m_outFlag = new QComboBox(outBox);
    m_outFlag->setObjectName(QStringLiteral("outFlagCombo"));
    m_outFlag->addItem(QStringLiteral("EOI [分块结束标志]"));
    m_outFlag->addItem(QStringLiteral("EOF [数据结束标志]"));
    m_outFlag->setCurrentIndex(0);          // DFM：Text = 0
    outGrid->addWidget(m_outFlag, 0, 1);

    outGrid->addWidget(new QLabel(QStringLiteral("输出文件"), outBox), 1, 0);
    m_outFile = new QLineEdit(outBox);
    m_outFile->setObjectName(QStringLiteral("outFileEdit"));
    m_outFile->setReadOnly(true);
    m_outFile->setText(QStringLiteral("不输出"));   // 0x5D7683 前后的字面量
    outGrid->addWidget(m_outFile, 1, 1);
    outGrid->setColumnStretch(1, 1);
    vbox->addWidget(outBox);

    // ------------------------------------------------ 构件参数 / 构件自定义
    auto* comBox = Category(QStringLiteral("构件参数"), "comParamGroup", this);
    auto* comGrid = new QGridLayout(comBox);
    comGrid->addWidget(new QLabel(QStringLiteral("自定义参数"), comBox), 0, 0);
    m_customParam = new QPushButton(QStringLiteral("点击按钮打开..."), comBox);
    m_customParam->setObjectName(QStringLiteral("customParamButton"));
    comGrid->addWidget(m_customParam, 0, 1);
    vbox->addWidget(comBox);

    auto* advBox = Category(QStringLiteral("构件自定义"), "advanceGroup", this);
    auto* advGrid = new QGridLayout(advBox);
    advGrid->addWidget(new QLabel(QStringLiteral("自定义显示"), advBox), 0, 0);
    m_customView = new QPushButton(QStringLiteral("点击按钮打开..."), advBox);
    m_customView->setObjectName(QStringLiteral("customViewButton"));
    advGrid->addWidget(m_customView, 0, 1);
    vbox->addWidget(advBox);

    // ------------------------------------------------ 状态行 + 进度条（DFM Panel1）
    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("infoLabel"));
    vbox->addWidget(m_info);
    m_bar = new QProgressBar(this);
    m_bar->setObjectName(QStringLiteral("progressBar"));
    m_bar->setRange(0, 256);                // DFM：Min 0 / Max 256
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    vbox->addWidget(m_bar);
    vbox->addStretch(1);

    // 未绑定构件时两行自定义按钮无对象（原版 com = [form+0x364] = nil）
    m_customParam->setEnabled(!comName.isEmpty());
    m_customView->setEnabled(!comName.isEmpty());

    connect(m_run,  &QPushButton::clicked, this, [this]{ startRun(); });
    connect(m_pause,&QPushButton::clicked, this, [this]{ onPause(); });
    connect(m_stop, &QPushButton::clicked, this, [this]{ onStop(); });
    connect(m_reset,&QPushButton::clicked, this, [this]{ onReset(); });
    connect(m_quit, &QPushButton::clicked, this, [this]{ onQuit(); });
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ applyMode(); });

    setOutputPort(false);
    applyMode();
    enterIdle();
}

// ------------------------------------------------ 处理参数

DlgProcess::Mode DlgProcess::inputMode() const
{
    return static_cast<Mode>(m_mode->currentIndex());
}

bool DlgProcess::deselect() const
{
    return m_deselect->isChecked();         // 原版值 '1'/'0'（ValueChecked/Unchecked）
}

QString DlgProcess::paramText() const
{
    return m_param->text();
}

int DlgProcess::dataTypeValue() const
{
    return m_dataType->currentData().toInt();
}

QString DlgProcess::dataTypeName() const
{
    return m_dataType->currentText();
}

bool DlgProcess::outFlagEoi() const
{
    return m_outFlag->currentIndex() == 0;
}

int DlgProcess::priorityValue() const
{
    const int i = m_priority->currentIndex();
    if (i < 0 || i >= int(sizeof(kPriority) / sizeof(kPriority[0]))) return 0;
    return kPriority[i];
}

bool DlgProcess::autoQuit() const { return m_autoQuit->isChecked(); }
bool DlgProcess::autoOpen() const { return m_autoOpen->isChecked(); }

QString DlgProcess::outputFile() const
{
    const QString t = m_outFile->text();
    const QString prefix = QStringLiteral("文件: ");
    return t.startsWith(prefix) ? t.mid(prefix.size()) : QString();
}

void DlgProcess::setOutputPort(bool on)
{
    m_hasOutputPort = on;
    m_outFile->setText(on ? QStringLiteral("文件: temp.dat")        // 0x5D7668/0x5D7670
                          : QStringLiteral("不输出"));
    // FormCreate 0x413AF7：输出标志仅在没有输出文件时可选
    m_outFlag->setEnabled(!on);
}

// SelectModeRowChange(0x4163B0)：
//   反选可用 = mode ∈ {全部数据, 块选取}（或列选取且构件类型 7）；不可用时清空勾选。
//   选取参数：mode = 全部数据 → 可用并写入「全部数据」；否则禁用并写入段名
//             （连续/块/行/列选取），仅当构件接受手工输入时重新可用。
void DlgProcess::applyMode()
{
    const Mode m = inputMode();
    const bool deSel = (m == kAll || m == kBlock || (m == kCol && m_deselectForCol));
    m_deselect->setEnabled(deSel);
    if (deSel)
        m_deselect->setChecked(false);      // 0x41644C：置空值（48F2F0 写 ''）

    if (m == kAll) {
        m_param->setEnabled(true);
        m_param->setText(ModeLabel(kAll));
    } else {
        // 段名取自 [form+0x340] 列表（连续/块/行/列选取），只有构件接受手工输入时可编辑
        m_param->setEnabled(m_manualInput);
        m_param->setText(ModeLabel(m));
    }
}

// ------------------------------------------------ 运行状态机

bool DlgProcess::validate(QString* err) const
{
    // RunBtnClick 0x415C15..0x415C70：mode≠0 且选取参数为空 → 红字提示并中止
    if (inputMode() != kAll && paramText().isEmpty()) {
        if (err) *err = QStringLiteral("选取参数不能为空！");
        return false;
    }
    // 0x415D8E 起：有输出端口却拿不到文件名 → 「: 文件名不能为空！」
    if (m_hasOutputPort && outputFile().isEmpty()) {
        if (err) *err = QStringLiteral(": 文件名不能为空！");
        return false;
    }
    return true;
}

// ------------------------------------------------ 数据流管线

void DlgProcess::setSource(const bes::FileMap* map, const bes::SelectionSet& sel,
                           const bes::FrameModel& fm, int bitsPerCell)
{
    m_map = map;
    m_sel = &sel;
    m_fm = fm;
    m_bitsPerCell = bitsPerCell > 0 ? bitsPerCell : 8;
}

bes::FlowParams DlgProcess::flowParams() const
{
    bes::FlowParams p;
    switch (inputMode()) {
    case kContinuous: p.mode = bes::SelectMode::Continuous; break;
    case kBlock:      p.mode = bes::SelectMode::Rect;       break;
    case kRow:        p.mode = bes::SelectMode::Row;        break;
    case kCol:        p.mode = bes::SelectMode::Col;        break;
    default:          p.mode = bes::SelectMode::None;       break;
    }
    p.deselect    = deselect();
    p.outEoi      = outFlagEoi();
    p.dataType    = dataTypeValue();
    p.param       = paramText().toStdWString();
    p.priority    = priorityValue();
    p.bitsPerCell = m_bitsPerCell;
    return p;                                        // blockBytes 用 [ctx+0x8c] 的 0x10000
}

// 空闲态（0x415650）：仅「开始」可用，进度条隐藏
void DlgProcess::enterIdle()
{
    m_running = false;
    m_paused = false;
    m_pause->setChecked(false);
    m_run->setEnabled(true);
    m_pause->setEnabled(false);
    m_stop->setEnabled(false);
    m_reset->setEnabled(false);
    m_bar->setVisible(false);
    m_bar->setValue(0);
}

bool DlgProcess::startRun()
{
    QString err;
    if (!validate(&err)) {
        m_info->setText(err);              // 原版把提示写进 InfoLabel 并置红字
        return false;
    }
    if (m_running) return false;
    m_running = true;
    m_paused = false;
    m_exit = false;
    m_pause->setChecked(false);
    // RunBtnClick 尾部 0x416198：开始禁用，暂停/停止可用，复位取决于构件能力 [form+0x344]
    m_run->setEnabled(false);
    m_pause->setEnabled(true);
    m_stop->setEnabled(true);
    m_reset->setEnabled(m_resetSupported);
    m_bar->setVisible(true);
    m_bar->setValue(0);
    m_info->setText(QStringLiteral("正在处理..."));

    // 已绑定数据源与构件输入端口 → 真正把选区字节推给构件。
    // 原版这一步只置 [ctx+0x19] 运行位、由主窗体的数据流线程搬运；本版本在同一线程里
    // 分块搬运并在块间泵事件，于是「暂停/停止/退出」仍能即时生效。
    if (m_map && m_sel && m_engine.Port()) {
        m_engine.SetPump([this](const bes::FlowEngine& e) {
            setProgress(e.bytesWritten(), e.totalBytes());
            while (e.state() == bes::FlowState::Paused && !e.exitRequested())
                QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 20);
        });
        std::wstring werr;
        const bool ok = m_engine.Start(*m_map, m_fm, *m_sel, flowParams(), &werr);
        if (!ok && !werr.empty()) m_info->setText(QString::fromStdWString(werr));
        if (!m_exit) enterIdle();
    }
    return true;
}

// PauseBtnClick(0x4161F8)：读按钮 Down，Down→命令 3（暂停），弹起→命令 4（继续）
void DlgProcess::onPause()
{
    m_paused = m_pause->isChecked();
    // 原版：按下 → SuspendThread([ctx+0x14]) + 构件命令 3；弹起 → 命令 4 + ResumeThread
    if (m_paused) m_engine.Pause();
    else          m_engine.Resume();
    m_info->setText(m_paused ? QStringLiteral("已暂停") : QStringLiteral("正在处理..."));
}

// StopBtnClick(0x416250)：暂停/停止/复位禁用，清运行标志（进度保留）
void DlgProcess::onStop()
{
    m_running = false;
    m_paused = false;
    m_pause->setChecked(false);
    m_pause->setEnabled(false);
    m_stop->setEnabled(false);
    m_reset->setEnabled(false);
    m_engine.Stop();                 // 停止搬运，进度保留
    m_info->setText(QStringLiteral("已停止"));
}

// ResetBtnClick(0x41629C)：向构件发命令 5，界面回到空闲（窗口保持打开）
void DlgProcess::onReset()
{
    m_engine.Reset();                // 构件命令 5
    enterIdle();
    m_info->setText(QString());
}

// CancelBtnClick(0x4162AC)：四个按钮全禁 + 清运行标志 + 置退出请求；FormClose 也走这里
void DlgProcess::onQuit()
{
    m_running = false;
    m_paused = false;
    m_exit = true;
    m_engine.RequestExit();          // [ctx+0x18] := 1
    for (QPushButton* b : { m_run, m_pause, m_stop, m_reset })
        b->setEnabled(false);
    reject();
}

// 原版进度条 Min 0 / Max 256
void DlgProcess::setProgress(uint64_t done, uint64_t total)
{
    m_bar->setVisible(true);
    const int v = total ? int(done * 256 / total) : 0;
    m_bar->setValue(v > 256 ? 256 : v);
    m_info->setText(QStringLiteral("已完成：%1 / %2")
                        .arg(static_cast<qulonglong>(done))
                        .arg(static_cast<qulonglong>(total)));
}

void DlgProcess::setInfo(const QString& text)
{
    m_info->setText(text);
}
