// DlgParamSet.cpp
#include "dlg/DlgParamSet.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStorageInfo>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

using namespace bes;

namespace {
inline QColor ToQColor(uint32_t c) {
    return QColor(static_cast<int>(c & 0xFF),
                  static_cast<int>((c >> 8) & 0xFF),
                  static_cast<int>((c >> 16) & 0xFF));
}
inline uint32_t FromQColor(const QColor& c) {
    return static_cast<uint32_t>(c.red()) |
           (static_cast<uint32_t>(c.green()) << 8) |
           (static_cast<uint32_t>(c.blue()) << 16);
}

struct ColorSlot {
    const wchar_t* name;
    uint32_t ColorScheme::*field;
};
const ColorSlot kColorSlots[9] = {
    { L"正文颜色",   &ColorScheme::body },
    { L"背景颜色",   &ColorScheme::background },
    { L"坐标颜色",   &ColorScheme::coord },
    { L"标尺颜色",   &ColorScheme::ruler },
    { L"TAB颜色",    &ColorScheme::tab },
    { L"标记颜色",   &ColorScheme::marker },
    { L"书签颜色",   &ColorScheme::bookmark },
    { L"选取颜色",   &ColorScheme::selection },
    { L"错误帧颜色", &ColorScheme::errFrame },
};

// 与原版 ModeComboBox 的 5 项一一对应（位图模式不在该列表中）
int ModeIndex(ViewMode v) {
    switch (v) {
    case ViewMode::BitBitmap: return 0;
    case ViewMode::BitChar:   return 1;
    case ViewMode::Hex:       return 2;
    case ViewMode::Char:      return 3;
    case ViewMode::HexChar:   return 4;
    case ViewMode::Pixel:     return 0;
    }
    return 0;
}
ViewMode ModeFromIndex(int i) {
    switch (i) {
    case 0: return ViewMode::BitBitmap;
    case 1: return ViewMode::BitChar;
    case 2: return ViewMode::Hex;
    case 3: return ViewMode::Char;
    case 4: return ViewMode::HexChar;
    }
    return ViewMode::BitBitmap;
}

uint64_t ParseU64(const QString& s, uint64_t def) {
    bool ok = false;
    const qlonglong v = s.trimmed().toLongLong(&ok);
    if (!ok || v < 0) return def;
    return static_cast<uint64_t>(v);
}
int ParseIntDef(const QString& s, int def) {
    bool ok = false;
    const int v = s.trimmed().toInt(&ok);
    return ok ? v : def;
}

const uint64_t kMaxFrameLen = 0x55D4A80ull; // 原版 90000000 比特
} // namespace

DlgParamSet::DlgParamSet(QWidget* parent, const std::vector<Window>& windows,
                         int current, const ColorScheme& colors,
                         const ComponentHost::Options& opts)
    : QDialog(parent), m_windows(windows), m_colors(colors), m_opts(opts)
{
    if (m_windows.empty())
        m_windows.push_back(Window{});
    if (current < 0 || current >= static_cast<int>(m_windows.size()))
        current = 0;

    BuildUi();

    m_cur = current;
    if (m_windowList && m_windowList->topLevelItemCount() > m_cur)
        m_windowList->setCurrentItem(m_windowList->topLevelItem(m_cur));
    LoadWindow(m_cur);

    // 初始化完成后再接编辑信号，避免打开对话框即置「应用」可用
    const auto edits = findChildren<QLineEdit*>();
    for (QLineEdit* e : edits)
        connect(e, &QLineEdit::textChanged, this, [this](const QString&) { MarkDirty(); });
    const auto combos = findChildren<QComboBox*>();
    for (QComboBox* c : combos)
        connect(c, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, [this](int) { MarkDirty(); });
    const auto checks = findChildren<QCheckBox*>();
    for (QCheckBox* c : checks)
        connect(c, &QCheckBox::toggled, this, [this](bool) { MarkDirty(); });
    connect(m_fromPort, &QRadioButton::toggled, this, [this](bool) {
        RefreshFormatEnabled();
        MarkDirty();
    });
    connect(m_fromManual, &QRadioButton::toggled, this, [this](bool) {
        RefreshFormatEnabled();
        MarkDirty();
    });
    if (m_windowList) {
        connect(m_windowList, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
                    if (!item || m_loading) return;
                    const int row = m_windowList->indexOfTopLevelItem(item);
                    if (row >= 0 && row != m_cur) OnWindowChanged(row);
                });
    }
}

void DlgParamSet::BuildUi()
{
    setWindowTitle(QStringLiteral("参数设置"));
    resize(440, 360);

    auto* vbox = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("paramTabs"));
    vbox->addWidget(tabs, 1);

    // ---------------- 数据格式 ----------------
    auto* pageData = new QWidget(tabs);
    auto* hData = new QHBoxLayout(pageData);

    m_windowList = new QTreeWidget(pageData);
    m_windowList->setObjectName(QStringLiteral("windowList"));
    m_windowList->setHeaderHidden(true);
    m_windowList->setRootIsDecorated(false);
    m_windowList->setIndentation(0);
    m_windowList->setColumnCount(1);
    m_windowList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_windowList->setFixedWidth(132);
    for (const Window& w : m_windows)
        m_windowList->addTopLevelItem(new QTreeWidgetItem(QStringList(w.title)));
    hData->addWidget(m_windowList, 0, Qt::AlignTop);

    auto* right = new QVBoxLayout;

    auto* gbChoice = new QGroupBox(QStringLiteral("格式选择"), pageData);
    auto* hChoice = new QHBoxLayout(gbChoice);
    m_fromPort = new QRadioButton(QStringLiteral("从对应状态端口读取"), gbChoice);
    m_fromPort->setObjectName(QStringLiteral("fromPortRadio"));
    m_fromManual = new QRadioButton(QStringLiteral("自行设置"), gbChoice);
    m_fromManual->setObjectName(QStringLiteral("fromManualRadio"));
    hChoice->addWidget(m_fromPort);
    hChoice->addStretch(1);
    hChoice->addWidget(m_fromManual);
    right->addWidget(gbChoice);

    m_formatGroup = new QGroupBox(QStringLiteral("格式设置"), pageData);
    m_formatGroup->setObjectName(QStringLiteral("formatGroup"));
    auto* form = new QFormLayout(m_formatGroup);
    m_cboFormat = new QComboBox(m_formatGroup);
    m_cboFormat->setObjectName(QStringLiteral("modeCombo"));
    m_cboFormat->addItems({
        QStringLiteral("比特象素"),
        QStringLiteral("等长帧比特字符"),
        QStringLiteral("等长帧十六进制"),
        QStringLiteral("字符方式"),
        QStringLiteral("十六进制字符对应"),
    });
    m_edStart = new QLineEdit(m_formatGroup);
    m_edStart->setObjectName(QStringLiteral("startEdit"));
    m_edStart->setMaxLength(11);
    m_edFrameLen = new QLineEdit(m_formatGroup);
    m_edFrameLen->setObjectName(QStringLiteral("frameLenEdit"));
    m_edFrameLen->setMaxLength(8);
    m_edFrameLen->setToolTip(QStringLiteral("0 = 按屏宽自动（一屏宽为一帧）"));
    m_edStep = new QLineEdit(m_formatGroup);
    m_edStep->setObjectName(QStringLiteral("stepEdit"));
    m_edStep->setMaxLength(8);
    form->addRow(QStringLiteral("浏览方式"), m_cboFormat);
    form->addRow(QStringLiteral("起点"), m_edStart);
    form->addRow(QStringLiteral("帧长"), m_edFrameLen);
    form->addRow(QStringLiteral("步进"), m_edStep);

    auto* hFlags = new QHBoxLayout;
    m_chkInvert = new QCheckBox(QStringLiteral("取反"), m_formatGroup);
    m_chkInvert->setObjectName(QStringLiteral("invertCheck"));
    m_chkHigh = new QCheckBox(QStringLiteral("高位在前(0-7)"), m_formatGroup);
    m_chkHigh->setObjectName(QStringLiteral("highBitCheck"));
    hFlags->addWidget(m_chkInvert);
    hFlags->addWidget(m_chkHigh);
    hFlags->addStretch(1);
    form->addRow(hFlags);
    right->addWidget(m_formatGroup);
    right->addStretch(1);
    hData->addLayout(right, 1);
    tabs->addTab(pageData, QStringLiteral("数据格式"));

    // ---------------- 显示参数 ----------------
    auto* pageView = new QWidget(tabs);
    auto* vView = new QVBoxLayout(pageView);
    auto* formView = new QFormLayout;
    m_edXStep = new QLineEdit(pageView);
    m_edXStep->setObjectName(QStringLiteral("xStepEdit"));
    m_edXStep->setMaxLength(9);
    m_edYStep = new QLineEdit(pageView);
    m_edYStep->setObjectName(QStringLiteral("yStepEdit"));
    m_edYStep->setMaxLength(9);
    m_edXTab = new QLineEdit(pageView);
    m_edXTab->setObjectName(QStringLiteral("xTabEdit"));
    m_edXTab->setMaxLength(9);
    m_edYTab = new QLineEdit(pageView);
    m_edYTab->setObjectName(QStringLiteral("yTabEdit"));
    m_edYTab->setMaxLength(9);
    formView->addRow(QStringLiteral("X方向浏览步进"), m_edXStep);
    formView->addRow(QStringLiteral("Y方向浏览步进"), m_edYStep);
    formView->addRow(QStringLiteral("X方向TAB宽度"), m_edXTab);
    formView->addRow(QStringLiteral("Y方向TAB宽度"), m_edYTab);
    vView->addLayout(formView);

    m_chkRuler = new QCheckBox(QStringLiteral("显示标尺"), pageView);
    m_chkRuler->setObjectName(QStringLiteral("rulerCheck"));
    m_chkXTab = new QCheckBox(QStringLiteral("显示X方向TAB"), pageView);
    m_chkXTab->setObjectName(QStringLiteral("showXTabCheck"));
    m_chkYTab = new QCheckBox(QStringLiteral("显示Y方向TAB"), pageView);
    m_chkYTab->setObjectName(QStringLiteral("showYTabCheck"));
    m_chkTabFollow = new QCheckBox(QStringLiteral("TAB跟随"), pageView);
    m_chkTabFollow->setObjectName(QStringLiteral("tabFollowCheck"));
    vView->addWidget(m_chkRuler);
    vView->addWidget(m_chkXTab);
    vView->addWidget(m_chkYTab);
    vView->addWidget(m_chkTabFollow);
    vView->addStretch(1);
    tabs->addTab(pageView, QStringLiteral("显示参数"));

    // ---------------- 颜色设置 ----------------
    auto* pageColor = new QWidget(tabs);
    auto* grid = new QGridLayout(pageColor);
    for (int i = 0; i < 9; ++i) {
        auto* lb = new QLabel(QString::fromWCharArray(kColorSlots[i].name), pageColor);
        m_colorBtn[i] = new QPushButton(pageColor);
        m_colorBtn[i]->setObjectName(QStringLiteral("colorButton%1").arg(i));
        m_colorBtn[i]->setFixedSize(72, 22);
        grid->addWidget(lb, i, 0);
        grid->addWidget(m_colorBtn[i], i, 1);
        connect(m_colorBtn[i], &QPushButton::clicked, this, [this, i] { PickColor(i); });
    }
    auto* btnDefault = new QPushButton(QStringLiteral("默认方案"), pageColor);
    btnDefault->setObjectName(QStringLiteral("defaultSchemeButton"));
    auto* btnOne = new QPushButton(QStringLiteral("方案一"), pageColor);
    btnOne->setObjectName(QStringLiteral("schemeOneButton"));
    auto* hScheme = new QHBoxLayout;
    hScheme->addWidget(btnDefault);
    hScheme->addWidget(btnOne);
    hScheme->addStretch(1);
    grid->addLayout(hScheme, 9, 0, 1, 2);
    grid->setRowStretch(10, 1);
    connect(btnDefault, &QPushButton::clicked, this, &DlgParamSet::DefaultScheme);
    connect(btnOne, &QPushButton::clicked, this, &DlgParamSet::SchemeOne);
    RefreshColorButtons();
    tabs->addTab(pageColor, QStringLiteral("颜色设置"));

    // ---------------- 构件选项 ----------------
    auto* pageCom = new QWidget(tabs);
    auto* vCom = new QVBoxLayout(pageCom);
    auto* formCom = new QFormLayout;
    m_edCache = new QLineEdit(pageCom);
    m_edCache->setObjectName(QStringLiteral("cacheEdit"));
    m_edCache->setMaxLength(4);
    m_edBlock = new QLineEdit(pageCom);
    m_edBlock->setObjectName(QStringLiteral("blockEdit"));
    m_edBlock->setMaxLength(4);
    formCom->addRow(QStringLiteral("缓存大小(M字节)"), m_edCache);
    formCom->addRow(QStringLiteral("块大小(K字节)"), m_edBlock);
    vCom->addLayout(formCom);

    auto* note1 = new QLabel(QStringLiteral(
        "每个输入端口最大缓存字节数，该缓存将循环使用，当缓存填满后，"
        "新的数据将覆盖最早到达的数据，下一次运行时生效。"), pageCom);
    note1->setWordWrap(true);
    note1->setStyleSheet(QStringLiteral("color:#000080;"));
    vCom->addWidget(note1);
    auto* note2 = new QLabel(QStringLiteral(
        "每次从输入端口读取的字节数，根据当前可读取的数据流大小，"
        "实际读取的字节数可能小于该值。"), pageCom);
    note2->setWordWrap(true);
    note2->setStyleSheet(QStringLiteral("color:#000080;"));
    vCom->addWidget(note2);

    auto* line = new QFrame(pageCom);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vCom->addWidget(line);

    auto* hTemp = new QHBoxLayout;
    m_edTemp = new QLineEdit(pageCom);
    m_edTemp->setObjectName(QStringLiteral("tempDirEdit"));
    m_edTemp->setReadOnly(true);
    auto* btnTemp = new QPushButton(QStringLiteral("…"), pageCom);
    btnTemp->setObjectName(QStringLiteral("browseTempButton"));
    btnTemp->setFixedWidth(24);
    hTemp->addWidget(m_edTemp, 1);
    hTemp->addWidget(btnTemp);
    auto* hTempRow = new QHBoxLayout;
    hTempRow->addWidget(new QLabel(QStringLiteral("临时文件目录"), pageCom));
    hTempRow->addLayout(hTemp, 1);
    vCom->addLayout(hTempRow);

    m_freeSpace = new QLabel(pageCom);
    m_freeSpace->setObjectName(QStringLiteral("freeSpaceLabel"));
    vCom->addWidget(m_freeSpace);

    auto* note3 = new QLabel(QStringLiteral(
        "存储缓存数据临时文件的存放路径，建议选择系统中可用空间最大，"
        "速度最快的硬盘分区。下一次创建构件时生效。"), pageCom);
    note3->setWordWrap(true);
    note3->setStyleSheet(QStringLiteral("color:#000080;"));
    vCom->addWidget(note3);
    vCom->addStretch(1);
    connect(btnTemp, &QPushButton::clicked, this, &DlgParamSet::OnBrowseTemp);
    // 原版 FormCreate 仅在状态基对象非空时保留该页（否则 ComOptionPage.TabVisible=False）。
    // 独立启动（bes.exe → ShowModal）传入的状态基为 nil，故本版不显示该页。
    tabs->addTab(pageCom, QStringLiteral("构件选项"));
    tabs->setTabVisible(tabs->indexOf(pageCom), false);

    // ---------------- 按钮 ----------------
    auto* hBtn = new QHBoxLayout;
    hBtn->addStretch(1);
    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setObjectName(QStringLiteral("okButton"));
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnCancel->setObjectName(QStringLiteral("cancelButton"));
    m_btnApply = new QPushButton(QStringLiteral("应用"), this);
    m_btnApply->setObjectName(QStringLiteral("applyButton"));
    m_btnApply->setEnabled(false);
    hBtn->addWidget(btnOk);
    hBtn->addWidget(btnCancel);
    hBtn->addWidget(m_btnApply);
    vbox->addLayout(hBtn);

    connect(btnOk, &QPushButton::clicked, this, &DlgParamSet::OnOk);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnApply, &QPushButton::clicked, this, &DlgParamSet::OnApply);

    m_edCache->setText(QString::number(m_opts.cacheMB));
    m_edBlock->setText(QString::number(m_opts.blockKB));
    m_edTemp->setText(QString::fromStdWString(m_opts.tempDir));
    UpdateFreeSpace();
}

void DlgParamSet::LoadWindow(int index)
{
    if (index < 0 || index >= static_cast<int>(m_windows.size())) return;
    m_cur = index;
    m_loading = true;
    const Window& w = m_windows[index];
    const RenderParams& p = w.params;
    const FrameModel& f = w.frame;

    m_cboFormat->setCurrentIndex(ModeIndex(p.view));
    m_edStart->setText(QString::number(f.startBit));
    m_edFrameLen->setText(QString::number(f.frameLen));
    m_edStep->setText(QString::number(f.step < 1 ? 1 : f.step));
    m_chkInvert->setChecked(p.invert);
    m_chkHigh->setChecked(p.highBitFirst);

    m_edXStep->setText(QString::number(p.xStep));
    m_edYStep->setText(QString::number(p.yStep));
    m_edXTab->setText(QString::number(p.xTab));
    m_edYTab->setText(QString::number(p.yTab));
    m_chkXTab->setChecked(p.showXTab);
    m_chkYTab->setChecked(p.showYTab);
    m_chkTabFollow->setChecked(p.tabFollow);
    m_chkRuler->setChecked(p.showRuler);

    m_fromPort->setEnabled(w.hasStatusPort);
    const bool usePort = w.hasStatusPort && w.fromPort;
    if (usePort) {
        m_fromPort->setChecked(true);
    } else {
        m_fromManual->setChecked(true);
    }
    RefreshFormatEnabled();
    m_loading = false;
}

void DlgParamSet::CommitCurrent()
{
    if (m_cur < 0 || m_cur >= static_cast<int>(m_windows.size())) return;
    Window& w = m_windows[m_cur];
    RenderParams& p = w.params;
    FrameModel& f = w.frame;

    const int modeIdx = m_cboFormat->currentIndex();
    if (modeIdx != ModeIndex(p.view))
        p.view = ModeFromIndex(modeIdx);

    uint64_t start = ParseU64(m_edStart->text(), 0);
    uint64_t len = ParseU64(m_edFrameLen->text(), 64);
    if (len > kMaxFrameLen) len = kMaxFrameLen;
    uint64_t step = ParseU64(m_edStep->text(), 1);
    if (step < 1) step = 1;
    // 帧长/步进（行数）超出 32 位有符号范围时，步进复位为 1
    if (len / step > static_cast<uint64_t>(0x7FFFFFFF))
        step = 1;

    int xStep = ParseIntDef(m_edXStep->text(), 1);
    int yStep = ParseIntDef(m_edYStep->text(), 1);
    int xTab = ParseIntDef(m_edXTab->text(), 8);
    int yTab = ParseIntDef(m_edYTab->text(), 8);
    if (xStep < 1) xStep = 1;
    if (yStep < 1) yStep = 1;
    if (xTab < 2) xTab = 2;
    if (yTab < 2) yTab = 2;

    f.startBit = start;
    f.frameLen = len;
    f.step = step;
    p.xStep = xStep;
    p.yStep = yStep;
    p.xTab = xTab;
    p.yTab = yTab;
    p.highBitFirst = m_chkHigh->isChecked();
    p.invert = m_chkInvert->isChecked();
    p.showXTab = m_chkXTab->isChecked();
    p.showYTab = m_chkYTab->isChecked();
    p.tabFollow = m_chkTabFollow->isChecked();
    p.showRuler = m_chkRuler->isChecked();
    w.fromPort = w.hasStatusPort && m_fromPort->isChecked();

    // 原版 EditExit 会把修正后的值写回输入框
    m_loading = true;
    m_edStart->setText(QString::number(start));
    m_edFrameLen->setText(QString::number(len));
    m_edStep->setText(QString::number(step));
    m_edXStep->setText(QString::number(xStep));
    m_edYStep->setText(QString::number(yStep));
    m_edXTab->setText(QString::number(xTab));
    m_edYTab->setText(QString::number(yTab));
    m_loading = false;
}

void DlgParamSet::RefreshFormatEnabled()
{
    if (m_formatGroup && m_fromManual)
        m_formatGroup->setEnabled(m_fromManual->isChecked());
}

void DlgParamSet::MarkDirty()
{
    if (m_loading) return;
    m_dirty = true;
    if (m_btnApply) m_btnApply->setEnabled(true);
}

void DlgParamSet::ValidateOptions()
{
    m_loading = true;
    int cache = ParseIntDef(m_edCache->text(), 64); // 非法输入按 64
    if (cache == 0) cache = 1;                      // 原版 0 -> 1
    if (cache < 1) cache = 1;
    if (cache > 1024) cache = 1024;
    m_edCache->setText(QString::number(cache));

    int block = ParseIntDef(m_edBlock->text(), 640); // 非法输入按 640
    const int maxBlock = std::min(cache * 1024, 4096);
    if (block < 32) block = 32;
    if (block > maxBlock) block = maxBlock;
    m_edBlock->setText(QString::number(block));
    m_loading = false;

    m_opts.cacheMB = cache;
    m_opts.blockKB = block;
    m_opts.tempDir = m_edTemp->text().toStdWString();
}

void DlgParamSet::OnWindowChanged(int row)
{
    CommitCurrent();
    LoadWindow(row);
}

void DlgParamSet::OnApply()
{
    CommitCurrent();
    ValidateOptions();
    m_dirty = false;
    m_btnApply->setEnabled(false);
    LoadWindow(m_cur);
    emit Applied();
}

void DlgParamSet::OnOk()
{
    CommitCurrent();
    ValidateOptions();
    if (m_dirty) {
        m_dirty = false;
        m_btnApply->setEnabled(false);
        emit Applied();
    }
    accept();
}

bool DlgParamSet::TestWritable(const QString& dir)
{
    const QString path = dir + QStringLiteral("\\") +
                         QString::number(QDateTime::currentMSecsSinceEpoch());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write("BES");
    f.close();
    f.remove();
    return true;
}

void DlgParamSet::OnBrowseTemp()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择临时目录"), m_edTemp->text());
    if (dir.isEmpty())
        return;
    if (!TestWritable(dir)) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QStringLiteral("目录不可写!"));
        return;
    }
    m_edTemp->setText(dir);
    UpdateFreeSpace();
    MarkDirty();
}

void DlgParamSet::UpdateFreeSpace()
{
    if (!m_freeSpace) return;
    const QString dir = m_edTemp->text().trimmed();
    if (dir.isEmpty()) {
        m_freeSpace->clear();
        return;
    }
    QStorageInfo si(dir);
    if (!si.isValid() || !si.isReady()) {
        m_freeSpace->clear();
        return;
    }
    const qint64 mb = si.bytesAvailable() / (1024 * 1024);
    m_freeSpace->setText(QStringLiteral("可用空间：%1M字节").arg(mb));
}

uint32_t DlgParamSet::ColorAt(int i) const
{
    if (i < 0 || i >= 9) return 0;
    return m_colors.*(kColorSlots[i].field);
}

void DlgParamSet::SetColorAt(int i, uint32_t c)
{
    if (i < 0 || i >= 9) return;
    m_colors.*(kColorSlots[i].field) = c;
}

void DlgParamSet::RefreshColorButtons()
{
    for (int i = 0; i < 9; ++i) {
        if (!m_colorBtn[i]) continue;
        const QColor c = ToQColor(ColorAt(i));
        m_colorBtn[i]->setStyleSheet(
            QStringLiteral("background-color: rgb(%1,%2,%3); border:1px solid #666;")
                .arg(c.red()).arg(c.green()).arg(c.blue()));
        m_colorBtn[i]->setText(c.name().toUpper());
    }
}

void DlgParamSet::PickColor(int index)
{
    const QColor c = QColorDialog::getColor(ToQColor(ColorAt(index)), this,
                                            QStringLiteral("选择颜色"));
    if (c.isValid()) {
        SetColorAt(index, FromQColor(c));
        RefreshColorButtons();
        MarkDirty();
    }
}

void DlgParamSet::DefaultScheme()
{
    // 原版「默认方案」颜色（0x00BBGGRR）
    m_colors.body       = 0x00FF0080u;
    m_colors.background = 0x00FFFFFFu;
    m_colors.coord      = 0x00000000u;
    m_colors.ruler      = 0x00000000u;
    m_colors.tab        = 0x000000FFu;
    m_colors.marker     = 0x0000FFFFu;
    m_colors.bookmark   = 0x00FF0000u;
    m_colors.selection  = 0x00800000u;
    m_colors.errFrame   = 0x00808080u;
    RefreshColorButtons();
    MarkDirty();
}

void DlgParamSet::SchemeOne()
{
    // 原版「方案一」颜色（0x00BBGGRR）
    m_colors.body       = 0x00FFFFFFu;
    m_colors.background = 0x00840000u;
    m_colors.coord      = 0x0000FF00u;
    m_colors.ruler      = 0x0000FF00u;
    m_colors.tab        = 0x000000FFu;
    m_colors.marker     = 0x0000FFFFu;
    m_colors.bookmark   = 0x00000084u;
    m_colors.selection  = 0x00FF0000u;
    m_colors.errFrame   = 0x00008080u;
    RefreshColorButtons();
    MarkDirty();
}
