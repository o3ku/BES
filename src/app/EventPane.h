// EventPane.h —— 事件窗格
#pragma once

#include <QTableWidget>

struct BesEvent {
    QString source;
    QString type;
    QString time;
    QString info;
};

class EventPane : public QTableWidget {
    Q_OBJECT
public:
    explicit EventPane(QWidget* parent = nullptr);

    void AddEvent(const BesEvent& e);
};
