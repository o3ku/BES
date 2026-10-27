// DlgSearchUnique.cpp
#include "dlg/DlgSearchUnique.h"
#include "dlg/DlgProgress.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace bes;

DlgSearchUnique::DlgSearchUnique(QWidget* parent, const FileMap& map)
    : QDialog(parent), m_map(&map)
{
    setWindowTitle(QStringLiteral("查找独特码"));
    resize(460, 360);

    auto* vbox = new QVBoxLayout(this);

    auto* gbFmt = new QGroupBox(QStringLiteral("格式选择"), this);
    auto* hFmt = new QHBoxLayout(gbFmt);
    m_rbBin = new QRadioButton(QStringLiteral("二进制(&B)"), gbFmt);
    m_rbHex = new QRadioButton(QStringLiteral("十六进制(&H)"), gbFmt);
    m_rbChar = new QRadioButton(QStringLiteral("字符(&C)"), gbFmt);
    m_rbHex->setChecked(true);
    auto* bg = new QButtonGroup(this);
    bg->addButton(m_rbBin);
    bg->addButton(m_rbHex);
    bg->addButton(m_rbChar);
    hFmt->addWidget(m_rbBin);
    hFmt->addWidget(m_rbHex);
    hFmt->addWidget(m_rbChar);
    vbox->addWidget(gbFmt);

    auto* gbCode = new QGroupBox(QStringLiteral("独特码"), this);
    auto* gCode = new QGridLayout(gbCode);
    m_edCode = new QLineEdit(gbCode);
    m_edCode->setPlaceholderText(QStringLiteral("十六进制，可带空格；长度不限"));
    gCode->addWidget(m_edCode, 0, 0, 1, 5);
    auto* lbErr = new QLabel(QStringLiteral("允许误码比特数"), gbCode);
    m_edErr = new QLineEdit(QStringLiteral("0"), gbCode);
    m_chkAmb = new QCheckBox(QStringLiteral("去除四相模糊"), gbCode);
    gCode->addWidget(lbErr, 1, 0);
    gCode->addWidget(m_edErr, 1, 1);
    gCode->addWidget(m_chkAmb, 1, 2, 1, 2);
    vbox->addWidget(gbCode);

    m_lblProgress = new QLabel(this);
    vbox->addWidget(m_lblProgress);

    m_edHits = new QPlainTextEdit(this);
    m_edHits->setReadOnly(true);
    vbox->addWidget(m_edHits, 1);

    auto* hBtn = new QHBoxLayout;
    m_btnFind = new QPushButton(QStringLiteral("查找"), this);
    m_btnFind->setEnabled(false);
    auto* btnClose = new QPushButton(QStringLiteral("关闭"), this);
    m_btnFind->setDefault(true);
    hBtn->addStretch(1);
    hBtn->addWidget(m_btnFind);
    hBtn->addWidget(btnClose);
    vbox->addLayout(hBtn);

    connect(bg, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, &DlgSearchUnique::OnFormatChanged);
    connect(m_edCode, &QLineEdit::textChanged, this, &DlgSearchUnique::OnCodeChanged);
    connect(m_btnFind, &QPushButton::clicked, this, &DlgSearchUnique::OnFind);
    connect(btnClose, &QPushButton::clicked, this, [this]{
        m_stop = true;
        reject();
    });

    OnFormatChanged();
}

void DlgSearchUnique::OnFormatChanged()
{
    if (m_rbBin->isChecked())      m_format = UniqueCodeFormat::Binary;
    else if (m_rbHex->isChecked()) m_format = UniqueCodeFormat::Hex;
    else                           m_format = UniqueCodeFormat::Char;
    m_edCode->setPlaceholderText(
        m_format == UniqueCodeFormat::Binary ? QStringLiteral("二进制 0101...，可带空格") :
        m_format == UniqueCodeFormat::Hex    ? QStringLiteral("十六进制，可带空格；长度不限") :
                                               QStringLiteral("字符方式（按字节）"));
    OnCodeChanged();
}

void DlgSearchUnique::OnCodeChanged()
{
    UniqueCodeSpec spec;
    QString err;
    const bool ok = ParseCode(spec.code, err);
    m_btnFind->setEnabled(ok);
    if (!ok && !m_edCode->text().isEmpty())
        m_lblProgress->setText(QStringLiteral("独特码输入中有非法字符！"));
    else if (m_lblProgress->text().startsWith(QStringLiteral("独特码输入")))
        m_lblProgress->clear();
}

bool DlgSearchUnique::ParseCode(std::vector<uint8_t>& bits, QString& err) const
{
    bits.clear();
    QString text = m_edCode->text();
    text.remove(QLatin1Char(' '));

    if (m_format == UniqueCodeFormat::Binary) {
        for (const QChar& qc : text) {
            if (qc != QChar(QLatin1Char('0')) && qc != QChar(QLatin1Char('1'))) {
                err = QStringLiteral("独特码输入有错误！");
                return false;
            }
        }
        int n = text.size();
        if (n == 0) { err = QStringLiteral("独特码长度不能为0！"); return false; }
        if (n % 8 != 0) n = ((n / 8) + 1) * 8;
        for (int i = 0; i < n; ++i) {
            const QChar c = (i < text.size()) ? QChar(text[i]) : QChar(QLatin1Char('0'));
            if (i % 8 == 0) bits.push_back(0);
            bits.back() = static_cast<uint8_t>((bits.back() << 1) |
                          (c == QChar(QLatin1Char('1')) ? 1 : 0));
        }
        return true;
    }

    if (m_format == UniqueCodeFormat::Hex) {
        QString t;
        for (const QChar& qc : text) {
            if (!qc.isDigit() && !QStringLiteral("abcdefABCDEF").contains(qc, Qt::CaseInsensitive)) {
                err = QStringLiteral("独特码输入有错误！");
                return false;
            }
            t += qc;
        }
        if (t.isEmpty()) { err = QStringLiteral("独特码长度不能为0！"); return false; }
        if (t.size() % 2 != 0) t.prepend(QLatin1Char('0'));
        for (int i = 0; i + 1 < t.size(); i += 2) {
            const uint v = t.mid(i, 2).toUInt(nullptr, 16);
            bits.push_back(static_cast<uint8_t>(v));
        }
        return true;
    }

    for (const QChar& qc : text)
        bits.push_back(static_cast<uint8_t>(qc.unicode() & 0xFF));
    if (bits.empty()) { err = QStringLiteral("独特码长度不能为0！"); return false; }
    return true;
}

void DlgSearchUnique::OnFind()
{
    UniqueCodeSpec spec;
    QString err;
    if (!ParseCode(spec.code, err)) {
        QMessageBox::warning(this, QStringLiteral("查找独特码"), err);
        return;
    }
    spec.errorBits = m_edErr->text().toInt();
    spec.removeAmbiguity = m_chkAmb->isChecked();
    std::wstring werr;
    if (!spec.Validate(&werr)) {
        QMessageBox::warning(this, QStringLiteral("查找独特码"),
                             QString::fromStdWString(werr));
        return;
    }

    m_stop = false;
    m_hits.clear();
    m_edHits->clear();
    m_btnFind->setEnabled(false);

    DlgProgress prog(this, QStringLiteral("查找独特码"));
    prog.show();

    SearchEngine eng;
    eng.FindUniqueCode(*m_map, spec, 0, true, m_hits,
        [this, &prog](uint64_t done, uint64_t total, bool finished) {
            if (prog.Cancelled()) m_stop = true;
            if (!finished) prog.SetProgress(done, total);
            else prog.SetText(QStringLiteral("扫描完成，共命中 %1 处")
                                  .arg(static_cast<qulonglong>(m_hits.size())));
        }, m_stop);

    prog.Finish();
    prog.close();
    m_btnFind->setEnabled(true);

    QStringList lines;
    for (size_t i = 0; i < m_hits.size() && i < 500; ++i)
        lines << QStringLiteral("第 %1 比特（帧 %2，列 %3）")
                    .arg(static_cast<qulonglong>(m_hits[i].bitPos))
                    .arg(static_cast<qulonglong>(m_hits[i].bitPos / 8))
                    .arg(static_cast<qulonglong>(m_hits[i].bitPos % 8));
    if (m_hits.size() > 500) lines << QStringLiteral("...");
    m_edHits->setPlainText(lines.join(QLatin1Char('\n')));
    m_lblProgress->setText(QStringLiteral("共找到 %1 个匹配")
                               .arg(static_cast<qulonglong>(m_hits.size())));
    accept();
}
