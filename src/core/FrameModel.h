// FrameModel.h —— 帧模型（等长帧 / 不等长帧）
#pragma once

#include <cstdint>
#include <vector>

namespace bes {

// 原程序的「参数设置」中有：起点、帧长、步进（单位比特）。
// 不等长帧模式下帧长由搜索/迭代产生，帧边界由 FrameBoundaries 单独维护。
struct FrameModel {
    uint64_t startBit  = 0;   // 数据起点（比特）
    uint64_t frameLen  = 0;   // 帧长（比特） 0 = 未设置
    uint64_t step      = 1;   // 步进（比特）
    bool     bVariable = false; // true = 不等长帧

    // 帧长×步进上限保护（原程序提示：帧长×步进大于(2^31-1)，步进将设为默认值1）
    static const uint64_t kMaxFrameStep = 0x7FFFFFFFull;

    // 按原程序规则修正步进
    void SanitizeStep() {
        if (step == 0) step = 1;
        if (frameLen == 0) return;   // 自动帧长：步进仍然有效
        if (frameLen > kMaxFrameStep / step)
            step = 1;
    }

    // 行数按帧长计算；step 只用于迭代/光标推进，不参与帧布局
    uint64_t FrameCount(uint64_t totalBits) const {
        if (frameLen == 0 || totalBits <= startBit)
            return 0;
        return (totalBits - startBit + frameLen - 1) / frameLen;
    }

    // 绝对比特位置 -> 帧号
    uint64_t FrameOf(uint64_t bitPos) const {
        if (frameLen == 0)
            return 0;
        if (bitPos <= startBit)
            return 0;
        return (bitPos - startBit) / frameLen;
    }

    // 绝对比特位置 -> 帧内比特偏移
    uint64_t BitInFrame(uint64_t bitPos) const {
        if (frameLen == 0)
            return 0;
        if (bitPos <= startBit)
            return 0;
        return (bitPos - startBit) % frameLen;
    }
};

// 不等长帧：显式保存每帧的 [startBit, len)
struct VariableFrame {
    uint64_t startBit = 0;
    uint64_t length   = 0;   // 比特
};

class VariableFrameSet {
public:
    void Clear() { m_frames.clear(); }
    void Add(uint64_t startBit, uint64_t length) { m_frames.push_back({ startBit, length }); }
    const std::vector<VariableFrame>& Frames() const { return m_frames; }
    std::vector<VariableFrame>& Frames() { return m_frames; }
    size_t Size() const { return m_frames.size(); }

    // 返回包含该比特位置的帧索引；找不到返回 -1
    int64_t IndexOf(uint64_t bitPos) const {
        // 帧有序，二分
        size_t lo = 0, hi = m_frames.size();
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            const VariableFrame& f = m_frames[mid];
            if (bitPos < f.startBit)            hi = mid;
            else if (bitPos >= f.startBit + f.length) lo = mid + 1;
            else                                  return static_cast<int64_t>(mid);
        }
        return -1;
    }

private:
    std::vector<VariableFrame> m_frames;
};

} // namespace bes
