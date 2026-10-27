// OutputWriter.cpp
#include "core/OutputWriter.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace bes {

bool OutputWriter::Write(const FileMap& map, const FrameModel& fm, const SelectionSet& sel,
                          const OutputOptions& opt, uint64_t& bytesWritten, std::wstring* err) {
    bytesWritten = 0;
    if (opt.target == OutputTarget::None)
        return true;
    if (!map.IsOpen()) {
        if (err) *err = L"数据长度为0！";
        return false;
    }

    std::ofstream file;
    IOutputPort* port = nullptr;

    if (opt.target == OutputTarget::File) {
        if (opt.filePath.empty()) {
            if (err) *err = L": 文件名不能为空！";
            return false;
        }
        file.open(opt.filePath, std::ios::binary | std::ios::trunc);
        if (!file) {
            if (err) *err = L"文件不可写！";
            return false;
        }
    } else {
        if (opt.portIndex < 1 || opt.portIndex > 32 || !m_ports[opt.portIndex]) {
            if (err) *err = L"没有可用的输出端口！";
            return false;
        }
        port = m_ports[opt.portIndex];
    }

    std::vector<uint8_t> buf;
    const uint64_t totalBits = map.SizeBits();
    const int bpc = opt.bitsPerCell > 0 ? opt.bitsPerCell : 8;

    auto emit = [&](uint64_t fromBit, uint64_t bitCount) -> bool {
        if (bitCount == 0) return true;
        if (fromBit >= totalBits) return true;
        if (fromBit + bitCount > totalBits)
            bitCount = totalBits - fromBit;
        const uint64_t startByte = fromBit >> 3;
        const uint64_t endByte = (fromBit + bitCount + 7) >> 3;
        if (startByte >= map.SizeBytes()) return true;
        uint64_t n = endByte - startByte;
        if (n == 0) return true;
        buf.resize(static_cast<size_t>(n));
        std::memcpy(buf.data(), map.Base() + startByte, static_cast<size_t>(n));
        bytesWritten += n;
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(n));
            if (!file) { if (err) *err = L"保存文件失败！"; return false; }
        } else if (port) {
            if (!port->Write(buf.data(), buf.size())) {
                if (err) *err = L"端口输出失败！";
                return false;
            }
        }
        return true;
    };

    auto frameStartOf = [&](uint64_t frameIdx) -> uint64_t {
        if (fm.frameLen == 0) return fm.startBit;
        return fm.startBit + frameIdx * fm.frameLen;
    };
    auto frameLenOf = [&](uint64_t) -> uint64_t {
        if (fm.frameLen == 0) return totalBits - fm.startBit;
        return fm.frameLen;
    };

    bool ok = true;

    if (opt.wholeData) {
        ok = emit(fm.startBit, totalBits > fm.startBit ? totalBits - fm.startBit : 0);
    } else {
        if (ok && opt.continuousSel) {
            for (const ContinuousSel& s : sel.Continuouses()) {
                ok = emit(s.startBit, s.endBit - s.startBit + 1);
                if (!ok) break;
            }
        }
        if (ok && opt.rectSel) {
            for (const RectSel& r : sel.Rects()) {
                if (r.cell1 < r.cell0 || r.frame1 < r.frame0) continue;
                const uint64_t bitsPerRow = (r.cell1 - r.cell0 + 1) * static_cast<uint64_t>(bpc);
                for (uint64_t f = r.frame0; f <= r.frame1 && ok; ++f) {
                    const uint64_t bit = frameStartOf(f) + r.cell0 * static_cast<uint64_t>(bpc);
                    ok = emit(bit, bitsPerRow);
                }
            }
        }
        if (ok && opt.rowSel) {
            for (uint64_t row : sel.Rows()) {
                const uint64_t bit = frameStartOf(row);
                ok = emit(bit, frameLenOf(row));
                if (!ok) break;
            }
        }
        if (ok && opt.colSel) {
            const uint64_t frameLen = frameLenOf(0);
            const uint64_t frames = frameLen ? (totalBits - fm.startBit) / frameLen : 0;
            for (uint64_t col : sel.Cols()) {
                for (uint64_t f = 0; f < frames && ok; ++f) {
                    const uint64_t bit = frameStartOf(f) + col * static_cast<uint64_t>(bpc);
                    ok = emit(bit, static_cast<uint64_t>(bpc));
                }
                if (!ok) break;
            }
        }
    }

    if (file.is_open()) file.close();
    if (ok && err) err->clear();
    return ok;
}

} // namespace bes
