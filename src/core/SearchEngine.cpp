// SearchEngine.cpp
#include "core/SearchEngine.h"

#include <algorithm>
#include <cstring>

namespace bes {

const wchar_t* CmpOpName(CmpOp op) {
    switch (op) {
    case CmpOp::Eq: return L"等于";
    case CmpOp::Gt: return L"大于";
    case CmpOp::Lt: return L"小于";
    case CmpOp::Ne: return L"不等于";
    default:        return L"";
    }
}

bool UniqueCodeSpec::Validate(std::wstring* err) {
    if (code.empty()) {
        if (err) *err = L"独特码长度不能为0！";
        return false;
    }
    const int bits = static_cast<int>(code.size() * 8);
    if (bits < 4) {
        if (err) *err = L"独特码长度不能小于4比特！";
        return false;
    }
    if (removeAmbiguity && (bits % 2) != 0) {
        if (err) *err = L"去除相位模糊要求独特码长度必须为2的整数倍！";
        return false;
    }
    if (errorBits < 0) {
        if (err) *err = L"请输入小于独特码长度的误码比特数！";
        return false;
    }
    if (errorBits >= bits) {
        if (err) *err = L"请输入小于独特码长度的误码比特数！";
        return false;
    }
    lengthBits = bits;
    return true;
}

namespace {

inline uint8_t GetBit(const std::vector<uint8_t>& data, int bitIndex) {
    const uint8_t b = data[static_cast<size_t>(bitIndex / 8)];
    return static_cast<uint8_t>((b >> (7 - (bitIndex % 8))) & 1u);
}

uint16_t Crc16(const uint8_t* data, size_t len, int algorithm) {
    uint16_t crc = (algorithm == 2) ? 0x0000 : 0xFFFF;   // XMODEM init 0
    for (size_t i = 0; i < len; ++i) {
        if (algorithm == 1) {                            // MODBUS (reflected)
            crc ^= data[i];
            for (int b = 0; b < 8; ++b)
                crc = (crc & 1u) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001u)
                                 : static_cast<uint16_t>(crc >> 1);
        } else {                                         // CCITT-FALSE / XMODEM
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (int b = 0; b < 8; ++b)
                crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
                                      : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

std::vector<std::vector<uint8_t>> MakeVariants(const UniqueCodeSpec& spec) {
    std::vector<std::vector<uint8_t>> out;
    out.push_back(spec.code);
    if (!spec.removeAmbiguity)
        return out;

    const size_t n = spec.code.size();
    // 180° 相位：按位取反
    std::vector<uint8_t> inv(n);
    for (size_t i = 0; i < n; ++i)
        inv[i] = static_cast<uint8_t>(~spec.code[i]);

    // 90°/270° 相位：相邻比特交换（I/Q 交换）
    std::vector<uint8_t> sw(n, 0), invSw(n, 0);
    const int bits = static_cast<int>(n * 8);
    for (int i = 0; i + 1 < bits; i += 2) {
        const uint8_t a = GetBit(spec.code, i);
        const uint8_t b = GetBit(spec.code, i + 1);
        if (b) sw[static_cast<size_t>(i / 8)] |= static_cast<uint8_t>(0x80u >> (i % 8));
        if (a) sw[static_cast<size_t>(i / 8)] |= static_cast<uint8_t>(0x80u >> ((i + 1) % 8));
        if (!b) invSw[static_cast<size_t>(i / 8)] |= static_cast<uint8_t>(0x80u >> (i % 8));
        if (!a) invSw[static_cast<size_t>(i / 8)] |= static_cast<uint8_t>(0x80u >> ((i + 1) % 8));
    }
    out.push_back(inv);
    out.push_back(sw);
    out.push_back(invSw);
    return out;
}

} // namespace

bool SearchEngine::FindUniqueCode(const FileMap& map, const UniqueCodeSpec& spec,
                                   uint64_t fromBit, bool wrap,
                                   std::vector<SearchHit>& hits,
                                   const ProgressFn& onProgress,
                                   const std::atomic<bool>& stopFlag) {
    hits.clear();
    if (!map.IsOpen() || spec.code.empty())
        return false;

    const int codeBits = static_cast<int>(spec.code.size() * 8);
    const uint64_t totalBits = map.SizeBits();
    if (totalBits < static_cast<uint64_t>(codeBits))
        return false;

    const std::vector<std::vector<uint8_t>> variants = MakeVariants(spec);
    const uint64_t lastStart = totalBits - static_cast<uint64_t>(codeBits);
    const uint64_t total = lastStart + 1;

    uint64_t pos = (fromBit <= lastStart) ? fromBit : 0;
    const uint64_t end = lastStart;
    bool wrapped = (fromBit > lastStart);
    uint64_t done = 0;

    for (;;) {
        if (stopFlag.load()) return false;
        if (pos > end) {
            if (!wrap || wrapped) break;
            pos = 0;
            wrapped = true;
        }

        int best = codeBits + 1;
        for (const std::vector<uint8_t>& v : variants) {
            int errors = 0;
            for (int i = 0; i < codeBits; ++i) {
                const uint8_t actual = static_cast<uint8_t>(map.ReadBits(pos + static_cast<uint64_t>(i), 1));
                const uint8_t expect = GetBit(v, i);
                if (actual != expect) {
                    if (++errors > spec.errorBits) break;
                }
            }
            best = std::min(best, errors);
            if (best <= spec.errorBits) break;
        }

        if (best <= spec.errorBits) {
            SearchHit h;
            h.bitPos = pos;
            hits.push_back(h);
        }

        ++pos;
        ++done;
        if (onProgress && (done & 0xFFFF) == 0)
            onProgress(done, total, false);
    }
    if (onProgress) onProgress(done, total, true);
    return true;
}

bool SearchEngine::FindFrameLength(const FileMap& map, const FrameLengthSpec& spec,
                                    FrameLengthResult& result,
                                    const ProgressFn& onProgress,
                                    const std::atomic<bool>& stopFlag) {
    result = FrameLengthResult();
    if (!map.IsOpen())
        return false;

    const int rows = std::max(2, spec.rowCount);
    const int minCols = std::max(1, spec.minSyncCols);
    const uint64_t totalBits = map.SizeBits();
    const uint64_t origin = std::min(spec.originBit, totalBits);
    const uint64_t avail = totalBits - origin;
    if (avail == 0)
        return false;

    // 至少要有 rows 行才能检测
    const uint64_t maxLen = avail / static_cast<uint64_t>(rows);
    if (maxLen == 0)
        return false;

    const uint64_t start = std::max<uint64_t>(1, spec.startLenBits);
    if (start > maxLen) {
        if (onProgress) onProgress(0, 0, true);
        return true;
    }
    const uint64_t candidates = maxLen - start + 1;
    uint64_t done = 0;

    for (uint64_t len = start; len <= maxLen; ++len) {
        if (stopFlag.load()) return false;

        uint64_t sync = 0;
        for (uint64_t col = 0; col < len; ++col) {
            if ((col & 0xFFF) == 0 && stopFlag.load()) return false;
            const uint32_t first = map.ReadBits(origin + col, 1);
            bool uniform = true;
            for (int r = 1; r < rows; ++r) {
                const uint64_t pos = origin + static_cast<uint64_t>(r) * len + col;
                if (map.ReadBits(pos, 1) != first) { uniform = false; break; }
            }
            if (uniform) ++sync;
        }

        if (sync >= static_cast<uint64_t>(minCols)) {
            result.frameLenBits = len;
            result.syncColumns = sync;
            if (onProgress) onProgress(done + 1, candidates, true);
            return true;
        }

        ++done;
        if (onProgress && (done & 0xFF) == 0)
            onProgress(done, candidates, false);
    }
    if (onProgress) onProgress(candidates, candidates, true);
    return true;
}

bool SearchEngine::ApplyFilter(const FileMap& map, const FrameModel& fm,
                                const FrameFilter& filter,
                                std::vector<uint64_t>& frameIndices,
                                const ProgressFn& onProgress,
                                const std::atomic<bool>& stopFlag) {
    frameIndices.clear();
    if (!map.IsOpen())
        return false;

    const uint64_t totalBits = map.SizeBits();
    const uint64_t start = fm.startBit;
    uint64_t frameBits = fm.frameLen > 0 ? fm.frameLen : (totalBits > start ? totalBits - start : 0);
    if (frameBits == 0) return false;

    const uint64_t frames = (totalBits > start) ? (totalBits - start) / frameBits : 0;

    auto compare = [](uint64_t a, uint64_t b, CmpOp op) {
        switch (op) {
        case CmpOp::Eq: return a == b;
        case CmpOp::Gt: return a > b;
        case CmpOp::Lt: return a < b;
        case CmpOp::Ne: return a != b;
        }
        return false;
    };

    for (uint64_t i = 0; i < frames; ++i) {
        if (stopFlag.load()) return false;
        const uint64_t base = start + i * frameBits;

        bool ok = true;
        for (const FilterCondition& c : filter.conditions) {
            if (!ok) break;
            switch (c.kind) {
            case FilterKind::FrameLength:
                if (!compare(frameBits, static_cast<uint64_t>(std::max(0, c.frameBits)),
                             c.frameOp))
                    ok = false;
                break;
            case FilterKind::Value: {
                int bits = c.valueBits;
                if (bits != 1 && bits != 8 && bits != 16 && bits != 32)
                    bits = 8;
                const int pos = std::max(0, c.valuePos);
                const uint64_t bitPos = (bits == 1)
                    ? base + static_cast<uint64_t>(pos)
                    : base + static_cast<uint64_t>(pos) * 8;
                if (bitPos < base || bitPos + static_cast<uint64_t>(bits) > base + frameBits) {
                    ok = false;
                    break;
                }
                uint64_t v = 0;
                if (bits == 1) {
                    v = map.ReadBits(bitPos, 1) & 1u;
                } else {
                    // 原版数值按低字节在前输入：帧内首字节为最低字节
                    for (int b = 0; b < bits / 8; ++b)
                        v |= static_cast<uint64_t>(map.ReadByteAtBit(bitPos + static_cast<uint64_t>(b) * 8))
                             << (b * 8);
                }
                if (!compare(v, c.value, c.valueOp))
                    ok = false;
                break;
            }
            case FilterKind::Crc: {
                const uint64_t frameBytes = frameBits / 8;
                if (frameBytes < 3) {
                    ok = false;
                    break;
                }
                const int crcPos = static_cast<int>(frameBytes) - 2;
                std::vector<uint8_t> frameData(static_cast<size_t>(frameBytes));
                for (uint64_t b = 0; b < frameBytes; ++b)
                    frameData[static_cast<size_t>(b)] = map.ReadByteAtBit(base + b * 8);
                const uint16_t calc = Crc16(frameData.data(),
                                            static_cast<size_t>(crcPos), 0);
                const uint16_t stored = static_cast<uint16_t>(
                    (frameData[static_cast<size_t>(crcPos)] << 8) |
                    frameData[static_cast<size_t>(crcPos) + 1]);
                if ((calc == stored) != c.crcPass)
                    ok = false;
                break;
            }
            }
        }
        if (ok)
            frameIndices.push_back(i);

        if (onProgress && (i & 0xFF) == 0)
            onProgress(i, frames, false);
    }
    if (onProgress) onProgress(frames, frames, true);
    return true;
}

} // namespace bes
