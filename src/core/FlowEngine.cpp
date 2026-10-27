// FlowEngine.cpp
#include "core/FlowEngine.h"

#include <algorithm>
#include <cstring>

namespace bes {

namespace {

struct ByteSpan { uint64_t start; uint64_t len; };

// 合并重叠/相邻的字节区间：反选取补与多段选区都依赖它
void MergeSpans(std::vector<ByteSpan>& spans)
{
    std::sort(spans.begin(), spans.end(),
              [](const ByteSpan& a, const ByteSpan& b) { return a.start < b.start; });
    std::vector<ByteSpan> merged;
    for (const ByteSpan& s : spans) {
        if (s.len == 0) continue;
        if (!merged.empty() && s.start <= merged.back().start + merged.back().len) {
            const uint64_t end = std::max(s.start + s.len, merged.back().start + merged.back().len);
            merged.back().len = end - merged.back().start;
        } else {
            merged.push_back(s);
        }
    }
    spans.swap(merged);
}

} // namespace

void FlowEngine::CollectSpans(const FileMap& map, const FrameModel& fm,
                              const SelectionSet& sel, const FlowParams& params,
                              std::vector<std::pair<uint64_t, uint64_t>>& spans)
{
    spans.clear();
    const uint64_t totalBits = map.SizeBits();
    if (!totalBits) return;
    const int bpc = params.bitsPerCell > 0 ? params.bitsPerCell : 8;
    const uint64_t base = fm.startBit;
    auto frameStart = [&](uint64_t f) { return base + f * fm.frameLen; };

    // 比特区间（起点、长度）
    std::vector<std::pair<uint64_t, uint64_t>> bits;
    switch (params.mode) {
    case SelectMode::None:                          // 全部数据
        if (totalBits > base) bits.push_back({ base, totalBits - base });
        break;
    case SelectMode::Continuous:
        for (const ContinuousSel& s : sel.Continuouses())
            if (s.endBit >= s.startBit)
                bits.push_back({ s.startBit, s.endBit - s.startBit + 1 });
        break;
    case SelectMode::Rect:
        for (const RectSel& r : sel.Rects()) {
            if (r.cell1 < r.cell0 || r.frame1 < r.frame0) continue;
            const uint64_t wide = (r.cell1 - r.cell0 + 1) * uint64_t(bpc);
            for (uint64_t f = r.frame0; f <= r.frame1; ++f)
                bits.push_back({ frameStart(f) + r.cell0 * uint64_t(bpc), wide });
        }
        break;
    case SelectMode::Row:
        for (uint64_t row : sel.Rows()) {
            const uint64_t len = fm.frameLen ? fm.frameLen : totalBits - base;
            bits.push_back({ frameStart(row), len });
        }
        break;
    case SelectMode::Col: {
        const uint64_t frameLen = fm.frameLen ? fm.frameLen : totalBits - base;
        const uint64_t frames = fm.frameLen && frameLen ? (totalBits - base) / frameLen : 1;
        for (uint64_t col : sel.Cols())
            for (uint64_t f = 0; f < frames; ++f)
                bits.push_back({ frameStart(f) + col * uint64_t(bpc), uint64_t(bpc) });
        break;
    }
    default:
        break;
    }

    // 反选：在 [base, totalBits) 内取补集（原版 DeSelectRow 的值写入 [form+0x348]）
    if (params.deselect) {
        std::vector<std::pair<uint64_t, uint64_t>> sorted = bits;
        std::sort(sorted.begin(), sorted.end());
        uint64_t cursor = base;
        std::vector<std::pair<uint64_t, uint64_t>> rest;
        for (auto& b : sorted) {
            if (b.first > cursor) rest.push_back({ cursor, b.first - cursor });
            cursor = std::max(cursor, b.first + b.second);
        }
        if (cursor < totalBits) rest.push_back({ cursor, totalBits - cursor });
        bits.swap(rest);
    }

    // 比特 → 字节（与 OutputWriter::emit 同一换算），裁剪到文件末尾后合并
    std::vector<ByteSpan> bytes;
    for (const auto& b : bits) {
        if (b.first >= totalBits || b.second == 0) continue;
        const uint64_t count = std::min(b.second, totalBits - b.first);
        const uint64_t startByte = b.first >> 3;
        uint64_t endByte = (b.first + count + 7) >> 3;
        if (startByte >= map.SizeBytes()) continue;
        endByte = std::min(endByte, map.SizeBytes());
        if (endByte > startByte) bytes.push_back({ startByte, endByte - startByte });
    }
    MergeSpans(bytes);

    // 按 [ctx+0x8c] 的块上限切分
    const uint64_t block = params.blockBytes ? params.blockBytes : 1;
    for (const ByteSpan& s : bytes)
        for (uint64_t off = 0; off < s.len; off += block)
            spans.push_back({ s.start + off, std::min(block, s.len - off) });
}

bool FlowEngine::Start(const FileMap& map, const FrameModel& fm, const SelectionSet& sel,
                       const FlowParams& params, std::wstring* err)
{
    if (m_state == FlowState::Running || m_state == FlowState::Paused) return false;
    if (!map.IsOpen()) {
        if (err) *err = L"数据长度为0！";
        return false;
    }
    if (!m_port) {
        if (err) *err = L"没有可用的数据流输入端口！";
        return false;
    }

    m_params = params;
    m_state = FlowState::Running;     // 原版：[ctx+0x19] := 1
    m_exit = false;                   // 原版：[ctx+0x18]
    m_command = 0;
    m_written = 0;

    std::vector<std::pair<uint64_t, uint64_t>> spans;
    CollectSpans(map, fm, sel, m_params, spans);
    m_total = 0;
    for (const auto& s : spans) m_total += s.second;

    m_port->Begin(m_params);
    bool ok = true;
    for (const auto& s : spans) {
        // 暂停：等待 Resume（无回调可驱动时直接收尾，避免自锁）
        while (m_state == FlowState::Paused && !m_exit) {
            if (m_pump) m_pump(*this);
            else break;
        }
        if (m_exit || m_state == FlowState::Stopped) { ok = false; break; }
        if (s.first + s.second > map.SizeBytes()) { ok = false;
            if (err) *err = L"端口输入失败！";
            break;
        }
        m_buf.resize(static_cast<size_t>(s.second));
        std::memcpy(m_buf.data(), map.Base() + s.first, static_cast<size_t>(s.second));
        if (!m_port->Write(m_buf.data(), m_buf.size())) {
            ok = false;
            if (err) *err = L"端口输入失败！";
            break;
        }
        m_written += s.second;
        if (m_pump) m_pump(*this);
    }
    m_port->End(m_params);
    m_port->Close();

    if (m_state != FlowState::Stopped)
        m_state = ok ? FlowState::Finished : FlowState::Stopped;
    return ok;
}

void FlowEngine::Pause()
{
    if (m_state != FlowState::Running) return;
    m_state = FlowState::Paused;
    m_command = 3;                    // 构件命令 3（@0x416228）
}

void FlowEngine::Resume()
{
    if (m_state == FlowState::Paused) m_state = FlowState::Running;
    m_command = 4;                    // 构件命令 4（@0x41623C）
}

void FlowEngine::Stop()
{
    if (m_state == FlowState::Idle) return;
    m_state = FlowState::Stopped;     // 进度保留，与原版一致
}

void FlowEngine::Reset()
{
    m_state = FlowState::Idle;
    m_exit = false;
    m_written = 0;
    m_total = 0;
    m_command = 5;                    // 构件命令 5（@0x4162A7）
}

void FlowEngine::RequestExit()
{
    m_exit = true;                    // [ctx+0x18] := 1
    if (m_state == FlowState::Running || m_state == FlowState::Paused)
        m_state = FlowState::Stopped;
}

} // namespace bes
