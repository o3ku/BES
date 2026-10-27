// DlgOutputSet.cpp
#include "dlg/DlgOutputSet.h"

#include <algorithm>

#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace bes;

DlgOutputSet::DlgOutputSet(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("输出设置"));
    resize(620, 470);

    auto* vbox = new QVBoxLayout(this);
    auto* hMain = new QHBoxLayout;

    // 左侧：端口树 + 输出内容
    auto* vLeft = new QVBoxLayout;
    auto* lblPorts = new QLabel(QStringLiteral("输出端口(&P)："), this);
    vLeft->addWidget(lblPorts);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("portsTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setMinimumWidth(150);
    vLeft->addWidget(m_tree, 1);

    auto* gbSel = new QGroupBox(QStringLiteral("输出内容"), this);
    auto* vSel = new QVBoxLayout(gbSel);
    m_chkWhole = new QCheckBox(QStringLiteral("全部数据"), gbSel);
    m_chkCont  = new QCheckBox(QStringLiteral("连续选取"), gbSel);
    m_chkRect  = new QCheckBox(QStringLiteral("块选取"), gbSel);
    m_chkRow   = new QCheckBox(QStringLiteral("行选取"), gbSel);
    m_chkCol   = new QCheckBox(QStringLiteral("列选取"), gbSel);
    m_chkWhole->setChecked(true);
    vSel->addWidget(m_chkWhole);
    vSel->addWidget(m_chkCont);
    vSel->addWidget(m_chkRect);
    vSel->addWidget(m_chkRow);
    vSel->addWidget(m_chkCol);
    vLeft->addWidget(gbSel);
    hMain->addLayout(vLeft, 1);

    // 右侧：输出选择 / 文件选择 / 端口选择
    auto* vRight = new QVBoxLayout;

    auto* gbOut = new QGroupBox(QStringLiteral("输出选择"), this);
    auto* hOut = new QHBoxLayout(gbOut);
    m_rbNone = new QRadioButton(QStringLiteral("不输出(&N)"), gbOut);
    m_rbNone->setObjectName(QStringLiteral("noneRadio"));
    m_rbFile = new QRadioButton(QStringLiteral("输出到文件(&F)"), gbOut);
    m_rbFile->setObjectName(QStringLiteral("fileRadio"));
    m_rbPort = new QRadioButton(QStringLiteral("输出到端口(&P)"), gbOut);
    m_rbPort->setObjectName(QStringLiteral("portRadio"));
    m_rbNone->setChecked(true);
    auto* bgMode = new QButtonGroup(this);
    bgMode->addButton(m_rbNone);
    bgMode->addButton(m_rbFile);
    bgMode->addButton(m_rbPort);
    hOut->addWidget(m_rbNone);
    hOut->addWidget(m_rbFile);
    hOut->addWidget(m_rbPort);
    vRight->addWidget(gbOut);

    auto* gbFile = new QGroupBox(QStringLiteral("文件选择"), this);
    auto* hFile = new QHBoxLayout(gbFile);
    m_edFile = new QLineEdit(gbFile);
    m_btnBrowse = new QPushButton(QStringLiteral("浏览(&B)..."), gbFile);
    hFile->addWidget(m_edFile, 1);
    hFile->addWidget(m_btnBrowse);
    vRight->addWidget(gbFile);

    auto* gbPort = new QGroupBox(QStringLiteral("端口选择"), this);
    auto* vPort = new QVBoxLayout(gbPort);
    auto* grid = new QGridLayout;
    m_portGroup = new QButtonGroup(this);
    for (int i = 0; i < 32; ++i) {
        const int port = i + 1;
        auto* rb = new QRadioButton(QStringLiteral("端口%1").arg(port), gbPort);
        rb->setObjectName(QStringLiteral("portSelect%1").arg(port));
        m_portRadios[i] = rb;
        m_portGroup->addButton(rb, port);
        // 原版 3 列：11 / 11 / 10
        grid->addWidget(rb, i % 11, i / 11);
    }
    vPort->addLayout(grid);
    m_portNote = new QLabel(gbPort);
    m_portNote->setStyleSheet(QStringLiteral("color:#666;"));
    m_portNote->setWordWrap(true);
    vPort->addWidget(m_portNote);
    vPort->addStretch(1);
    vRight->addWidget(gbPort, 1);
    hMain->addLayout(vRight, 2);

    vbox->addLayout(hMain, 1);

    auto* hBtn = new QHBoxLayout;
    auto* btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setDefault(true);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    hBtn->addStretch(1);
    hBtn->addWidget(btnOk);
    hBtn->addWidget(btnCancel);
    vbox->addLayout(hBtn);

    connect(bgMode, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, [this](int){ OnModeChanged(); });
    connect(m_portGroup, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this, &DlgOutputSet::OnPortClicked);
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem*, QTreeWidgetItem*){ OnPortTreeChanged(); });
    connect(m_btnBrowse, &QPushButton::clicked, this, &DlgOutputSet::OnBrowse);
    connect(btnOk, &QPushButton::clicked, this, [this]{
        ReadControls();
        accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_portNote->setText(QStringLiteral("未注册构件输出端口，使用通用端口 1..32（写入临时文件）。"));
    FillPortChoices();
}

void DlgOutputSet::SetPortChoices(const QStringList& labels,
                                  const QVector<int>& indices)
{
    m_portLabels = labels;
    m_portIndices = indices;
    FillPortChoices();
}

void DlgOutputSet::FillPortChoices()
{
    m_tree->clear();
    m_componentPorts = !m_portLabels.isEmpty();

    if (!m_componentPorts) {
        for (int port = 1; port <= 32; ++port) {
            auto* item = new QTreeWidgetItem(m_tree);
            item->setText(0, QStringLiteral("端口%1").arg(port));
            item->setData(0, Qt::UserRole, port);
        }
        m_portNote->setText(QStringLiteral("未注册构件输出端口，使用通用端口 1..32（写入临时文件）。"));
    } else {
        for (int i = 0; i < m_portLabels.size() && i < 32; ++i) {
            const int port = i < m_portIndices.size() ? m_portIndices[i] : i + 1;
            if (port < 1 || port > 32) continue;
            auto* item = new QTreeWidgetItem(m_tree);
            item->setText(0, m_portLabels[i]);
            item->setData(0, Qt::UserRole, port);
        }
        m_portNote->setText(QStringLiteral("端口来自已注册构件的数据输出端口；数据写入临时文件。"));
    }

    UpdatePortEnabled();
    if (m_rbPort->isChecked()) {
        int first = FirstAvailablePort();
        if (first > 0) SelectPortRadio(first, true);
    }
}

void DlgOutputSet::UpdatePortEnabled()
{
    const bool portMode = m_rbPort->isChecked();
    for (int i = 0; i < 32; ++i) {
        const int port = i + 1;
        const bool available = !m_componentPorts || m_portIndices.contains(port);
        m_portRadios[i]->setEnabled(portMode && available);
    }
}

void DlgOutputSet::SelectPortRadio(int port, bool syncTree)
{
    if (port < 1 || port > 32) return;
    m_updatingPorts = true;
    m_portRadios[port - 1]->setChecked(true);
    if (syncTree) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_tree->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toInt() == port) {
                m_tree->setCurrentItem(item);
                break;
            }
        }
    }
    m_updatingPorts = false;
}

int DlgOutputSet::FirstAvailablePort() const
{
    for (int port = 1; port <= 32; ++port) {
        if (!m_componentPorts || m_portIndices.contains(port))
            return port;
    }
    return 0;
}

void DlgOutputSet::OnModeChanged()
{
    const bool fileMode = m_rbFile->isChecked();
    m_edFile->setEnabled(fileMode);
    m_btnBrowse->setEnabled(fileMode);
    UpdatePortEnabled();

    if (m_rbPort->isChecked()) {
        const int id = m_portGroup->checkedId();
        if (id < 1 || !m_portRadios[id - 1]->isEnabled()) {
            const int first = FirstAvailablePort();
            if (first > 0) SelectPortRadio(first, true);
        }
    }
}

void DlgOutputSet::OnPortClicked(int id)
{
    if (m_updatingPorts) return;
    if (!m_rbPort->isChecked()) {
        m_rbPort->setChecked(true);
        OnModeChanged();
    }
    SelectPortRadio(id, true);
}

void DlgOutputSet::OnPortTreeChanged()
{
    if (m_updatingPorts) return;
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) return;
    const int port = item->data(0, Qt::UserRole).toInt();
    if (port < 1 || port > 32) return;
    if (!m_rbPort->isChecked()) {
        m_rbPort->setChecked(true);
        OnModeChanged();
    }
    SelectPortRadio(port, false);
}

void DlgOutputSet::OnBrowse()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("输出文件"), QString(),
        QStringLiteral("数据文件 (*.dat);;所有文件 (*.*)"));
    if (!path.isEmpty())
        m_edFile->setText(path);
}

void DlgOutputSet::ReadControls()
{
    if (m_rbNone->isChecked())      m_opt.target = OutputTarget::None;
    else if (m_rbFile->isChecked()) m_opt.target = OutputTarget::File;
    else                            m_opt.target = OutputTarget::Port;
    m_opt.filePath = m_edFile->text().toStdWString();
    int idx = m_portGroup->checkedId();
    if (idx < 1) idx = FirstAvailablePort();
    m_opt.portIndex = std::min(32, std::max(1, idx > 0 ? idx : 1));
    m_opt.continuousSel  = m_chkCont->isChecked();
    m_opt.rectSel        = m_chkRect->isChecked();
    m_opt.rowSel         = m_chkRow->isChecked();
    m_opt.colSel         = m_chkCol->isChecked();
    m_opt.wholeData      = m_chkWhole->isChecked() &&
                           !(m_opt.continuousSel || m_opt.rectSel ||
                             m_opt.rowSel || m_opt.colSel);
}
