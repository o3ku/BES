// DlgComponent.h —— 定制构件管理（对应原 TComCustomForm）
//
// 布局取自 BesCom.dll 内的 DFM（tools/forms.dfm.txt「ComCustomForm」）：
//   ComListBox(11,13,136,285) + GroupBox「构件属性」(153,11,308,257)
//   ComNameEdit(8,39,216,20) MaxLength=32、ComFileEdit(8,83,266,20) 只读、
//   BrowseBtn(275,83,26,21)「…」只负责浏览、ComInfoMemo(8,125,289,124) 只读，
//   底部 添加/删除/修改/退出 四个按钮，其中前三个初始时禁用。
#pragma once

#include <QDialog>
#include <QString>

#include "plugin/ComponentHost.h"

class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class DlgComponent : public QDialog {
    Q_OBJECT
public:
    DlgComponent(QWidget* parent, bes::ComponentHost& host);

    // 原版 FormCreate：列表非空时选中第 0 项并触发一次 ComListBoxClick
    void InitFromHost();

signals:
    // 原版 AddBtnClick 写事件表（该行的文本为 'Com'）
    void ComponentRegistered(const QString& name);

private slots:
    void OnBrowse();
    void OnAdd();
    void OnDelete();
    void OnModify();
    void OnSelectionChanged();

private:
    void Reload();
    void ShowComponent(const bes::ComponentInfo& ci);
    void ShowPending(const QString& name, const QString& file, const QString& info);
    // 原版 ComInfoText：作者/版本/简要说明/构件类型/物理端口数量 + 逐端口字段
    QString InfoText(const bes::InstanceInfo& info) const;
    bes::ComponentInfo* CurrentInfo();

    bes::ComponentHost& m_host;
    QListWidget*    m_list = nullptr;
    QLineEdit*      m_edName = nullptr;
    QLineEdit*      m_edFile = nullptr;
    QPlainTextEdit* m_edInfo = nullptr;
    QPushButton*    m_btnAdd = nullptr;
    QPushButton*    m_btnDel = nullptr;
    QPushButton*    m_btnEdit = nullptr;

    // 原版 Self+0x308：待「添加」的源文件（复制到 com\ 后注册副本）
    QString      m_pendingSrc;
    std::wstring m_selectedProbe;   // 「浏览」探测后选定的构件注册名
    bool         m_updating = false;
};
