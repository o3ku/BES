// GroupPane.cpp
#include "app/GroupPane.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QTimer>

GroupPane::GroupPane(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderLabel(QStringLiteral("窗口分组"));
    setFont(QFont(QStringLiteral("宋体"), 9));
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
}

void GroupPane::AddGroup(const QString& name)
{
    // 已存在则不重复创建
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (topLevelItem(i)->text(0) == name) {
            expandItem(topLevelItem(i));
            return;
        }
    }
    QTreeWidgetItem* it = new QTreeWidgetItem(this);
    it->setText(0, name);
    it->setData(0, Qt::UserRole, -1);
    addTopLevelItem(it);
    expandItem(it);
}

void GroupPane::AddToGroup(const QString& group, const QString& child, int tabIndex)
{
    QTreeWidgetItem* target = nullptr;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (topLevelItem(i)->text(0) == group) {
            target = topLevelItem(i);
            break;
        }
    }
    if (!target) {
        AddGroup(group);
        for (int i = 0; i < topLevelItemCount(); ++i)
            if (topLevelItem(i)->text(0) == group) target = topLevelItem(i);
    }
    if (!target) return;

    QTreeWidgetItem* c = new QTreeWidgetItem(target);
    c->setText(0, child);
    c->setData(0, Qt::UserRole, tabIndex);
    target->addChild(c);
    expandItem(target);
}

QString GroupPane::GroupOf(QTreeWidgetItem* item) const
{
    if (!item) return QString();
    if (item->parent())
        return item->parent()->text(0);
    return item->text(0);
}

QString GroupPane::TabMimeType()
{
    return QStringLiteral("application/x-bes-window-tab");
}

QMimeData* GroupPane::CreateTabMimeData(int tabIndex)
{
    if (tabIndex < 0) return nullptr;
    auto* data = new QMimeData;
    data->setData(TabMimeType(), QByteArray::number(tabIndex));
    return data;
}

bool GroupPane::DecodeTabMimeData(const QMimeData* data, int* tabIndex)
{
    if (!data || !data->hasFormat(TabMimeType())) return false;
    bool ok = false;
    const int idx = data->data(TabMimeType()).toInt(&ok);
    if (!ok || idx < 0) return false;
    if (tabIndex) *tabIndex = idx;
    return true;
}

QMimeData* GroupPane::mimeData(const QList<QTreeWidgetItem*> items) const
{
    if (items.size() != 1) return nullptr;
    const int idx = items.first()->data(0, Qt::UserRole).toInt();
    if (idx < 0) return nullptr;   // 分组节点本身不可拖
    return CreateTabMimeData(idx);
}

void GroupPane::dragEnterEvent(QDragEnterEvent* ev)
{
    if (DecodeTabMimeData(ev->mimeData(), nullptr)) {
        ev->setDropAction(Qt::MoveAction);
        ev->accept();
        return;
    }
    QTreeWidget::dragEnterEvent(ev);
}

void GroupPane::dragMoveEvent(QDragMoveEvent* ev)
{
    if (DecodeTabMimeData(ev->mimeData(), nullptr)) {
        ev->setDropAction(Qt::MoveAction);
        ev->accept();
        return;
    }
    QTreeWidget::dragMoveEvent(ev);
}

void GroupPane::dropEvent(QDropEvent* ev)
{
    int idx = -1;
    if (!DecodeTabMimeData(ev->mimeData(), &idx)) {
        QTreeWidget::dropEvent(ev);
        return;
    }
    QTreeWidgetItem* item = itemAt(ev->pos());
    const QString group = GroupOf(item);
    if (group.isEmpty()) {
        ev->ignore();
        return;
    }
    ev->setDropAction(Qt::MoveAction);
    ev->accept();
    // 拖放事务结束后再重建树，避免在 Qt 仍持有 item 指针时清空模型
    QTimer::singleShot(0, this, [this, idx, group]{
        emit WindowDroppedOnGroup(idx, group);
    });
}

void GroupPane::contextMenuEvent(QContextMenuEvent* ev)
{
    QTreeWidgetItem* item = itemAt(ev->pos());
    QMenu menu(this);
    QAction* aAdd = menu.addAction(QStringLiteral("新建分组..."));
    QAction* aDel = nullptr;
    QAction* aClose = nullptr;
    if (item) {
        const QString group = GroupOf(item);
        menu.addSeparator();
        aDel = menu.addAction(QStringLiteral("删除分组"));
        aClose = menu.addAction(QStringLiteral("关闭组内窗口"));
        aDel->setData(group);
        aClose->setData(group);
    }
    QAction* chosen = menu.exec(ev->globalPos());
    if (!chosen) return;
    if (chosen == aAdd)
        emit AddGroupRequested();
    else if (chosen == aDel)
        emit DeleteGroupRequested(chosen->data().toString());
    else if (chosen == aClose)
        emit CloseGroupRequested(chosen->data().toString());
}
