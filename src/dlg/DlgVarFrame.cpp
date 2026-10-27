// DlgVarFrame.cpp
#include "dlg/DlgVarFrame.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "core/SearchEngine.h"

using namespace bes;

namespace {
const char* kPresetKey = "varframe/presets";

QString FormatPlaceholder(UniqueCodeFormat f)
{
    switch (f) {
    case UniqueCodeFormat::Binary: return QStringLiteral("二进制 0101...，可带空格");
    case UniqueCodeFormat::Hex:    return QStringLiteral("十六进制，可带空格，例如 1ACFFC1D");
    default:                       return QStringLiteral("字符方式（按字节）");
    }
}
} // namespace

DlgVarFrame::DlgVarFrame(QWidget* parent, const FileMap& map,
                         const FrameModel& frame, bool processMode)
    : QDialog(parent), m_map(&map), m_frame(frame), m_processMode(processMode)
{
    setWindowTitle(processMode ? QStringLiteral("不等长帧处理")
                               : QStringLiteral("不等长帧查看"));
    resize(760, 620);

    auto* vbox = new QVBoxLayout(this);

    // ------------------------------------------------ 处理参数
    m_paramGroup = new QGroupBox(QStringLiteral("处理参数"), this);
    auto* grid = new QGridLayout(m_paramGroup);

    grid->addWidget(new QLabel(QStringLiteral("预置"), m_paramGroup), 0, 0);
    m_edPresetName = new QLineEdit(m_paramGroup);
    m_edPresetName->setMaxLength(12);
    m_edPresetName->setPlaceholderText(QStringLiteral("名称"));
    grid->addWidget(m_edPresetName, 0, 1);
    auto* hPre = new QHBoxLayout;
    m_btnPresetAdd = new QPushButton(QStringLiteral("添加"), m_paramGroup);
    m_btnPresetDel = new QPushButton(QStringLiteral("删除"), m_paramGroup);
    hPre->addWidget(m_btnPresetAdd);
    hPre->addWidget(m_btnPresetDel);
    grid->addLayout(hPre, 0, 2);

    m_presets = new QListWidget(m_paramGroup);
    m_presets->setFixedWidth(180);
    grid->addWidget(m_presets, 1, 0, 4, 3);

    grid->addWidget(new QLabel(QStringLiteral("前导码"), m_paramGroup), 0, 3);
    m_edPrecode = new QLineEdit(m_paramGroup);
    m_edPrecode->setObjectName(QStringLiteral("precodeEdit"));
    m_edPrecode->setPlaceholderText(FormatPlaceholder(UniqueCodeFormat::Hex));
    grid->addWidget(m_edPrecode, 0, 4, 1, 3);

    grid->addWidget(new QLabel(QStringLiteral("格式"), m_paramGroup), 1, 3);
    m_cboFormat = new QComboBox(m_paramGroup);
    m_cboFormat->setObjectName(QStringLiteral("formatCombo"));
    m_cboFormat->addItems({ QStringLiteral("二进制"),
                            QStringLiteral("十六进制"),
                            QStringLiteral("字符") });
    m_cboFormat->setCurrentIndex(1);
    grid->addWidget(m_cboFormat, 1, 4, 1, 3);

    grid->addWidget(new QLabel(QStringLiteral("允许误码比特数"), m_paramGroup), 2, 3);
    m_edErr = new QLineEdit(QStringLiteral("0"), m_paramGroup);
    m_edErr->setObjectName(QStringLiteral("errorEdit"));
    m_edErr->setFixedWidth(56);
    grid->addWidget(m_edErr, 2, 4);
    m_chkAmb = new QCheckBox(QStringLiteral("去除四相模糊"), m_paramGroup);
    m_chkAmb->setObjectName(QStringLiteral("ambiguityCheck"));
    grid->addWidget(m_chkAmb, 2, 5, 1, 2);

    grid->addWidget(new QLabel(QStringLiteral("输出文件"), m_paramGroup), 3, 3);
    m_edOutFile = new QLineEdit(m_paramGroup);
    m_edOutFile->setObjectName(QStringLiteral("outputEdit"));
    m_edOutFile->setPlaceholderText(QStringLiteral("*.bnt（可留空，仅查看）"));
    grid->addWidget(m_edOutFile, 3, 4, 1, 2);
    auto* btnBrowse = new QPushButton(QStringLiteral("…"), m_paramGroup);
    btnBrowse->setFixedWidth(28);
    grid->addWidget(btnBrowse, 3, 6);

    m_btnStart = new QPushButton(QStringLiteral("开始(&S)"), this);
    m_btnStart->setObjectName(QStringLiteral("startButton"));
    m_btnStop  = new QPushButton(QStringLiteral("中止(&T)"), this);
    m_btnStop->setEnabled(false);
    m_prog = new QProgressBar(this);
    m_prog->setRange(0, 1000);
    m_prog->setValue(0);
    m_prog->setVisible(false);
    m_info = new QLabel(this);
    grid->setColumnStretch(4, 1);
    grid->setColumnStretch(5, 1);
    vbox->addWidget(m_paramGroup);

    auto* hRun = new QHBoxLayout;
    hRun->addWidget(m_btnStart);
    hRun->addWidget(m_btnStop);
    hRun->addWidget(m_prog, 1);
    hRun->addWidget(m_info, 1);
    vbox->addLayout(hRun);

    // ------------------------------------------------ 帧表
    m_frameGroup = new QGroupBox(QStringLiteral("帧标记"), this);
    auto* fv = new QVBoxLayout(m_frameGroup);

    auto* hLen = new QHBoxLayout;
    hLen->addWidget(new QLabel(QStringLiteral("默认帧长(比特)"), m_frameGroup));
    m_edFrameLen = new QLineEdit(m_frameGroup);
    m_edFrameLen->setText(QString::number(frame.frameLen ? frame.frameLen : 64));
    m_edFrameLen->setFixedWidth(90);
    hLen->addWidget(m_edFrameLen);
    auto* btnSplit = new QPushButton(QStringLiteral("按帧长切分"), m_frameGroup);
    hLen->addWidget(btnSplit);
    auto* btnLoadFrames = new QPushButton(QStringLiteral("读入帧表…"), m_frameGroup);
    btnLoadFrames->setObjectName(QStringLiteral("loadFramesButton"));
    hLen->addWidget(btnLoadFrames);
    hLen->addStretch(1);
    m_btnInsert   = new QPushButton(QStringLiteral("插入帧标记(&I)"), m_frameGroup);
    m_btnDelete   = new QPushButton(QStringLiteral("删除帧标记(&D)"), m_frameGroup);
    m_btnSetStart = new QPushButton(QStringLiteral("设为起点(&S)"), m_frameGroup);
    m_btnLocate   = new QPushButton(QStringLiteral("定位(&L)"), m_frameGroup);
    hLen->addWidget(m_btnInsert);
    hLen->addWidget(m_btnDelete);
    hLen->addWidget(m_btnSetStart);
    hLen->addWidget(m_btnLocate);
    fv->addLayout(hLen);

    m_table = new QTableWidget(m_frameGroup);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("序号"), QStringLiteral("起始比特"), QStringLiteral("长度(比特)") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked);
    fv->addWidget(m_table, 1);
    vbox->addWidget(m_frameGroup, 1);

    auto* hOk = new QHBoxLayout;
    m_btnApply = new QPushButton(QStringLiteral("应用"), this);
    m_btnOk = new QPushButton(QStringLiteral("确定"), this);
    m_btnOk->setDefault(true);
    m_btnCancel = new QPushButton(QStringLiteral("取消"), this);
    hOk->addWidget(m_btnApply);
    hOk->addStretch(1);
    hOk->addWidget(m_btnOk);
    hOk->addWidget(m_btnCancel);
    vbox->addLayout(hOk);

    connect(btnSplit, &QPushButton::clicked, this, &DlgVarFrame::OnSplit);
    connect(btnLoadFrames, &QPushButton::clicked, this, &DlgVarFrame::OnLoadFrames);
    connect(btnBrowse, &QPushButton::clicked, this, &DlgVarFrame::OnBrowse);
    connect(m_btnInsert, &QPushButton::clicked, this, &DlgVarFrame::OnInsert);
    connect(m_btnDelete, &QPushButton::clicked, this, &DlgVarFrame::OnDelete);
    connect(m_btnSetStart, &QPushButton::clicked, this, &DlgVarFrame::OnSetStart);
    connect(m_btnLocate, &QPushButton::clicked, this, &DlgVarFrame::OnLocate);
    connect(m_btnStart, &QPushButton::clicked, this, &DlgVarFrame::OnStart);
    connect(m_btnStop, &QPushButton::clicked, this, &DlgVarFrame::OnStop);
    connect(m_btnPresetAdd, &QPushButton::clicked, this, &DlgVarFrame::OnPresetAdd);
    connect(m_btnPresetDel, &QPushButton::clicked, this, &DlgVarFrame::OnPresetDelete);
    connect(m_presets, &QListWidget::itemClicked, this, &DlgVarFrame::OnPresetClicked);
    connect(m_cboFormat, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &DlgVarFrame::OnFormatChanged);
    connect(m_edPrecode, &QLineEdit::textChanged, this, &DlgVarFrame::UpdateStartEnabled);
    connect(m_edErr, &QLineEdit::textChanged, this, &DlgVarFrame::UpdateStartEnabled);
    connect(m_edOutFile, &QLineEdit::textChanged, this, &DlgVarFrame::UpdateStartEnabled);
    connect(m_chkAmb, &QCheckBox::toggled, this, &DlgVarFrame::UpdateStartEnabled);
    connect(m_btnApply, &QPushButton::clicked, this, &DlgVarFrame::OnApply);
    connect(m_btnOk, &QPushButton::clicked, this, &DlgVarFrame::OnOk);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it){
        if (!it || it->column() != 2) return;
        const int row = it->row();
        auto& fs = m_frames.Frames();
        if (row < 0 || row >= static_cast<int>(fs.size())) return;
        const uint64_t len = std::max<uint64_t>(1, it->text().toULongLong());
        fs[static_cast<size_t>(row)].length = len;
        uint64_t pos = fs[static_cast<size_t>(row)].startBit + len;
        for (size_t i = static_cast<size_t>(row) + 1; i < fs.size(); ++i) {
            fs[i].startBit = pos;
            pos += fs[i].length;
        }
        QTimer::singleShot(0, this, [this]{ Reload(); });
    });

    LoadPresets();
    RebuildUniform(frame.frameLen ? frame.frameLen : 64);
    UpdateStartEnabled();
}

void DlgVarFrame::closeEvent(QCloseEvent* ev)
{
    if (m_running) {
        m_stop = true;
        ev->ignore();
        return;
    }
    QDialog::closeEvent(ev);
}

bool DlgVarFrame::ParseCode(std::vector<uint8_t>& out, QString* err) const
{
    out.clear();
    QString text = m_edPrecode->text();
    text.remove(QLatin1Char(' '));
    const int fmt = m_cboFormat->currentIndex();

    if (fmt == 0) {
        for (const QChar& c : text) {
            if (c != QLatin1Char('0') && c != QLatin1Char('1')) {
                if (err) *err = QStringLiteral("前导码输入中有非法字符！");
                return false;
            }
        }
        int n = text.size();
        if (n == 0) {
            if (err) *err = QStringLiteral("前导码长度不能为0！");
            return false;
        }
        if (n % 8) n = ((n / 8) + 1) * 8;
        for (int i = 0; i < n; ++i) {
            const QChar c = (i < text.size()) ? QChar(text[i]) : QChar(QLatin1Char('0'));
            if (i % 8 == 0) out.push_back(0);
            out.back() = static_cast<uint8_t>((out.back() << 1) |
                          (c == QLatin1Char('1') ? 1 : 0));
        }
        return true;
    }

    if (fmt == 1) {
        QString t;
        for (const QChar& c : text) {
            if (!c.isDigit() &&
                !QStringLiteral("abcdefABCDEF").contains(c, Qt::CaseInsensitive)) {
                if (err) *err = QStringLiteral("前导码输入中有非法字符！");
                return false;
            }
            t += c;
        }
        if (t.isEmpty()) {
            if (err) *err = QStringLiteral("前导码长度不能为0！");
            return false;
        }
        if (t.size() % 2) t.prepend(QLatin1Char('0'));
        for (int i = 0; i + 1 < t.size(); i += 2)
            out.push_back(static_cast<uint8_t>(t.mid(i, 2).toUInt(nullptr, 16)));
        return true;
    }

    for (const QChar& c : text)
        out.push_back(static_cast<uint8_t>(c.unicode() & 0xFF));
    if (out.empty()) {
        if (err) *err = QStringLiteral("前导码长度不能为0！");
        return false;
    }
    return true;
}

void DlgVarFrame::UpdateStartEnabled()
{
    bool ok = !m_running && m_map && m_map->IsOpen() &&
              !m_edPrecode->text().trimmed().isEmpty();
    if (ok) {
        std::vector<uint8_t> code;
        ok = ParseCode(code, nullptr);
    }
    if (m_processMode)
        ok = ok && !m_edOutFile->text().trimmed().isEmpty();
    m_btnStart->setEnabled(ok);
}

void DlgVarFrame::SetRunning(bool running)
{
    m_running = running;
    m_paramGroup->setEnabled(!running);
    m_frameGroup->setEnabled(!running);
    m_btnApply->setEnabled(!running);
    m_btnOk->setEnabled(!running);
    m_btnCancel->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_prog->setVisible(running);
    if (running) {
        m_prog->setRange(0, 1000);
        m_prog->setValue(0);
        m_info->setText(QStringLiteral("正在扫描…"));
    }
    UpdateStartEnabled();
    qApp->processEvents();
}

void DlgVarFrame::OnFormatChanged()
{
    const auto f = static_cast<UniqueCodeFormat>(std::max(0, m_cboFormat->currentIndex()));
    m_edPrecode->setPlaceholderText(FormatPlaceholder(f));
    UpdateStartEnabled();
}

void DlgVarFrame::RebuildUniform(uint64_t lenBits)
{
    m_frames.Clear();
    if (!m_map || !m_map->IsOpen()) { Reload(); return; }
    if (lenBits == 0) lenBits = 64;

    const uint64_t total = m_map->SizeBits();
    // 防止把超大文件切成上百万行导致界面卡死
    const uint64_t maxRows = 200000;
    if (total / lenBits > maxRows)
        lenBits = ((total + maxRows - 1) / maxRows + 7) / 8 * 8;
    m_edFrameLen->setText(QString::number(lenBits));

    for (uint64_t pos = 0; pos < total; pos += lenBits)
        m_frames.Add(pos, std::min<uint64_t>(lenBits, total - pos));
    Reload();
}

void DlgVarFrame::Reload()
{
    const QSignalBlocker b(m_table);
    const auto& fs = m_frames.Frames();
    m_table->setRowCount(static_cast<int>(fs.size()));
    for (size_t i = 0; i < fs.size(); ++i) {
        m_table->setItem(static_cast<int>(i), 0,
                         new QTableWidgetItem(QString::number(i)));
        m_table->setItem(static_cast<int>(i), 1,
                         new QTableWidgetItem(QString::number(fs[i].startBit)));
        m_table->setItem(static_cast<int>(i), 2,
                         new QTableWidgetItem(QString::number(fs[i].length)));
    }
}

void DlgVarFrame::OnSplit()
{
    RebuildUniform(m_edFrameLen->text().toULongLong());
}

void DlgVarFrame::OnInsert()
{
    int row = m_table->currentRow();
    if (row < 0) row = m_table->rowCount();
    const auto& old = m_frames.Frames();
    uint64_t start = 0, len = m_edFrameLen->text().toULongLong();
    if (len == 0) len = 64;
    if (row > 0 && row - 1 < static_cast<int>(old.size())) {
        start = old[static_cast<size_t>(row - 1)].startBit + old[static_cast<size_t>(row - 1)].length;
        len = old[static_cast<size_t>(row - 1)].length;
    } else if (!old.empty()) {
        start = old.back().startBit + old.back().length;
        len = old.back().length;
    }

    VariableFrameSet ns;
    for (int i = 0; i < row && i < static_cast<int>(old.size()); ++i)
        ns.Add(old[static_cast<size_t>(i)].startBit, old[static_cast<size_t>(i)].length);
    ns.Add(start, len);
    for (size_t i = static_cast<size_t>(row); i < old.size(); ++i)
        ns.Add(old[i].startBit, old[i].length);
    m_frames = ns;
    Reload();
    if (row < m_table->rowCount()) m_table->selectRow(row);
}

void DlgVarFrame::OnDelete()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const auto& old = m_frames.Frames();
    if (row >= static_cast<int>(old.size())) return;

    VariableFrameSet ns;
    for (size_t i = 0; i < old.size(); ++i)
        if (static_cast<int>(i) != row)
            ns.Add(old[i].startBit, old[i].length);
    m_frames = ns;
    Reload();
}

void DlgVarFrame::OnSetStart()
{
    const int row = m_table->currentRow();
    const auto& fs = m_frames.Frames();
    if (row < 0 || row >= static_cast<int>(fs.size())) return;
    m_frame.startBit = fs[static_cast<size_t>(row)].startBit;
    m_edFrameLen->setText(QString::number(fs[static_cast<size_t>(row)].length));
}

void DlgVarFrame::OnLocate()
{
    const int row = m_table->currentRow();
    const auto& fs = m_frames.Frames();
    if (row < 0 || row >= static_cast<int>(fs.size())) return;
    m_frame.startBit = fs[static_cast<size_t>(row)].startBit;
}

bool DlgVarFrame::LoadFrameFile(const QString& path, QString* err)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (err) *err = QStringLiteral("无法打开文件：%1").arg(file.errorString());
        return false;
    }

    QString fmtText, precodeText;
    int errBits = 0;
    bool amb = false;
    VariableFrameSet ns;
    bool headerSeen = false;

    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty()) continue;
        if (!headerSeen) {
            if (!line.startsWith(QStringLiteral("BES-BNT"))) {
                if (err) *err = QStringLiteral("不是有效的 BNT 帧表文件。");
                return false;
            }
            headerSeen = true;
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (key.compare(QStringLiteral("Format"), Qt::CaseInsensitive) == 0) {
            fmtText = val;
        } else if (key.compare(QStringLiteral("Precode"), Qt::CaseInsensitive) == 0) {
            precodeText = val;
        } else if (key.compare(QStringLiteral("ErrorBits"), Qt::CaseInsensitive) == 0) {
            errBits = std::max(0, val.toInt());
        } else if (key.compare(QStringLiteral("RemoveAmbiguity"), Qt::CaseInsensitive) == 0) {
            amb = val.toInt() != 0;
        } else if (key.compare(QStringLiteral("Frame"), Qt::CaseInsensitive) == 0) {
            const int comma = val.indexOf(QLatin1Char(','));
            if (comma <= 0) continue;
            const uint64_t start = val.left(comma).trimmed().toULongLong();
            const uint64_t len = val.mid(comma + 1).trimmed().toULongLong();
            if (len == 0) continue;
            ns.Add(start, len);
        }
    }
    file.close();

    if (!headerSeen) {
        if (err) *err = QStringLiteral("不是有效的 BNT 帧表文件。");
        return false;
    }
    if (ns.Size() == 0) {
        if (err) *err = QStringLiteral("帧表中没有帧数据。");
        return false;
    }

    // 超出当前数据范围的帧丢弃，末尾帧按文件长度截断
    if (m_map && m_map->IsOpen()) {
        const uint64_t totalBits = m_map->SizeBits();
        std::vector<VariableFrame> kept;
        for (const VariableFrame& f : ns.Frames()) {
            if (f.startBit >= totalBits) continue;
            const uint64_t len = std::min<uint64_t>(f.length, totalBits - f.startBit);
            if (len == 0) continue;
            kept.push_back(VariableFrame{ f.startBit, len });
        }
        ns.Clear();
        for (const VariableFrame& f : kept) ns.Add(f.startBit, f.length);
        if (ns.Size() == 0) {
            if (err) *err = QStringLiteral("帧表内容与当前数据不匹配（起始位置超出文件）。");
            return false;
        }
    }

    m_frames = ns;
    if (!precodeText.isEmpty()) m_edPrecode->setText(precodeText);
    if (fmtText.contains(QStringLiteral("二进制")))
        m_cboFormat->setCurrentIndex(0);
    else if (fmtText.contains(QStringLiteral("字符")))
        m_cboFormat->setCurrentIndex(2);
    else if (fmtText.contains(QStringLiteral("十六进制")) || !fmtText.isEmpty())
        m_cboFormat->setCurrentIndex(1);
    m_edErr->setText(QString::number(errBits));
    m_chkAmb->setChecked(amb);

    const VariableFrame& first = m_frames.Frames().front();
    m_frame.startBit = first.startBit;
    m_edFrameLen->setText(QString::number(first.length));
    Reload();
    m_table->selectRow(0);
    m_info->setText(QStringLiteral("已读入 %1 帧（%2）")
                        .arg(static_cast<qulonglong>(m_frames.Size()))
                        .arg(QFileInfo(path).fileName()));
    return true;
}

void DlgVarFrame::OnLoadFrames()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("读入帧表"), QString(),
        QStringLiteral("不等长帧数据文件(*.bnt);;所有文件(*.*)"));
    if (path.isEmpty()) return;
    QString err;
    if (!LoadFrameFile(path, &err))
        QMessageBox::warning(this, windowTitle(), err);
}

void DlgVarFrame::OnBrowse()
{
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("输出文件"), m_edOutFile->text(),
        QStringLiteral("不等长帧数据文件(*.bnt);;所有文件(*.*)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".bnt");
    m_edOutFile->setText(path);
}

void DlgVarFrame::OnStart()
{
    if (m_running) return;
    std::vector<uint8_t> code;
    QString err;
    if (!ParseCode(code, &err)) {
        QMessageBox::warning(this, windowTitle(), err);
        return;
    }

    UniqueCodeSpec spec;
    spec.code = code;
    spec.errorBits = std::max(0, m_edErr->text().toInt());
    spec.removeAmbiguity = m_chkAmb->isChecked();
    std::wstring werr;
    if (!spec.Validate(&werr)) {
        QMessageBox::warning(this, windowTitle(), QString::fromStdWString(werr));
        return;
    }

    if (m_processMode && m_edOutFile->text().trimmed().isEmpty()) {
        OnBrowse();
        if (m_edOutFile->text().trimmed().isEmpty()) return;
    }

    m_stop = false;
    SetRunning(true);

    SearchEngine eng;
    std::vector<SearchHit> hits;
    const bool finished = eng.FindUniqueCode(*m_map, spec, 0, false, hits,
        [this](uint64_t done, uint64_t total, bool fin) {
            if (!fin && total > 0)
                m_prog->setValue(static_cast<int>(done * 1000 / total));
            if (!fin)
                QCoreApplication::processEvents();
        }, m_stop);

    SetRunning(false);
    m_prog->setValue(finished ? 1000 : m_prog->value());

    if (!finished) {
        m_info->setText(QStringLiteral("已中止"));
        return;
    }
    if (hits.empty()) {
        m_info->setText(QStringLiteral("未找到前导码"));
        QMessageBox::information(this, windowTitle(),
                                 QStringLiteral("没有找到前导码。"));
        return;
    }

    // 帧起点：相邻前导码之间的数据为一帧，重叠的命中点忽略
    const uint64_t totalBits = m_map->SizeBits();
    const uint64_t minGap = static_cast<uint64_t>(spec.lengthBits);
    std::vector<uint64_t> starts;
    starts.reserve(std::min<size_t>(hits.size(), 200000));
    for (const SearchHit& h : hits) {
        if (starts.size() >= 200000) break;
        if (starts.empty() || h.bitPos - starts.back() >= minGap)
            starts.push_back(h.bitPos);
    }

    VariableFrameSet ns;
    for (size_t i = 0; i < starts.size(); ++i) {
        const uint64_t start = starts[i];
        const uint64_t end = (i + 1 < starts.size()) ? starts[i + 1] : totalBits;
        if (end <= start) continue;
        ns.Add(start, end - start);
    }
    m_frames = ns;
    Reload();

    QString msg = QStringLiteral("共找到 %1 个前导码，生成 %2 帧")
                      .arg(static_cast<qulonglong>(hits.size()))
                      .arg(static_cast<qulonglong>(m_frames.Size()));
    const QString outPath = m_edOutFile->text().trimmed();
    if (!outPath.isEmpty()) {
        QString writeErr;
        if (WriteOutput(outPath, m_edPrecode->text(), starts, &writeErr))
            msg += QStringLiteral("，已写入 %1").arg(QFileInfo(outPath).fileName());
        else
            msg += QStringLiteral("；输出失败：%1").arg(writeErr);
    }
    m_info->setText(msg);
}

void DlgVarFrame::OnStop()
{
    m_stop = true;
    m_info->setText(QStringLiteral("正在中止…"));
}

void DlgVarFrame::OnPresetClicked()
{
    QListWidgetItem* it = m_presets->currentItem();
    if (!it) return;
    m_edPresetName->setText(it->text());
    m_edPrecode->setText(it->data(Qt::UserRole).toString());
    UpdateStartEnabled();
}

void DlgVarFrame::OnPresetAdd()
{
    const QString code = m_edPrecode->text().trimmed();
    if (code.isEmpty()) return;
    QString name = m_edPresetName->text().trimmed();
    if (name.isEmpty()) name = code.left(12);

    for (int i = 0; i < m_presets->count(); ++i) {
        if (m_presets->item(i)->text() == name) {
            m_presets->item(i)->setData(Qt::UserRole, code);
            SavePresets();
            return;
        }
    }
    auto* it = new QListWidgetItem(name, m_presets);
    it->setData(Qt::UserRole, code);
    SavePresets();
}

void DlgVarFrame::OnPresetDelete()
{
    const int row = m_presets->currentRow();
    if (row < 0) return;
    delete m_presets->takeItem(row);
    SavePresets();
}

void DlgVarFrame::LoadPresets()
{
    QSettings st;
    const QStringList list = st.value(QLatin1String(kPresetKey)).toStringList();
    for (const QString& item : list) {
        const int tab = item.indexOf(QLatin1Char('\t'));
        if (tab <= 0) continue;
        auto* it = new QListWidgetItem(item.left(tab), m_presets);
        it->setData(Qt::UserRole, item.mid(tab + 1));
    }
}

void DlgVarFrame::SavePresets()
{
    QStringList list;
    for (int i = 0; i < m_presets->count(); ++i) {
        QListWidgetItem* it = m_presets->item(i);
        list << it->text() + QLatin1Char('\t') + it->data(Qt::UserRole).toString();
    }
    QSettings st;
    st.setValue(QLatin1String(kPresetKey), list);
}

bool DlgVarFrame::WriteOutput(const QString& path, const QString& codeText,
                              const std::vector<uint64_t>& starts, QString* err) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (err) *err = file.errorString();
        return false;
    }
    QTextStream ts(&file);
    ts.setCodec("UTF-8");
    ts << "BES-BNT 1.0\n";
    ts << "Format=" << m_cboFormat->currentText() << "\n";
    ts << "Precode=" << codeText.trimmed() << "\n";
    ts << "ErrorBits=" << m_edErr->text().toInt() << "\n";
    ts << "RemoveAmbiguity=" << (m_chkAmb->isChecked() ? 1 : 0) << "\n";
    ts << "FrameCount=" << static_cast<qulonglong>(starts.size()) << "\n";
    const uint64_t totalBits = m_map ? m_map->SizeBits() : 0;
    for (size_t i = 0; i < starts.size(); ++i) {
        const uint64_t start = starts[i];
        const uint64_t end = (i + 1 < starts.size()) ? starts[i + 1] : totalBits;
        if (end <= start) continue;
        ts << "Frame=" << static_cast<qulonglong>(start)
           << "," << static_cast<qulonglong>(end - start) << "\n";
    }
    file.close();
    return true;
}

void DlgVarFrame::OnApply()
{
    if (m_running) return;
    m_applied = true;
    m_info->setText(QStringLiteral("已应用不等长帧，共 %1 帧")
                        .arg(static_cast<qulonglong>(m_frames.Size())));
}

void DlgVarFrame::OnOk()
{
    if (m_running) return;
    m_applied = true;
    accept();
}
