// ViewArea.h —— 视图区：标签条 + 分隔窗口，承载原版的「窗口排布」
// 原版 [form+0x498] 是 TMainForm 的全局字段（PageBtnClick=0 / HorizBtnClick=1 /
// VertBtnClick=2 / PageHorizBtnClick=3 / PageVertBtnClick=4），五个按钮遍历所有
// 停靠站点（GroupTreeView 的站点列表）逐个重排，因此排布对「全部视图」生效。
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QVector>

#include "core/DataTypes.h"

class QSplitter;

// QTabWidget 的替代：外观保持标签条，页面区换成可排布的分隔窗口。
class ViewArea : public QWidget {
    Q_OBJECT
public:
    explicit ViewArea(QWidget* parent = nullptr);

    int  count() const { return m_views.size(); }
    int  currentIndex() const { return m_bar->currentIndex(); }
    QWidget* widget(int idx) const;
    QWidget* currentWidget() const { return widget(currentIndex()); }
    int  indexOf(QWidget* w) const;
    QString tabText(int idx) const;
    void   setTabText(int idx, const QString& text);

    int  addTab(QWidget* w, const QString& text);
    void insertTab(int idx, QWidget* w, const QString& text);
    void removeTab(int idx);           // 不销毁控件，调用方负责
    void setCurrentIndex(int idx);
    void setCurrentWidget(QWidget* w)  { setCurrentIndex(indexOf(w)); }

    QTabBar* tabBar() const { return m_bar; }
    void setTabsClosable(bool on);
    void setMovable(bool on)          { m_bar->setMovable(on); }
    void setDocumentMode(bool on)     { m_bar->setDrawBase(on); }

    // ---- 排布（对应 [form+0x498]）----
    void setArrangement(bes::ViewLayout a);
    bes::ViewLayout arrangement() const { return m_arr; }
    // 排布模式下同时显示的视图序号；空 = 全部（分组排布由主窗口给出组内序号）
    void setShownViews(const QVector<int>& idxs);
    QVector<int> shownViews() const { return m_shown; }
    int  visibleViewCount() const;

signals:
    void currentChanged(int idx);
    void tabCloseRequested(int idx);

private:
    void relayout();

    QTabBar*   m_bar = nullptr;
    QSplitter* m_split = nullptr;
    QVector<QWidget*> m_views;
    QVector<int> m_shown;
    bes::ViewLayout m_arr = bes::ViewLayout::Paged;
};
