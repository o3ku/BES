// SearchEngine.h —— 搜索/过滤（独特码查找、帧长查找、帧过滤、数值过滤）
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/FileMap.h"
#include "core/FrameModel.h"

namespace bes {

enum class UniqueCodeFormat {
    Binary = 0,
    Hex,
    Char
};

// 独特码参数（对应原程序 ProcessForm / 不等长帧处理）
struct UniqueCodeSpec {
    std::vector<uint8_t> code;      // 已转换成的比特序列（每字节 8 位）
    int   errorBits = 0;            // 允许误码比特数
    bool  removeAmbiguity = false;  // 去除四相模糊
    int   lengthBits = 0;           // 独特码长度（比特）

    bool Validate(std::wstring* err);
};

// 帧过滤条件
enum class CmpOp { Eq, Gt, Lt, Ne };
const wchar_t* CmpOpName(CmpOp op);

// 对应原版 TFilterForm 的三类条件（列表中的每一项）
enum class FilterKind { FrameLength = 0, Value = 1, Crc = 2 };

struct FilterCondition {
    FilterKind kind = FilterKind::FrameLength;

    // 帧长：界面按字节输入，内部统一按比特保存
    int    frameBits = 0;
    CmpOp  frameOp = CmpOp::Eq;

    // 数值：比特类型的位置按比特计，其余类型的位置按字节计；
    // 数值按原版要求从低字节到高字节输入（首字节为最低字节）。
    int      valuePos = 0;
    int      valueBits = 8;   // 1/8/16/32
    uint64_t value = 0;
    CmpOp    valueOp = CmpOp::Eq;

    // CRC：校验帧尾最后 2 字节（CRC-16/CCITT-FALSE，高字节在前）
    bool crcPass = true;      // true = CRC 通过，false = CRC 失败
};

struct FrameFilter {
    std::vector<FilterCondition> conditions;   // 各条件之间为“与”关系
    bool Active() const { return !conditions.empty(); }
};

// 搜索结果
struct SearchHit {
    uint64_t bitPos = 0;
};

// 帧长查找参数（对应原版 TSearchFlForm）
struct FrameLengthSpec {
    uint64_t startLenBits = 2;  // 候选帧长起点
    uint64_t originBit = 0;     // 检测行的起点（当前帧起点）
    int rowCount = 30;          // 检测行数
    int minSyncCols = 2;        // 最少同步列数（全0/1列）
};

struct FrameLengthResult {
    uint64_t frameLenBits = 0;  // 0 = 未找到
    uint64_t syncColumns = 0;   // 该帧长下的全0/1列数
};

// 搜索引擎：通过回调报告进度，stopFlag 置 true 可中止。
class SearchEngine {
public:
    using ProgressFn = std::function<void(uint64_t done, uint64_t total, bool finished)>;

    // 查找独特码（支持任意长度，错误比特数，去四相模糊）。
    bool FindUniqueCode(const FileMap& map, const UniqueCodeSpec& spec,
                        uint64_t fromBit, bool wrap,
                        std::vector<SearchHit>& hits,
                        const ProgressFn& onProgress,
                        const std::atomic<bool>& stopFlag);

    // 查找帧长（原版算法）：从 startLenBits 起逐比特尝试候选帧长，
    // 以 originBit 为行起点、rowCount 行为一组统计全0/1列，
    // 返回首个同步列数 >= minSyncCols 的帧长。
    bool FindFrameLength(const FileMap& map, const FrameLengthSpec& spec,
                         FrameLengthResult& result,
                         const ProgressFn& onProgress,
                         const std::atomic<bool>& stopFlag);

    // 帧过滤：返回满足条件的帧号列表
    bool ApplyFilter(const FileMap& map, const FrameModel& fm, const FrameFilter& filter,
                     std::vector<uint64_t>& frameIndices,
                     const ProgressFn& onProgress,
                     const std::atomic<bool>& stopFlag);

    // 不等长帧过滤：帧号即 VariableFrameSet 的下标，帧长条件按每帧自身长度判定
    bool ApplyFilter(const FileMap& map, const VariableFrameSet& frames, const FrameFilter& filter,
                     std::vector<uint64_t>& frameIndices,
                     const ProgressFn& onProgress,
                     const std::atomic<bool>& stopFlag);
};

} // namespace bes
