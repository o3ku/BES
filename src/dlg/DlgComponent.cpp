// DlgComponent.cpp —— 定制构件管理（TComCustomForm）
#include "dlg/DlgComponent.h"

#include "dlg/DlgComSelect.h"

#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QStringList>

#include <algorithm>

using namespace bes;

namespace {

// Delphi 的 TGroupBox 子控件坐标以「客户区」为原点（标题行之下），Qt 以控件
// 左上角为原点，故组内控件统一下沉一个标题高度。
constexpr int kGroupBoxCaption = 14;

QString LeafOf(const QString& path) {
    const int sep = path.lastIndexOf(QChar('\\'));
    const int sep2 = path.lastIndexOf(QChar('/'));
    const int cut = std::max(sep, sep2);
    return cut < 0 ? path : path.mid(cut + 1);
}

QString FromStd(const std::wstring& s) { return QString::fromStdWString(s); }

std::wstring ToStd(const QString& s) { return s.toStdWString(); }

// 注册名是 ANSI(GBK)，原版直接显示在 VCL 控件里
QString RegNameText(const std::string& s) {
    return QString::fromLocal8Bit(s.data(), static_cast<int>(s.size()));
}

bool IsDll(const QString& path) {
    return QFileInfo(path).suffix().compare(QStringLiteral("dll"),
                                            Qt::CaseInsensitive) == 0;
}

} // namespace

DlgComponent::DlgComponent(QWidget* parent, ComponentHost& host)
    : QDialog(parent), m_host(host)
{
    // DFM「ComCustomForm」：BorderIcons=[biSystemMenu]、BorderStyle=bsSingle、
    // 客户区 468x312、poScreenCenter
    setWindowTitle(QStringLiteral("定制构件管理"));
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                   Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    setFixedSize(468, 312);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("ComListBox"));
    m_list->setGeometry(11, 13, 136, 285);

    auto* group = new QGroupBox(QStringLiteral("构件属性"), this);
    group->setObjectName(QStringLiteral("GroupBox1"));
    group->setGeometry(153, 11, 308, 257);

    auto* lbName = new QLabel(QStringLiteral("构件名称"), group);
    lbName->setGeometry(8, 24 + kGroupBoxCaption, 48, 12);
    m_edName = new QLineEdit(group);
    m_edName->setObjectName(QStringLiteral("ComNameEdit"));
    m_edName->setMaxLength(32);
    m_edName->setGeometry(8, 39 + kGroupBoxCaption, 216, 20);

    auto* lbFile = new QLabel(QStringLiteral("构件程序文件"), group);
    lbFile->setGeometry(8, 66 + kGroupBoxCaption, 72, 12);
    m_edFile = new QLineEdit(group);
    m_edFile->setObjectName(QStringLiteral("ComFileEdit"));
    m_edFile->setReadOnly(true);
    m_edFile->setGeometry(8, 83 + kGroupBoxCaption, 266, 20);

    auto* btnBrowse = new QPushButton(QStringLiteral("…"), group);
    btnBrowse->setObjectName(QStringLiteral("BrowseBtn"));
    btnBrowse->setGeometry(275, 83 + kGroupBoxCaption, 26, 21);
    btnBrowse->setToolTip(QStringLiteral("浏览"));

    auto* lbInfo = new QLabel(QStringLiteral("构件信息"), group);
    lbInfo->setGeometry(8, 109 + kGroupBoxCaption, 48, 12);
    m_edInfo = new QPlainTextEdit(group);
    m_edInfo->setObjectName(QStringLiteral("ComInfoMemo"));
    m_edInfo->setReadOnly(true);
    m_edInfo->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edInfo->setGeometry(8, 125 + kGroupBoxCaption, 289, 124);

    // 原版 DFM 三个按钮初始 Enabled=False，选中列表项或探测成功后才放开
    m_btnAdd = new QPushButton(QStringLiteral("添加"), this);
    m_btnAdd->setObjectName(QStringLiteral("AddBtn"));
    m_btnAdd->setGeometry(151, 275, 75, 23);
    m_btnAdd->setEnabled(false);

    m_btnDel = new QPushButton(QStringLiteral("删除"), this);
    m_btnDel->setObjectName(QStringLiteral("DelBtn"));
    m_btnDel->setGeometry(229, 275, 75, 23);
    m_btnDel->setEnabled(false);

    m_btnEdit = new QPushButton(QStringLiteral("修改"), this);
    m_btnEdit->setObjectName(QStringLiteral("EditBtn"));
    m_btnEdit->setGeometry(308, 275, 75, 23);
    m_btnEdit->setEnabled(false);

    auto* btnQuit = new QPushButton(QStringLiteral("退出"), this);
    btnQuit->setObjectName(QStringLiteral("QuitBtn"));
    btnQuit->setGeometry(386, 275, 75, 23);

    connect(btnBrowse, &QPushButton::clicked, this, &DlgComponent::OnBrowse);
    connect(m_btnAdd, &QPushButton::clicked, this, &DlgComponent::OnAdd);
    connect(m_btnDel, &QPushButton::clicked, this, &DlgComponent::OnDelete);
    connect(m_btnEdit, &QPushButton::clicked, this, &DlgComponent::OnModify);
    // QuitBtn.ModalResult = mrCancel
    connect(btnQuit, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged,
            this, &DlgComponent::OnSelectionChanged);

    if (auto* scr = QApplication::primaryScreen())
        move(scr->availableGeometry().center() - rect().center());

    InitFromHost();   // 原版 FormCreate
}

void DlgComponent::InitFromHost() {
    Reload();
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);   // 原版 FormCreate 选中第 0 项并触发 Click
    else
        OnSelectionChanged();
}

ComponentInfo* DlgComponent::CurrentInfo() {
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_host.Count())) return nullptr;
    return &m_host.ComponentsMutable()[static_cast<size_t>(row)];
}

void DlgComponent::Reload() {
    m_updating = true;
    const int keep = m_list->currentRow();
    m_list->clear();
    for (const ComponentInfo& ci : m_host.Components()) {
        auto* it = new QListWidgetItem(FromStd(ci.name), m_list);
        it->setSizeHint(QSize(0, 16));   // DFM ItemHeight = 16
    }
    m_updating = false;
    if (keep >= 0 && keep < m_list->count())
        m_list->setCurrentRow(keep);
}

QString DlgComponent::InfoText(const InstanceInfo& info) const {
    // 标签取自 BesCom.dll 0x5DB0BD 起连续存放的常量串（标签自带冒号与空格，
    // 但「物理端口 ID:」后没有空格）；端口字段顺序为 ID/数据类型/端口类型/
    // 缺省端口数量/最大端口数量，每行以换行结束，端口数量行后多一个空行。
    QString s;
    s += QStringLiteral("作者: ") + FromStd(info.author) + QChar('\n');
    s += QStringLiteral("版本: ") + FromStd(info.version) + QChar('\n');
    s += QStringLiteral("简要说明: ") + FromStd(info.desc) + QChar('\n');
    const ComponentKind kind = (info.comType >= 1 && info.comType <= 4)
        ? static_cast<ComponentKind>(info.comType - 1) : ComponentKind::Normal;
    s += QStringLiteral("构件类型: ")
       + QString::fromWCharArray(ComponentKindName(kind)) + QChar('\n');
    s += QStringLiteral("物理端口数量: ")
       + QString::number(static_cast<int>(info.ports.size()))
       + QStringLiteral("\n\n");
    for (const PortInfo& p : info.ports) {
        s += QStringLiteral("物理端口 ID:") + QString::number(p.index) + QChar('\n');
        s += QStringLiteral("端口数据类型: ") + QString::number(p.rawDataType) + QChar('\n');
        s += QStringLiteral("端口类型: ")
           + QString::fromWCharArray(PortKindName(p.kind)) + QChar('\n');
        s += QStringLiteral("缺省端口数量: ") + QString::number(p.defaultCount) + QChar('\n');
        s += QStringLiteral("最大端口数量: ") + QString::number(p.maxCount) + QChar('\n');
    }
    return s;
}

void DlgComponent::ShowComponent(const ComponentInfo& ci) {
    const QString path = FromStd(ci.dllPath);
    m_edName->setText(FromStd(ci.name));
    // 原版 ComListBoxClick：DLL 记录的 ComFileEdit = 文件名 + ': ' + 注册名，
    // 备忘由 ComInfoText(com\文件名, 注册名) 现取；非 DLL 记录显示完整路径、
    // 备忘清空。
    if (IsDll(path)) {
        const QString leaf = LeafOf(path);
        m_edFile->setText(leaf + QStringLiteral(": ") + RegNameText(ci.regNameAnsi));
        m_pendingSrc = FromStd(ComponentHost::ComDir()) + QStringLiteral("\\") + leaf;
        InstanceInfo info;
        std::wstring err;
        if (ComponentHost::QueryInstanceInfo(ci.dllPath, ci.regNameAnsi, &info, &err)
            == ComponentHost::InstanceInfoStatus::Ok)
            m_edInfo->setPlainText(InfoText(info));
        else
            m_edInfo->clear();
    } else {
        m_edFile->setText(path);
        m_edInfo->clear();
        m_pendingSrc = path;
    }
    // 原版选中列表项后 添加/删除/修改 三个按钮都放开
    m_btnAdd->setEnabled(true);
    m_btnDel->setEnabled(true);
    m_btnEdit->setEnabled(true);
}

void DlgComponent::ShowPending(const QString& name, const QString& file,
                               const QString& info) {
    m_edName->setText(name);
    m_edFile->setText(file);
    m_edInfo->setPlainText(info);
    m_btnAdd->setEnabled(true);
}

void DlgComponent::OnSelectionChanged() {
    if (m_updating) return;
    const ComponentInfo* ci = CurrentInfo();
    if (!ci) {
        // 原版 ItemIndex = -1 时直接返回：输入框保留上次内容，只禁用删除/修改
        m_btnDel->setEnabled(false);
        m_btnEdit->setEnabled(false);
        return;
    }
    ShowComponent(*ci);
}

void DlgComponent::OnBrowse() {
    // OpenDialog：Title=选择构件程序文件、Filter=构件程序文件(*.dll,*.exe)
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择构件程序文件"), QString(),
        QStringLiteral("构件程序文件 (*.dll *.exe)"));
    if (path.isEmpty()) return;

    m_pendingSrc = path;

    if (!IsDll(path)) {
        // 原版非 .dll 分支：名称取文件名，信息清空，直接允许「添加」
        ShowPending(LeafOf(path), path, QString());
        return;
    }

    std::vector<ComponentInfo> probed;
    std::wstring err;
    if (!ComponentHost::PeekAbi(ToStd(path), &probed, &err)) {
        QMessageBox::critical(this, QStringLiteral("错误"), FromStd(err));
        return;
    }
    DlgComSelect dlg(this, probed);
    if (dlg.exec() != QDialog::Accepted || dlg.SelectedName().empty()) return;

    const ComponentInfo* pick = nullptr;
    for (const ComponentInfo& ci : probed)
        if (ci.regNameAnsi == dlg.SelectedRegName()) { pick = &ci; break; }
    if (!pick) pick = &probed.front();

    m_selectedProbe = dlg.SelectedName();
    // 原版：ComFileEdit = 文件名 + ': ' + 注册名，备忘 = ComInfoText(原路径, 注册名)
    ShowPending(FromStd(pick->name),
                LeafOf(path) + QStringLiteral(": ") + RegNameText(pick->regNameAnsi),
                InfoText(dlg.Instance()));
}

void DlgComponent::OnAdd() {
    if (m_pendingSrc.isEmpty()) return;
    const QString shown = m_edName->text();
    if (shown.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QStringLiteral("没有可用构件！"));
        return;
    }

    if (IsDll(m_pendingSrc)) {
        std::wstring copied, err;
        if (!m_host.RegisterDllCopied(ToStd(m_pendingSrc), &copied, &err)) {
            QMessageBox::critical(this, QStringLiteral("错误"), FromStd(err));
            return;
        }
        // 原版把界面上的名称写回记录（rec+0x174 := ComNameEdit.Text）
        if (!m_selectedProbe.empty()) {
            if (ComponentInfo* ci = m_host.FindMutableByName(m_selectedProbe))
                ci->name = ToStd(shown);
        }
    } else {
        // 非构件文件（原版允许登记 .exe 等）：不复制，直接记完整路径
        ComponentInfo ci;
        ci.dllPath = ToStd(m_pendingSrc);
        ci.name = ToStd(shown);
        m_host.ComponentsMutable().push_back(ci);
    }

    Reload();
    m_list->setCurrentRow(m_list->count() - 1);
    m_btnAdd->setEnabled(false);
    emit ComponentRegistered(shown);
}

void DlgComponent::OnDelete() {
    ComponentInfo* ci = CurrentInfo();
    if (!ci) return;
    const int row = m_list->currentRow();
    const std::wstring name = ci->name;
    // 原版只从构件表移除记录并释放，不删除磁盘文件
    m_host.UnregisterByName(name);
    Reload();
    if (m_list->count() == 0) {
        m_btnDel->setEnabled(false);
        m_btnEdit->setEnabled(false);
        return;
    }
    m_list->setCurrentRow(std::max(0, row - 1));
}

void DlgComponent::OnModify() {
    ComponentInfo* ci = CurrentInfo();
    if (!ci) return;
    const QString shown = m_edName->text();
    if (shown.isEmpty()) return;
    const std::wstring old = ci->name;
    if (ComponentInfo* target = m_host.FindMutableByName(old))
        target->name = ToStd(shown);
    Reload();
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->text() == shown) {
            m_list->setCurrentRow(i);
            break;
        }
    }
}
