// GroupPane.h —— 窗口分组（树形，支持新建/删除分组、双击激活与拖动归组）
#pragma once

#include <QTreeWidget>

class QMimeData;

class GroupPane : public QTreeWidget {
    Q_OBJECT
public:
    explicit GroupPane(QWidget* parent = nullptr);

    void AddGroup(const QString& name);
    void AddToGroup(const QString& group, const QString& child, int tabIndex);
    QString GroupOf(QTreeWidgetItem* item) const;

    // 拖动窗口项：MIME 只带标签页序号，目标分组由落点节点决定
    static QString TabMimeType();
    static QMimeData* CreateTabMimeData(int tabIndex);
    static bool DecodeTabMimeData(const QMimeData* data, int* tabIndex);

signals:
    void AddGroupRequested();
    void DeleteGroupRequested(const QString& group);
    void CloseGroupRequested(const QString& group);
    // 「数据窗口」是全部窗口的虚拟组，落到它上面表示移出所有自定义分组
    void WindowDroppedOnGroup(int tabIndex, const QString& group);

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;
    QMimeData* mimeData(const QList<QTreeWidgetItem*> items) const override;
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dragMoveEvent(QDragMoveEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;
};
