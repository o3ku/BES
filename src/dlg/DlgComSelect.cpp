// DlgComSelect.cpp
#include "dlg/DlgComSelect.h"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>

#include <utility>

#include "plugin/ComponentHost.h"

using namespace bes;

DlgComSelect::DlgComSelect(QWidget* parent, const ComponentHost& host)
    : QDialog(parent)
{
    Build(host.Components());
}

DlgComSelect::DlgComSelect(QWidget* parent, const std::vector<ComponentInfo>& items)
    : QDialog(parent)
{
    Build(items);
}

// DFM「ComSelectForm」：BorderStyle=bsDialog、308x211、poScreenCenter；
// Label1(15,20)、ListBox1(13,39,196,157) ItemHeight=16、OKBtn(218,39,75,23)
// 「确定」、Button2(218,67,75,23)「取消」ModalResult=2、
// ListBox1.OnDblClick = OKBtnClick、OKBtn.OnClick = OKBtnClick。
void DlgComSelect::Build(const std::vector<ComponentInfo>& items) {
    setWindowTitle(QStringLiteral("构件选择"));
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setFixedSize(308, 211);

    // 原版 FormCreate：Label1.Caption := '找到' + IntToStr(数量) + '个构件'
    m_items = items;
    auto* label = new QLabel(QStringLiteral("找到%1个构件").arg(items.size()), this);
    label->setObjectName(QStringLiteral("Label1"));
    label->setGeometry(15, 20, label->sizeHint().width(), 12);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("ListBox1"));
    m_list->setGeometry(13, 39, 196, 157);
    for (const ComponentInfo& ci : m_items) {
        auto* it = new QListWidgetItem(QString::fromStdWString(ci.name), m_list);
        it->setSizeHint(QSize(0, 16));
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setObjectName(QStringLiteral("OKBtn"));
    btnOk->setGeometry(218, 39, 75, 23);
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnCancel->setObjectName(QStringLiteral("Button2"));
    btnCancel->setGeometry(218, 67, 75, 23);

    connect(btnOk, &QPushButton::clicked, this, &DlgComSelect::OnOk);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    // 原版 ListBox1.OnDblClick = OKBtnClick
    connect(m_list, &QListWidget::itemDoubleClicked, this, [btnOk]{ btnOk->click(); });

    if (auto* scr = QApplication::primaryScreen())
        move(scr->availableGeometry().center() - rect().center());
}

void DlgComSelect::OnOk() {
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_items.size())) return;
    const ComponentInfo& ci = m_items[static_cast<size_t>(row)];

    InstanceInfo info;
    std::wstring err;
    const ComponentHost::InstanceInfoStatus st =
        ComponentHost::QueryInstanceInfo(ci.dllPath, ci.regNameAnsi, &info, &err);
    if (st != ComponentHost::InstanceInfoStatus::Ok) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QString::fromStdWString(err));
        return;
    }

    // 原版只允许一个数据流输入端口，且其最大端口数量必须为 1；
    // 数据类型合法值为 1..0x0D（LSAS_TYPE_BIT..LSAS_TYPE_64F）与 0xFF（VOID）。
    // 注意：原版把多条提示直接拼接（中间没有换行）。
    QString msg;
    int dataInCount = 0;
    int dataInPortId = 0;
    bool hasOutput = false;
    for (const PortInfo& p : info.ports) {
        if (p.kind != PortKind::DataIn) {
            if (p.kind == PortKind::DataOut) hasOutput = true;
            continue;
        }
        ++dataInCount;
        dataInPortId = p.index;
        if (p.rawDataType != 0xFF && p.rawDataType > 0x0D) {
            msg += QStringLiteral(
                "不支持该构件的数据流输入端口数据类型！\n"
                "支持的数据类型为(LSAS_TYPE_BIT~LSAS_TYPE_64F,LSAS_TYPE_VOID)");
        }
        if (p.kind == PortKind::DataIn && p.maxCount != 1) {
            msg += QStringLiteral("不支持数据流输入端口数目可变！");
            break;
        }
        if (dataInCount > 1) {
            msg += QStringLiteral("不支持多个数据流输入端口！");
            break;
        }
    }
    if (!msg.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("错误"), msg);
        return;
    }

    m_selected = ci.name;
    m_selectedReg = ci.regNameAnsi;
    m_info = std::move(info);
    m_dataSource = m_info.comType == 2;
    m_dataInPortId = dataInPortId;
    m_hasOutput = hasOutput;
    m_portCount = static_cast<int>(m_info.ports.size());
    m_comType = m_info.comType;
    accept();
}
