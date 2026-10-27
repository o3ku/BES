// DlgProcess.h —— 构件数据流处理（对应原 TProcessForm）
//
// 逆向依据（BesCom.dll）：
//   VMT 0x5D86A0，实例大小 0x3A4；字段表 0x5D8784 给出各行/按钮字段；
//   方法表 0x5D896F：FormCreate 0x413A08、RunBtnClick 0x415B94、
//   PauseBtnClick 0x4161F8、StopBtnClick 0x416250、ResetBtnClick 0x41629C、
//   CancelBtnClick 0x4162AC、FormClose 0x416308、SelectModeRowChange 0x4163B0、
//   PriorityComboBoxChange 0x416590、SelectInfoRowValidate 0x416614。
// 数据流推送由 `bes::FlowEngine`（src/core/FlowEngine.h）承担，端口由调用方绑定。
#pragma once

#include <QDialog>
#include <cstdint>

#include "core/FlowEngine.h"
#include "core/FrameModel.h"
#include "core/Selection.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

class DlgProcess : public QDialog {
    Q_OBJECT
public:
    // 输入选择（原版 SelectModeRow 的值 0..4，字面量 0x5D761F..0x5D7656）
    enum Mode { kAll = 0, kContinuous, kBlock, kRow, kCol };

    // comName 为空 = 未绑定构件（自定义参数/自定义显示两行不可用）
    DlgProcess(QWidget* parent, const QString& comName, const bes::SelectionSet& sel);

    // ---- 处理参数（对应各行 +0xD0 / +0xB0 / +0x88 取值）----
    Mode    inputMode() const;
    bool    deselect() const;            // 反选（值 '1'/'0'，0x48F2F0 写 +0xB0）
    QString paramText() const;           // 选取参数
    int     dataTypeValue() const;       // 数据类型：0..14，255=未知数据类型
    QString dataTypeName() const;
    bool    outFlagEoi() const;          // 输出标志 0=EOI、1=EOF
    int     priorityValue() const;       // 0x5D7600 表 {-15,-2,-1,0,+1}
    bool    autoQuit() const;
    bool    autoOpen() const;
    QString outputFile() const;          // 「文件: temp.dat」/「不输出」

    // 构件能力（原版从 [ctx+0x74] 端口/构件类型推出，本版本由调用方给定）
    void setDeselectForCol(bool on) { m_deselectForCol = on; applyMode(); }
    void setManualInputAllowed(bool on) { m_manualInput = on; applyMode(); }
    void setOutputPort(bool on);
    void setResetSupported(bool on) { m_resetSupported = on; }  // [form+0x344]

    // ---- 数据流管线 ----
    // 原版 [form+0x360] 是主窗体传入的数据流上下文（其 +0x14 为线程句柄），
    // 「开始」只是置运行位；本版本用同一线程模型不成立，故由引擎在按钮回调里同步搬运。
    void setSource(const bes::FileMap* map, const bes::SelectionSet& sel,
                   const bes::FrameModel& fm, int bitsPerCell);
    void bindFlowPort(bes::IFlowPort* port) { m_engine.BindPort(port); }
    const bes::FlowEngine& engine() const { return m_engine; }
    bes::FlowParams flowParams() const;    // 各行取值 → 运行参数

    // ---- 运行状态机 ----
    bool running() const { return m_running; }
    bool paused() const  { return m_paused; }
    bool exitRequested() const { return m_exit; }
    // 校验：mode≠0 且选取参数为空 → 「选取参数不能为空！」（0x5D7670）
    bool validate(QString* err) const;
    bool startRun();                     // 「开始」

    void setProgress(uint64_t done, uint64_t total);   // 原版进度条 0..256
    void setInfo(const QString& text);

private:
    void enterIdle();                    // 0x415650：仅「开始」可用，进度条隐藏
    void applyMode();                    // SelectModeRowChange 的启用规则
    void onPause();
    void onStop();
    void onReset();
    void onQuit();

    QString m_comName;

    QPushButton* m_run = nullptr;
    QPushButton* m_pause = nullptr;      // 原版 TSpeedButton：AllowAllUp + GroupIndex 1
    QPushButton* m_stop = nullptr;
    QPushButton* m_reset = nullptr;
    QPushButton* m_quit = nullptr;

    QComboBox* m_priority = nullptr;
    QCheckBox* m_autoQuit = nullptr;
    QCheckBox* m_autoOpen = nullptr;

    QComboBox* m_mode = nullptr;
    QCheckBox* m_deselect = nullptr;
    QLineEdit* m_param = nullptr;
    QComboBox* m_dataType = nullptr;
    QComboBox* m_outFlag = nullptr;
    QLineEdit* m_outFile = nullptr;
    QPushButton* m_customParam = nullptr;
    QPushButton* m_customView = nullptr;

    QLabel*       m_info = nullptr;
    QProgressBar* m_bar = nullptr;

    bool m_running = false;
    bool m_paused = false;
    bool m_exit = false;
    bool m_hasOutputPort = false;
    bool m_resetSupported = true;       // 原版 [form+0x344]：构造里写 1（@0x413984）
    bool m_deselectForCol = false;       // 列选取是否允许反选（构件类型 7）
    bool m_manualInput = false;          // 选取参数是否接受手工输入

    bes::FlowEngine          m_engine;
    const bes::FileMap*      m_map = nullptr;
    const bes::SelectionSet* m_sel = nullptr;
    bes::FrameModel          m_fm;
    int                      m_bitsPerCell = 8;
};
