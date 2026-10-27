// DlgCustomize.cpp
#include "dlg/DlgCustomize.h"
#include "app/MainWindow.h"
#include "app/Actions.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {
QString CommandLabel(const QAction* a, int id)
{
    if (id == MainWindow::BarSeparator)
        return QStringLiteral("（分隔符）");
    if (id == MainWindow::BarWidgetMarker)
        return QStringLiteral("（纵轴显示）");
    if (a) {
        const QString text = a->iconText().isEmpty() ? a->text() : a->iconText();
        if (!text.isEmpty())
            return text;
    }
    return QString::number(id);
}
} // namespace

DlgCustomize::DlgCustomize(MainWindow* mw)
    : QDialog(mw), m_mw(mw)
{
    // 原版为 DevExpress 内嵌的 TdxBarCustomizingForm（Caption "Customize"）
    setWindowTitle(QStringLiteral("Customize"));
    resize(600, 430);

    auto* vbox = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("custTabs"));
    vbox->addWidget(tabs, 1);

    // ---- Toolbars 页：每条工具栏独立显隐 ----
    auto* pageBars = new QWidget(tabs);
    auto* vb = new QVBoxLayout(pageBars);
    vb->addWidget(new QLabel(QStringLiteral("显示以下工具栏："), pageBars));
    for (int i = 0; i < MainWindow::BarCount; ++i) {
        auto* cb = new QCheckBox(MainWindow::ToolBarName(i), pageBars);
        cb->setObjectName(QStringLiteral("custBar%1").arg(i));
        const QToolBar* tb = mw->GetToolBar(i);
        cb->setChecked(tb && !tb->isHidden());
        connect(cb, &QCheckBox::toggled, this, [this, i](bool on){ m_mw->SetBarVisible(i, on); });
        vb->addWidget(cb);
        m_barChecks << cb;
    }
    vb->addStretch(1);
    tabs->addTab(pageBars, QStringLiteral(" Toolbars "));

    // ---- Commands 页：当前按钮 / 可用命令 ----
    auto* pageCmd = new QWidget(tabs);
    auto* vc = new QVBoxLayout(pageCmd);

    auto* hTop = new QHBoxLayout;
    hTop->addWidget(new QLabel(QStringLiteral("工具栏："), pageCmd));
    m_barCombo = new QComboBox(pageCmd);
    m_barCombo->setObjectName(QStringLiteral("custBarCombo"));
    for (int i = 0; i < MainWindow::BarCount; ++i)
        m_barCombo->addItem(MainWindow::ToolBarName(i), i);
    hTop->addWidget(m_barCombo, 1);
    vc->addLayout(hTop);

    auto* hLists = new QHBoxLayout;

    auto* colCur = new QVBoxLayout;
    colCur->addWidget(new QLabel(QStringLiteral("当前按钮："), pageCmd));
    m_current = new QListWidget(pageCmd);
    m_current->setObjectName(QStringLiteral("custCurrent"));
    colCur->addWidget(m_current, 1);
    hLists->addLayout(colCur, 1);

    auto* colBtn = new QVBoxLayout;
    colBtn->addStretch(1);
    m_add = new QPushButton(QStringLiteral("添加 >>"), pageCmd);
    m_add->setObjectName(QStringLiteral("custAdd"));
    m_remove = new QPushButton(QStringLiteral("<< 删除"), pageCmd);
    m_remove->setObjectName(QStringLiteral("custRemove"));
    m_up = new QPushButton(QStringLiteral("上移"), pageCmd);
    m_up->setObjectName(QStringLiteral("custUp"));
    m_down = new QPushButton(QStringLiteral("下移"), pageCmd);
    m_down->setObjectName(QStringLiteral("custDown"));
    for (QPushButton* b : { m_add, m_remove, m_up, m_down })
        colBtn->addWidget(b);
    colBtn->addStretch(1);
    hLists->addLayout(colBtn);

    auto* colAvail = new QVBoxLayout;
    colAvail->addWidget(new QLabel(QStringLiteral("可用命令："), pageCmd));
    m_available = new QListWidget(pageCmd);
    m_available->setObjectName(QStringLiteral("custAvailable"));
    colAvail->addWidget(m_available, 1);
    hLists->addLayout(colAvail, 1);

    vc->addLayout(hLists, 1);
    tabs->addTab(pageCmd, QStringLiteral(" Commands "));

    auto* hBtn = new QHBoxLayout;
    m_reset = new QPushButton(QStringLiteral("Reset..."), this);
    m_reset->setObjectName(QStringLiteral("custReset"));
    auto* btnClose = new QPushButton(QStringLiteral("Close"), this);
    btnClose->setObjectName(QStringLiteral("custClose"));
    btnClose->setDefault(true);
    hBtn->addWidget(m_reset);
    hBtn->addStretch(1);
    hBtn->addWidget(btnClose);
    vbox->addLayout(hBtn);

    FillAvailable();
    LoadBar(0);

    connect(m_barCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int idx){ LoadBar(m_barCombo->itemData(idx).toInt()); });
    connect(m_current, &QListWidget::itemSelectionChanged, this, &DlgCustomize::UpdateButtons);
    connect(m_available, &QListWidget::itemSelectionChanged, this, &DlgCustomize::UpdateButtons);
    connect(m_current, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*){ if (m_remove->isEnabled()) m_remove->click(); });
    connect(m_available, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*){ m_add->click(); });

    connect(m_add, &QPushButton::clicked, this, [this]{
        QVector<int> items = CurrentItems();
        for (QListWidgetItem* it : m_available->selectedItems())
            items << it->data(Qt::UserRole).toInt();
        m_mw->SetToolBarItems(m_barCombo->currentData().toInt(), items);
        LoadBar(m_barCombo->currentData().toInt());
    });
    connect(m_remove, &QPushButton::clicked, this, [this]{
        QVector<int> items = CurrentItems();
        QList<int> rows;
        for (QListWidgetItem* it : m_current->selectedItems())
            rows << m_current->row(it);
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int row : rows) {
            if (row >= 0 && row < items.size() && items[row] != MainWindow::BarWidgetMarker)
                items.remove(row);
        }
        m_mw->SetToolBarItems(m_barCombo->currentData().toInt(), items);
        LoadBar(m_barCombo->currentData().toInt());
    });
    auto move = [this](int delta) {
        const int bar = m_barCombo->currentData().toInt();
        QVector<int> items = CurrentItems();
        const int row = m_current->currentRow();
        const int dst = row + delta;
        if (row < 0 || dst < 0 || dst >= items.size()) return;
        if (items[row] == MainWindow::BarWidgetMarker ||
            items[dst] == MainWindow::BarWidgetMarker) return;
        std::swap(items[row], items[dst]);
        m_mw->SetToolBarItems(bar, items);
        LoadBar(bar);
        m_current->setCurrentRow(dst);
    };
    connect(m_up, &QPushButton::clicked, this, [move]{ move(-1); });
    connect(m_down, &QPushButton::clicked, this, [move]{ move(1); });
    connect(m_reset, &QPushButton::clicked, this, &DlgCustomize::ResetAll);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
}

void DlgCustomize::FillAvailable()
{
    m_available->clear();
    if (!m_mw->Cmds()) return;
    for (QAction* a : m_mw->Cmds()->All()) {
        if (!a) continue;
        const int id = m_mw->Cmds()->IdOf(a);
        if (id <= 0) continue;
        auto* it = new QListWidgetItem(a->iconText().isEmpty() ? a->text() : a->iconText(),
                                       m_available);
        it->setData(Qt::UserRole, id);
    }
}

void DlgCustomize::LoadBar(int bar)
{
    if (m_barCombo->currentData().toInt() != bar) {
        QSignalBlocker block(m_barCombo);
        m_barCombo->setCurrentIndex(m_barCombo->findData(bar));
    }
    m_current->clear();
    for (int id : m_mw->ToolBarItems(bar)) {
        auto* it = new QListWidgetItem(CommandLabel(m_mw->Cmds() ? m_mw->Cmds()->Find(id) : nullptr, id),
                                       m_current);
        it->setData(Qt::UserRole, id);
        if (id == MainWindow::BarWidgetMarker)
            it->setFlags(Qt::ItemIsEnabled);   // 固定控件占位，不可删除/移动
    }
    UpdateButtons();
}

QVector<int> DlgCustomize::CurrentItems() const
{
    QVector<int> out;
    for (int i = 0; i < m_current->count(); ++i)
        out << m_current->item(i)->data(Qt::UserRole).toInt();
    return out;
}

void DlgCustomize::UpdateButtons()
{
    const int row = m_current->currentRow();
    const bool movable = row >= 0 &&
        m_current->item(row)->data(Qt::UserRole).toInt() != MainWindow::BarWidgetMarker;
    m_remove->setEnabled(movable);
    m_up->setEnabled(movable && row > 0 &&
        m_current->item(row - 1)->data(Qt::UserRole).toInt() != MainWindow::BarWidgetMarker);
    m_down->setEnabled(movable && row + 1 < m_current->count() &&
        m_current->item(row + 1)->data(Qt::UserRole).toInt() != MainWindow::BarWidgetMarker);
    m_add->setEnabled(!m_available->selectedItems().isEmpty());
}

void DlgCustomize::ResetAll()
{
    m_mw->ResetToolBars();
    for (int i = 0; i < m_barChecks.size(); ++i)
        m_barChecks[i]->setChecked(true);
    LoadBar(m_barCombo->currentData().toInt());
}
