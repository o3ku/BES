// DlgGroupItems.h —— 新建窗口分组 / 添加分组项（对应原 TfrmAddGroupItems）
#pragma once

#include <QDialog>
#include <QVector>

class QLineEdit;
class QListWidget;

class DlgGroupItems : public QDialog {
    Q_OBJECT
public:
    DlgGroupItems(QWidget* parent, const QStringList& tabNames,
                  const QString& defaultName = QString());

    QString GroupName() const;
    QVector<int> SelectedTabs() const;

private:
    QLineEdit*   m_edName = nullptr;
    QListWidget* m_list = nullptr;
};
