// FlowEngine.h —— 构件数据流输入管线（原版 TProcessForm「开始」之后的推送过程）
//
// 反汇编依据（BesCom.dll，TProcessForm VMT 0x5D86A0，实例大小 0x3A4）：
//   · 构造 @0x413844：[form+0x360] := 由 TMainForm 传入的数据流上下文（@0x410689 push [[esi]]），
//     [form+0x364]（构件实例）:= nil，[form+0x344]（支持复位）:= 1，[form+0x358] := 100。
//   · RunBtnClick 尾部 @0x4161D7：`mov eax,[form+0x360]` / `mov byte [eax+0x19],1`
//     —— 「开始」只置上下文运行位，真正搬运数据的是该上下文拥有的线程（[ctx+0x14] 是线程句柄）。
//   · PauseBtnClick @0x4161F8：按下 → SuspendThread([ctx+0x14])（@0x416213）+ 构件命令 3（@0x416228）；
//     弹起 → 构件命令 4（@0x41623C）+ ResumeThread([ctx+0x14])（@0x416248）。
//   · ResetBtnClick @0x41629C：构件命令 5（@0x4162A7）。命令走构件对象 vtable 槽 +0x28（stdcall，参数为命令号）。
//   · PriorityComboBoxChange @0x416590：把 0x5D7600 表里的优先级数值写入 vtable 槽 +0x30。
//   · 自定义显示/自定义参数分别走槽 +0x34（@0x41660E 传 1/2，@0x416133 传 101,0,0）与 +0x18（@0x41714A/@0x4174A9）。
//   · 上下文块大小字段 [ctx+0x8c] 初值 0x10000（@0x41067B），即每次搬运的块上限。
//
// 本版本的构件仍只有 4 个导出（RegComNum/RegComInfo/CreateInstance/ShowModal），
// 端口按现有约定以临时文件承载（见 MainWindow 的 TempFilePort），故管线把选区字节推到 IFlowPort。
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/DataTypes.h"
#include "core/FileMap.h"
#include "core/FrameModel.h"
#include "core/Selection.h"

namespace bes {

// 一次运行的参数，字段与 TProcessForm 的私有字段一一对应
struct FlowParams {
    SelectMode   mode      = SelectMode::None;  // +0x345 输入选择
    bool         deselect  = false;             // +0x348 反选
    bool         outEoi    = true;              // +0x34c 输出标志（EOI/EOF）
    int          dataType  = 0;                 // +0x350 数据类型（16 项表的数值，未知=255）
    std::wstring param;                         // +0x354 选取参数
    int          priority  = 0;                 // 0x5D7600：{-15,-2,-1,0,1}
    uint64_t     blockBytes = 0x10000;          // [ctx+0x8c] 初值 65536
    int          bitsPerCell = 8;               // 每显示格比特数（1=比特模式，8=字节模式）
};

// 构件的数据流输入端口（与 OutputWriter 的 IOutputPort 对称）
class IFlowPort {
public:
    virtual ~IFlowPort() = default;
    // 原版把 数据类型/输出标志/优先级 作为端口属性下发，故先 Begin 再写数据
    virtual void Begin(const FlowParams& params) { (void)params; }
    virtual bool Write(const uint8_t* data, size_t len) = 0;
    virtual void End(const FlowParams& params) { (void)params; }
    virtual void Close() {}
};

enum class FlowState { Idle = 0, Running, Paused, Stopped, Finished };

class FlowEngine {
public:
    // 单线程实现：每搬一块回调一次，UI 在其中刷新进度、处理事件并决定暂停/停止/退出
    using Pump = std::function<void (const FlowEngine&)>;

    void BindPort(IFlowPort* port) { m_port = port; }
    IFlowPort* Port() const { return m_port; }
    void SetPump(Pump pump) { m_pump = std::move(pump); }

    // 校验取自 RunBtnClick：0x415C15..0x415C70（选取参数为空）、0x415D8E（文件名）、端口可用性
    bool Start(const FileMap& map, const FrameModel& fm, const SelectionSet& sel,
               const FlowParams& params, std::wstring* err);

    void Pause();       // SuspendThread + 命令 3
    void Resume();      // ResumeThread   + 命令 4
    void Stop();        // 停止搬运，进度保留
    void Reset();       // 命令 5，回到空闲
    void RequestExit(); // [ctx+0x18] := 1

    FlowState state() const { return m_state; }
    bool exitRequested() const { return m_exit; }
    uint64_t bytesWritten() const { return m_written; }
    uint64_t totalBytes() const { return m_total; }
    int comCommand() const { return m_command; }   // 最近一次发给构件的命令号
    const FlowParams& params() const { return m_params; }

    // 选区 → 比特区间（已按反选取补、按块上限切分），供 Start 与测试复用
    static void CollectSpans(const FileMap& map, const FrameModel& fm,
                             const SelectionSet& sel, const FlowParams& params,
                             std::vector<std::pair<uint64_t, uint64_t>>& spans);

private:
    IFlowPort*   m_port = nullptr;
    Pump         m_pump;
    FlowParams   m_params;
    FlowState    m_state = FlowState::Idle;
    bool         m_exit = false;
    uint64_t     m_written = 0;
    uint64_t     m_total = 0;
    int          m_command = 0;
    std::vector<uint8_t> m_buf;
};

} // namespace bes
