// DlgFilter.h —— 帧过滤（对应原版 TFilterForm）
#pragma once

#include <QDialog>
#include <vector>

#include "core/SearchEngine.h"

namespace bes { class FileMap; class VariableFrameSet; }

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QTabWidget;

class DlgFilter : public QDialog {
    Q_OBJECT
public:
    DlgFilter(QWidget* parent, const bes::FileMap& map, const bes::FrameModel& fm,
              const bes::FrameFilter* existing = nullptr,
              const bes::VariableFrameSet* vframes = nullptr);

    const bes::FrameFilter& Filter() const { return m_filter; }
    const std::vector<uint64_t>& Matched() const { return m_matched; }
    bool ApplyToView() const { return m_filter.Active(); }

signals:
    void FilterApplied();

private slots:
    void OnAdd();
    void OnEdit();
    void OnDelete();
    void OnClear();
    void OnApply();
    void OnOk();
    void OnSelectionChanged();
    void OnDataTypeChanged();

private:
    bool BuildFromControls(bes::FilterCondition& cond, QString& desc);
    bool RunFilter();
    void LoadRow(int row);
    void SyncButtons();

    const bes::FileMap* m_map = nullptr;
    bes::FrameModel     m_frame;
    const bes::VariableFrameSet* m_vframes = nullptr;
    bes::FrameFilter    m_filter;
    std::vector<bes::FilterCondition> m_conds;
    std::vector<uint64_t> m_matched;
    bool m_needRun = false;

    QTabWidget*    m_tabs = nullptr;
    QComboBox*     m_frameJudge = nullptr;
    QLineEdit*     m_frameEdit = nullptr;
    QLineEdit*     m_dataPos = nullptr;
    QComboBox*     m_dataType = nullptr;
    QComboBox*     m_dataJudge = nullptr;
    QLineEdit*     m_dataEdit = nullptr;
    QRadioButton*  m_crcOk = nullptr;
    QRadioButton*  m_crcError = nullptr;
    QListWidget*   m_list = nullptr;
    QLabel*        m_status = nullptr;
    QPushButton*   m_btnEdit = nullptr;
    QPushButton*   m_btnDel = nullptr;
    QPushButton*   m_btnClear = nullptr;
    QPushButton*   m_btnApply = nullptr;
};
