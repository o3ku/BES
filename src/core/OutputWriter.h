// OutputWriter.h —— 数据输出（输出到文件 / 输出到端口）
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/FileMap.h"
#include "core/FrameModel.h"
#include "core/Selection.h"

namespace bes {

enum class OutputTarget {
    None = 0,
    File,
    Port
};

// 原程序「输出设置」：不输出 / 输出到文件 / 输出到端口(1..32)
struct OutputOptions {
    OutputTarget target = OutputTarget::None;
    std::wstring filePath;
    int  portIndex = 0;          // 1..32
    bool wholeData = true;       // 全部数据
    bool continuousSel = false;  // 连续选取
    bool rectSel = false;        // 块选取
    bool rowSel = false;         // 行选取
    bool colSel = false;         // 列选取
    bool autoOpenOutput = false; // 自动打开输出文件
    int  bitsPerCell = 8;        // 当前显示模式每格比特数（1=比特模式，8=字节模式）
};

// 端口输出接口（预留：实际端口由构件提供）
class IOutputPort {
public:
    virtual ~IOutputPort() = default;
    virtual bool Write(const uint8_t* data, size_t len) = 0;
    virtual void Close() {}
};

class OutputWriter {
public:
    // 按选项输出。返回实际写出的字节数；失败时 err 非空。
    bool Write(const FileMap& map, const FrameModel& fm, const SelectionSet& sel,
               const OutputOptions& opt, uint64_t& bytesWritten, std::wstring* err);

    // 不用 SetPort 作名字：windows.h 会把 SetPort 宏映射到 SetPortA
    void BindPort(int index, IOutputPort* port) { m_ports[index] = port; }

private:
    IOutputPort* m_ports[33] = { nullptr };
};

} // namespace bes
