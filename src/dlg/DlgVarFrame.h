// DlgVarFrame.h —— 不等长帧查看/处理（帧标记插入/删除/编辑，按前导码检测）
#pragma once

#include <QDialog>
#include <QVector>

#include <atomic>

#include "core/FrameModel.h"
#include "core/FileMap.h"

class QTableWidget;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QListWidget;
class QProgressBar;
class QLabel;
class QGroupBox;

class DlgVarFrame : public QDialog {
    Q_OBJECT
public:
    DlgVarFrame(QWidget* parent, const bes::FileMap& map,
                const bes::FrameModel& frame, bool processMode = false);

    // 编辑后的不等长帧集合
    const bes::VariableFrameSet& Frames() const { return m_frames; }
    // 是否应用（确定）
    bool Applied() const { return m_applied; }
    // 编辑后的帧模型（含新起点）
    const bes::FrameModel& Frame() const { return m_frame; }

    // 读入 BES-BNT 帧表（不等长帧查看/处理输出）
    bool LoadFrameFile(const QString& path, QString* err = nullptr);

protected:
    void closeEvent(QCloseEvent* ev) override;

private slots:
    void OnInsert();       // 插入帧标记
    void OnDelete();       // 删除帧标记
    void OnSetStart();     // 设为起点
    void OnLocate();       // 定位到选中帧
    void OnSplit();        // 按帧长切分
    void OnLoadFrames();   // 读入帧表文件
    void OnStart();        // 按前导码开始处理
    void OnStop();         // 中止处理
    void OnBrowse();       // 选择输出文件
    void OnPresetAdd();    // 添加预置
    void OnPresetDelete(); // 删除预置
    void OnPresetClicked();
    void OnFormatChanged();
    void OnApply();        // 应用（写入结果）
    void OnOk();

private:
    void Reload();
    void RebuildUniform(uint64_t lenBits);
    bool ParseCode(std::vector<uint8_t>& out, QString* err) const;
    bool WriteOutput(const QString& path, const QString& codeText,
                     const std::vector<uint64_t>& starts, QString* err) const;
    void LoadPresets();
    void SavePresets();
    void SetRunning(bool running);
    void UpdateStartEnabled();

    const bes::FileMap*    m_map = nullptr;
    bes::FrameModel        m_frame;
    bes::VariableFrameSet  m_frames;
    bool                   m_applied = false;
    bool                   m_processMode = false;
    bool                   m_running = false;
    std::atomic<bool>      m_stop{false};

    QTableWidget* m_table = nullptr;
    QLineEdit*    m_edFrameLen = nullptr;
    QLineEdit*    m_edPrecode = nullptr;
    QLineEdit*    m_edErr = nullptr;
    QLineEdit*    m_edPresetName = nullptr;
    QLineEdit*    m_edOutFile = nullptr;
    QComboBox*    m_cboFormat = nullptr;
    QCheckBox*    m_chkAmb = nullptr;
    QListWidget*  m_presets = nullptr;
    QProgressBar* m_prog = nullptr;
    QLabel*       m_info = nullptr;
    QGroupBox*    m_paramGroup = nullptr;
    QGroupBox*    m_frameGroup = nullptr;
    QPushButton*  m_btnInsert = nullptr;
    QPushButton*  m_btnDelete = nullptr;
    QPushButton*  m_btnSetStart = nullptr;
    QPushButton*  m_btnLocate = nullptr;
    QPushButton*  m_btnStart = nullptr;
    QPushButton*  m_btnStop = nullptr;
    QPushButton*  m_btnPresetAdd = nullptr;
    QPushButton*  m_btnPresetDel = nullptr;
    QPushButton*  m_btnApply = nullptr;
    QPushButton*  m_btnOk = nullptr;
    QPushButton*  m_btnCancel = nullptr;
};
