#include "app/ViewArea.h"

#include <QTabBar>
#include <QSplitter>
#include <QVBoxLayout>

ViewArea::ViewArea(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_bar = new QTabBar(this);
    m_bar->setExpanding(false);
    m_bar->setDrawBase(true);
    m_bar->setUsesScrollButtons(true);
    lay->addWidget(m_bar);

    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(4);
    lay->addWidget(m_split, 1);

    connect(m_bar, &QTabBar::currentChanged, this, [this](int){
        relayout();
        emit currentChanged(m_bar->currentIndex());
    });
    connect(m_bar, &QTabBar::tabCloseRequested, this, &ViewArea::tabCloseRequested);
}

QWidget* ViewArea::widget(int idx) const
{
    if (idx < 0 || idx >= m_views.size()) return nullptr;
    return m_views[idx];
}

int ViewArea::indexOf(QWidget* w) const
{
    return m_views.indexOf(w);
}

QString ViewArea::tabText(int idx) const
{
    if (idx < 0 || idx >= m_bar->count()) return QString();
    return m_bar->tabText(idx);
}

void ViewArea::setTabText(int idx, const QString& text)
{
    if (idx >= 0 && idx < m_bar->count()) m_bar->setTabText(idx, text);
}

void ViewArea::insertTab(int idx, QWidget* w, const QString& text)
{
    if (!w) return;
    idx = qBound(0, idx, m_views.size());
    m_views.insert(idx, w);
    m_bar->insertTab(idx, text);   // 首个标签会把当前页置为 0
    m_split->insertWidget(idx, w);
    relayout();
}

int ViewArea::addTab(QWidget* w, const QString& text)
{
    const int idx = m_views.size();
    insertTab(idx, w, text);
    return idx;
}

void ViewArea::removeTab(int idx)
{
    if (idx < 0 || idx >= m_views.size()) return;
    if (m_shown.size()) {
        QVector<int> out;
        for (int v : m_shown) {
            if (v == idx) continue;
            out.push_back(v > idx ? v - 1 : v);
        }
        m_shown = out;
    }
    QWidget* w = m_views.takeAt(idx);
    m_bar->removeTab(idx);         // 当前页受影响时会发出 currentChanged
    w->setParent(nullptr);         // 同时离开 QSplitter
    relayout();
}

void ViewArea::setCurrentIndex(int idx)
{
    if (idx < 0 || idx >= m_views.size()) return;
    m_bar->setCurrentIndex(idx);   // 经 currentChanged 走 relayout
}

void ViewArea::setTabsClosable(bool on)
{
    m_bar->setTabsClosable(on);
}

void ViewArea::setArrangement(bes::ViewLayout a)
{
    if (m_arr == a) return;
    m_arr = a;
    relayout();
}

void ViewArea::setShownViews(const QVector<int>& idxs)
{
    m_shown = idxs;
    relayout();
}

int ViewArea::visibleViewCount() const
{
    int n = 0;
    for (QWidget* w : m_views)
        if (w && !w->isHidden()) ++n;
    return n;
}

void ViewArea::relayout()
{
    const bool split = (m_arr != bes::ViewLayout::Paged);
    m_split->setOrientation((m_arr == bes::ViewLayout::Vertical ||
                             m_arr == bes::ViewLayout::GroupV)
                                ? Qt::Vertical : Qt::Horizontal);
    if (!split) {
        // 分页：全部视图留在分隔窗口里并占据同一格（隐藏件不占空间），
        // 未选中的只是不可见——保留其几何尺寸，滚动/度量才不会失真。
        for (int i = 0; i < m_views.size(); ++i) {
            QWidget* w = m_views[i];
            if (w) w->setVisible(i == currentIndex());
        }
        return;
    }
    QVector<int> shown = m_shown;
    if (shown.isEmpty())
        for (int i = 0; i < m_views.size(); ++i) shown.push_back(i);
    for (int i = 0; i < m_views.size(); ++i) {
        QWidget* w = m_views[i];
        if (w) w->setVisible(shown.contains(i));
    }
}
