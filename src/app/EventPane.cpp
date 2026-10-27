// EventPane.cpp
#include "app/EventPane.h"

#include <QHeaderView>

EventPane::EventPane(QWidget* parent)
    : QTableWidget(0, 4, parent)
{
    setHorizontalHeaderLabels({
        QStringLiteral("来源"), QStringLiteral("类型"),
        QStringLiteral("时间"), QStringLiteral("事件信息") });
    horizontalHeader()->setStretchLastSection(true);
    verticalHeader()->setVisible(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFont(QFont(QStringLiteral("宋体"), 9));
}

void EventPane::AddEvent(const BesEvent& e)
{
    const int n = rowCount();
    insertRow(n);
    setItem(n, 0, new QTableWidgetItem(e.source));
    setItem(n, 1, new QTableWidgetItem(e.type));
    setItem(n, 2, new QTableWidgetItem(e.time));
    setItem(n, 3, new QTableWidgetItem(e.info));
    scrollToBottom();
}
