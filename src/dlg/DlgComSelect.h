// DlgComSelect.h —— 构件选择（对应原 TComSelectForm）
#pragma once

#include <QDialog>
#include <string>
#include <vector>

#include "plugin/ComponentHost.h"

class QListWidget;

class DlgComSelect : public QDialog {
    Q_OBJECT
public:
    DlgComSelect(QWidget* parent, const bes::ComponentHost& host);
    // 原版 BrowseBtnClick 用探测到的构件清单（而非已注册清单）弹出本窗口
    explicit DlgComSelect(QWidget* parent,
                          const std::vector<bes::ComponentInfo>& items);

    std::wstring SelectedName() const { return m_selected; }      // 显示名
    std::string  SelectedRegName() const { return m_selectedReg; } // 注册名(ANSI)

    // 原版 OKBtnClick 把实例信息写回 TComSelectForm 私有字段：
    // +0x2F4 = (comType == 2)、+0x2E4 = 唯一数据流输入端口 ID。
    bool IsDataSource() const { return m_dataSource; }
    int  DataInPortId() const { return m_dataInPortId; }
    bool HasOutputPort() const { return m_hasOutput; }
    int  PortCount() const { return m_portCount; }
    int  ComType() const { return m_comType; }
    const bes::InstanceInfo& Instance() const { return m_info; }

private:
    void Build(const std::vector<bes::ComponentInfo>& items);
    // 原版 OKBtnClick：CreateInstance → 读取实例信息 → 校验端口 → 关闭
    void OnOk();

    QListWidget* m_list = nullptr;
    std::vector<bes::ComponentInfo> m_items;
    bes::InstanceInfo m_info;
    std::wstring m_selected;
    std::string m_selectedReg;
    bool m_dataSource = false;
    bool m_hasOutput = false;
    int  m_dataInPortId = 0;
    int  m_portCount = 0;
    int  m_comType = 0;
};
