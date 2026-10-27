// DlgAbout.cpp —— 按 TAboutBox DFM 还原：332×256、左侧徽标、版本/接口/作者信息
#include "dlg/DlgAbout.h"

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPolygon>
#include <QPushButton>

namespace {

class LogoBox : public QWidget {
public:
    explicit LogoBox(QWidget* parent = nullptr) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRect r = rect().adjusted(2, 2, -3, -3);
        p.fillRect(r, QColor(0x20, 0x40, 0x90));

        QFont f = font();
        f.setPointSize(22);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(r.adjusted(0, 0, 0, -40), Qt::AlignCenter, QStringLiteral("BES"));

        p.setPen(QPen(QColor(0x80, 0xC0, 0xFF), 2));
        QPolygon wave;
        const int midY = r.center().y() + 55;
        for (int x = r.left() + 10; x <= r.right() - 10; x += 4) {
            const int phase = (x - r.left()) % 32;
            const int dy = phase < 8 ? -phase : phase < 16 ? phase - 16
                         : phase < 24 ? 8 - phase : phase - 32;
            wave << QPoint(x, midY + dy);
        }
        p.drawPolyline(wave);
    }
};

QLabel* MakeLabel(QWidget* parent, const QString& name, const QString& text,
                  const QRect& geom, const QColor& color)
{
    auto* lab = new QLabel(text, parent);
    lab->setObjectName(name);
    lab->setGeometry(geom);
    QPalette pal = lab->palette();
    pal.setColor(QPalette::WindowText, color);
    lab->setPalette(pal);
    return lab;
}

} // namespace

DlgAbout::DlgAbout(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("dlgAbout"));
    setWindowTitle(QStringLiteral("关于BES"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) | Qt::MSWindowsFixedSizeDialogHint);
    setFixedSize(332, 256);

    auto* logo = new LogoBox(this);
    logo->setObjectName(QStringLiteral("aboutLogo"));
    logo->setGeometry(8, 11, 88, 208);

    QFont titleFont(QStringLiteral("SimHei"), 14);
    titleFont.setBold(true);
    QLabel* product = MakeLabel(this, QStringLiteral("aboutProductName"),
                                QStringLiteral("浏览/编辑/选取"),
                                QRect(108, 10, 200, 24), QColor(0, 0, 255));
    product->setFont(titleFont);

    MakeLabel(this, QStringLiteral("aboutVersion"),
              QStringLiteral("版本：1.2 2007-12-07"),
              QRect(108, 42, 200, 16), Qt::black);
    MakeLabel(this, QStringLiteral("aboutApiVersion"),
              QStringLiteral("分析应用层接口版本: 2.0"),
              QRect(108, 63, 200, 16), Qt::black);
    MakeLabel(this, QStringLiteral("aboutAuthor"),
              QStringLiteral("作者: 范晓星"),
              QRect(108, 185, 160, 16), Qt::black);
    MakeLabel(this, QStringLiteral("aboutOrg"),
              QStringLiteral("2007  57所信号分析科"),
              QRect(108, 203, 200, 16), Qt::black);

    auto* bevel = new QFrame(this);
    bevel->setObjectName(QStringLiteral("aboutBevel"));
    bevel->setGeometry(108, 211, 214, 2);
    bevel->setFrameShape(QFrame::HLine);
    bevel->setFrameShadow(QFrame::Sunken);

    auto* ok = new QPushButton(QStringLiteral("确定"), this);
    ok->setObjectName(QStringLiteral("aboutOkButton"));
    ok->setGeometry(248, 227, 75, 23);
    ok->setDefault(true);
    ok->setAutoDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}
