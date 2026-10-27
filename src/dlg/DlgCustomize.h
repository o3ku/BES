// DlgCustomize.h —— 工具栏定制（对应原版 TdxBarCustomizingForm）
#pragma once

#include <QDialog>
#include <QVector>

class MainWindow;
class QCheckBox;
class QComboBox;
class QListWidget;
class QPushButton;

class DlgCustomize : public QDialog {
    Q_OBJECT
public:
    explicit DlgCustomize(MainWindow* mw);

private:
    void FillAvailable();
    void LoadBar(int bar);
    QVector<int> CurrentItems() const;
    void UpdateButtons();
    void ResetAll();

    MainWindow*  m_mw = nullptr;
    QVector<QCheckBox*> m_barChecks;
    QComboBox*   m_barCombo = nullptr;
    QListWidget* m_current = nullptr;
    QListWidget* m_available = nullptr;
    QPushButton* m_add = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
    QPushButton* m_reset = nullptr;
};
