// DlgOutputSet.h —— 输出设置（端口树 + 不输出 / 文件 / 端口 + 输出内容）
#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

#include "core/OutputWriter.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;

class DlgOutputSet : public QDialog {
    Q_OBJECT
public:
    explicit DlgOutputSet(QWidget* parent = nullptr);

    const bes::OutputOptions& Options() const { return m_opt; }
    void SetBitsPerCell(int b) { m_opt.bitsPerCell = b; }
    // indices 为构件实际端口号；为空时按 1..32 通用端口
    void SetPortChoices(const QStringList& labels,
                        const QVector<int>& indices = QVector<int>());

private slots:
    void OnModeChanged();
    void OnBrowse();
    void OnPortTreeChanged();
    void OnPortClicked(int id);

private:
    void ReadControls();
    void FillPortChoices();
    void UpdatePortEnabled();
    void SelectPortRadio(int port, bool syncTree);
    int  FirstAvailablePort() const;

    bes::OutputOptions m_opt;
    QStringList m_portLabels;
    QVector<int> m_portIndices;
    bool m_componentPorts = false;   // true: 仅构件端口可用
    bool m_updatingPorts = false;

    QTreeWidget*  m_tree = nullptr;
    QRadioButton* m_rbNone = nullptr;
    QRadioButton* m_rbFile = nullptr;
    QRadioButton* m_rbPort = nullptr;
    QLineEdit*    m_edFile = nullptr;
    QLabel*       m_portNote = nullptr;
    QPushButton*  m_btnBrowse = nullptr;
    QButtonGroup* m_portGroup = nullptr;
    QRadioButton* m_portRadios[32] = { nullptr };
    QCheckBox*    m_chkWhole = nullptr;
    QCheckBox*    m_chkCont  = nullptr;
    QCheckBox*    m_chkRect  = nullptr;
    QCheckBox*    m_chkRow   = nullptr;
    QCheckBox*    m_chkCol   = nullptr;
};
