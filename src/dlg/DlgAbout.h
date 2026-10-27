// DlgAbout.h —— 关于对话框（对应原版 TAboutBox）
#pragma once

#include <QDialog>

class DlgAbout : public QDialog {
    Q_OBJECT
public:
    explicit DlgAbout(QWidget* parent = nullptr);
};
