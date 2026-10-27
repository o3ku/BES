// Renderer.cpp
#include "core/Renderer.h"

#include <algorithm>
#include <cstdio>

namespace bes {

int Renderer::BitsPerCell(ViewMode m) {
    switch (m) {
    case ViewMode::BitBitmap:
    case ViewMode::BitChar:   return 1;
    case ViewMode::Hex:       return 8;
    case ViewMode::Char:      return 8;
    case ViewMode::Pixel:     return 8;
    case ViewMode::HexChar:   return 8;
    }
    return 8;
}

int Renderer::CharsPerCell(ViewMode m) {
    switch (m) {
    case ViewMode::Hex:      return 3;   // 高/低 nibble + 空格
    case ViewMode::HexChar:  return 4;   // 高/低 nibble + 空格 + ASCII
    default:                 return 1;
    }
}

uint64_t Renderer::EffectiveFrameLen(int colsPerRow) const {
    if (m_frame && m_frame->frameLen > 0)
        return m_frame->frameLen;
    int c = (colsPerRow > 0) ? colsPerRow : m_colsPerRow;
    if (c < 1) c = 1;
    return static_cast<uint64_t>(c) * static_cast<uint64_t>(BitsPerCell(m_params.view));
}

uint64_t Renderer::CellsPerFrame(int colsPerRow) const {
    const int bpc = BitsPerCell(m_params.view);
    const uint64_t fl = EffectiveFrameLen(colsPerRow);
    return std::max<uint64_t>(1, (fl + static_cast<uint64_t>(bpc) - 1) / static_cast<uint64_t>(bpc));
}

Renderer::Geometry Renderer::Compute(int colsPerRow) const {
    Geometry g;
    if (!m_map) return g;

    const int bpc = BitsPerCell(m_params.view);
    const uint64_t totalBits = m_map->SizeBits();
    g.totalCells = (totalBits + static_cast<uint64_t>(bpc) - 1) / static_cast<uint64_t>(bpc);
    g.variable = (m_frame && m_frame->bVariable && m_vframes && m_vframes->Size() > 0);

    if (g.variable) {
        uint64_t maxCells = 1;
        for (const VariableFrame& f : m_vframes->Frames()) {
            const uint64_t n = std::max<uint64_t>(1, (f.length + static_cast<uint64_t>(bpc) - 1) /
                                                     static_cast<uint64_t>(bpc));
            maxCells = std::max(maxCells, n);
        }
        g.cellsPerFrame = maxCells;
        g.rows = (m_visibleActive && m_visible)
                     ? m_visible->size()
                     : m_vframes->Size();
        g.cols = maxCells;
        return g;
    }

    g.cellsPerFrame = CellsPerFrame(colsPerRow);
    g.cols = g.cellsPerFrame;

    if (m_visibleActive && m_visible) {
        g.rows = m_visible->size();
        return g;
    }

    const uint64_t fl = EffectiveFrameLen(colsPerRow);
    const uint64_t start = m_frame ? m_frame->startBit : 0;
    if (totalBits <= start) {
        g.rows = 0;
    } else {
        g.rows = (totalBits - start + fl - 1) / fl;
    }
    return g;
}

void Renderer::Layout(int viewWidth, int viewHeight, int charWidth, int charHeight,
                      uint64_t firstRow, uint64_t hCellOffset,
                      std::vector<Cell>& cells,
                      uint64_t& totalRows, uint64_t& totalCols) {
    cells.clear();
    totalRows = 0;
    totalCols = 1;
    if (!m_map || charWidth <= 0 || charHeight <= 0)
        return;

    const int zoom = m_params.zoom > 0 ? m_params.zoom : 1;
    const int cw = charWidth * zoom;
    const int ch = charHeight * zoom;
    const int bpc = BitsPerCell(m_params.view);
    const int span = CharsPerCell(m_params.view);

    int colsPerRow = viewWidth / std::max(1, cw * span);
    if (colsPerRow < 1) colsPerRow = 1;
    m_colsPerRow = colsPerRow;

    Geometry g = Compute(colsPerRow);
    totalRows = g.rows;
    totalCols = g.cols;

    const uint64_t totalBits = m_map->SizeBits();
    int rowsPerScreen = viewHeight / std::max(1, ch);
    if (rowsPerScreen < 1) rowsPerScreen = 1;

    const int xStep = m_params.xStep > 0 ? m_params.xStep : 1;
    const int yStep = m_params.yStep > 0 ? m_params.yStep : 1;

    // ---- 横排：连续数据流，按屏宽换行 ----
    if (m_params.layout == ViewLayout::Horizontal ||
        m_params.layout == ViewLayout::GroupH) {
        const uint64_t totalCells = g.totalCells;
        totalRows = (totalCells + static_cast<uint64_t>(colsPerRow) - 1) /
                    static_cast<uint64_t>(colsPerRow);
        totalCols = static_cast<uint64_t>(colsPerRow);

        for (int r = 0; r < rowsPerScreen; ++r) {
            const uint64_t rowAbs = firstRow + static_cast<uint64_t>(r) * static_cast<uint64_t>(yStep);
            if (rowAbs >= totalRows) break;
            const uint64_t rowStart = rowAbs * static_cast<uint64_t>(colsPerRow) + hCellOffset;
            for (int c = 0; c < colsPerRow; ++c) {
                const uint64_t cellIdx = rowStart + static_cast<uint64_t>(c) * static_cast<uint64_t>(xStep);
                if (cellIdx >= totalCells) break;
                Cell cell;
                cell.x = c * cw * span;
                cell.y = r * ch;
                cell.w = cw * span;
                cell.h = ch;
                cell.bitPos = cellIdx * static_cast<uint64_t>(bpc);
                cell.bitLen = bpc;
                cells.push_back(cell);
            }
        }
        return;
    }

    // ---- 按帧分行（Paged / Vertical / GroupV / Title）----
    for (int r = 0; r < rowsPerScreen; ++r) {
        const uint64_t rowAbs = firstRow + static_cast<uint64_t>(r) * static_cast<uint64_t>(yStep);
        if (rowAbs >= g.rows) break;

        uint64_t frameStart = 0, frameLen = 0, nCells = 0, frameIdx = rowAbs;

        if (g.variable) {
            const uint64_t fi = FrameOfRow(rowAbs);
            if (fi >= m_vframes->Size()) break;
            const VariableFrame& vf = m_vframes->Frames()[static_cast<size_t>(fi)];
            frameStart = vf.startBit;
            frameLen = vf.length;
            nCells = std::max<uint64_t>(1, (frameLen + static_cast<uint64_t>(bpc) - 1) /
                                           static_cast<uint64_t>(bpc));
        } else {
            if (m_visibleActive && m_visible) {
                if (rowAbs >= m_visible->size()) break;
                frameIdx = (*m_visible)[static_cast<size_t>(rowAbs)];
            }
            frameLen = EffectiveFrameLen(colsPerRow);
            frameStart = (m_frame ? m_frame->startBit : 0) + frameIdx * frameLen;
            nCells = g.cellsPerFrame;
        }

        if (frameStart >= totalBits) break;

        for (int c = 0; c < colsPerRow; ++c) {
            const uint64_t cellCol = hCellOffset + static_cast<uint64_t>(c) * static_cast<uint64_t>(xStep);
            if (cellCol >= nCells) break;
            const uint64_t bitPos = frameStart + cellCol * static_cast<uint64_t>(bpc);
            if (bitPos >= totalBits || bitPos >= frameStart + frameLen) break;

            Cell cell;
            cell.x = c * cw * span;
            cell.y = r * ch;
            cell.w = cw * span;
            cell.h = ch;
            cell.bitPos = bitPos;
            cell.bitLen = bpc;
            cell.frameStart = (cellCol == 0);
            cells.push_back(cell);
        }
    }
}

static uint8_t ReverseByteBits(uint8_t v) {
    return static_cast<uint8_t>(((v & 0x01) << 7) | ((v & 0x02) << 5) |
                                ((v & 0x04) << 3) | ((v & 0x08) << 1) |
                                ((v & 0x10) >> 1) | ((v & 0x20) >> 3) |
                                ((v & 0x40) >> 5) | ((v & 0x80) >> 7));
}

void Renderer::FillCell(Cell& c) const {
    if (!m_map) return;
    const ViewMode vm = m_params.view;
    uint64_t bp = c.bitPos;

    if (vm == ViewMode::BitBitmap || vm == ViewMode::BitChar) {
        // 原版「倒序」= 低位在前(7-0)：每字节内比特反向，不跨字节
        if (!m_params.highBitFirst)
            bp = (bp & ~7ull) | (7ull - (bp & 7ull));
        const uint32_t v = m_map->ReadBits(bp, 1);
        c.isSet = (v != 0);
        c.byteVal = static_cast<uint8_t>(c.isSet ? 1 : 0);
        // 原版「等长帧比特字符」用 '.' 表示 0，'1' 表示 1
        c.ch = (vm == ViewMode::BitChar) ? (c.isSet ? L'1' : L'.')
                                         : (c.isSet ? L'1' : L'0');
        return;
    }

    uint8_t raw = m_map->ReadByteAtBit(bp);
    if (!m_params.highBitFirst)
        raw = ReverseByteBits(raw);
    const uint8_t val = m_params.invert ? static_cast<uint8_t>(~raw) : raw;
    c.byteVal = val;

    static const wchar_t* kHex = L"0123456789ABCDEF";
    const wchar_t hi = kHex[(val >> 4) & 0xF];
    const wchar_t lo = kHex[val & 0xF];
    const wchar_t printable = (val >= 0x20 && val < 0x7F) ? static_cast<wchar_t>(val) : L'.';

    switch (vm) {
    case ViewMode::Hex:
        c.ch  = hi;
        c.ch2 = lo;
        return;
    case ViewMode::HexChar:
        c.ch  = hi;
        c.ch2 = lo;
        c.ch3 = printable;
        return;
    case ViewMode::Char:
        c.ch = printable;
        return;
    case ViewMode::Pixel:
        c.ch = 0;
        return;
    default:
        return;
    }
}

bool Renderer::HitTest(int x, int y, int viewWidth, int viewHeight,
                       int charWidth, int charHeight,
                       uint64_t firstRow, uint64_t hCellOffset,
                       uint64_t& bitPos) {
    if (!m_map || charWidth <= 0 || charHeight <= 0)
        return false;
    if (x < 0 || y < 0 || x >= viewWidth || y >= viewHeight)
        return false;

    const int zoom = m_params.zoom > 0 ? m_params.zoom : 1;
    const int cw = charWidth * zoom;
    const int ch = charHeight * zoom;
    const int bpc = BitsPerCell(m_params.view);
    const int span = CharsPerCell(m_params.view);

    int colsPerRow = viewWidth / std::max(1, cw * span);
    if (colsPerRow < 1) colsPerRow = 1;
    m_colsPerRow = colsPerRow;
    Geometry g = Compute(colsPerRow);

    const uint64_t totalBits = m_map->SizeBits();
    const uint64_t totalCells = g.totalCells;

    if (m_params.layout == ViewLayout::Horizontal ||
        m_params.layout == ViewLayout::GroupH) {
        const int yStep = m_params.yStep > 0 ? m_params.yStep : 1;
        const uint64_t rowAbs = firstRow +
            static_cast<uint64_t>(y / std::max(1, ch)) * static_cast<uint64_t>(yStep);
        const uint64_t rows = (totalCells + static_cast<uint64_t>(colsPerRow) - 1) /
                              static_cast<uint64_t>(colsPerRow);
        if (rowAbs >= rows) return false;
        const uint64_t rowStart = rowAbs * static_cast<uint64_t>(colsPerRow) + hCellOffset;
        const int xStep = m_params.xStep > 0 ? m_params.xStep : 1;
        const uint64_t cellIdx = rowStart +
            static_cast<uint64_t>(x / std::max(1, cw * span)) * static_cast<uint64_t>(xStep);
        if (cellIdx >= totalCells) return false;
        bitPos = cellIdx * static_cast<uint64_t>(bpc);
        return true;
    }

    const int yStep = m_params.yStep > 0 ? m_params.yStep : 1;
    const uint64_t rowAbs = firstRow +
        static_cast<uint64_t>(y / std::max(1, ch)) * static_cast<uint64_t>(yStep);
    if (rowAbs >= g.rows) return false;

    uint64_t frameStart = 0, frameLen = 0, nCells = 0, frameIdx = rowAbs;
    if (g.variable) {
        frameIdx = FrameOfRow(rowAbs);
        if (frameIdx >= m_vframes->Size()) return false;
        const VariableFrame& vf = m_vframes->Frames()[static_cast<size_t>(frameIdx)];
        frameStart = vf.startBit;
        frameLen = vf.length;
        nCells = std::max<uint64_t>(1, (frameLen + static_cast<uint64_t>(bpc) - 1) /
                                       static_cast<uint64_t>(bpc));
    } else {
        if (m_visibleActive && m_visible) {
            if (rowAbs >= m_visible->size()) return false;
            frameIdx = (*m_visible)[static_cast<size_t>(rowAbs)];
        }
        frameLen = EffectiveFrameLen(colsPerRow);
        frameStart = (m_frame ? m_frame->startBit : 0) + frameIdx * frameLen;
        nCells = g.cellsPerFrame;
    }
    if (frameStart >= totalBits) return false;

    const int xStep = m_params.xStep > 0 ? m_params.xStep : 1;
    const uint64_t cellCol = hCellOffset +
        static_cast<uint64_t>(x / std::max(1, cw * span)) * static_cast<uint64_t>(xStep);
    if (cellCol >= nCells) return false;
    const uint64_t bit = frameStart + cellCol * static_cast<uint64_t>(bpc);
    if (bit >= totalBits || bit >= frameStart + frameLen) return false;
    bitPos = bit;
    return true;
}

bool Renderer::IsSelected(uint64_t bitPos) const {
    if (!m_sel || !m_map) return false;
    if (bitPos >= m_map->SizeBits()) return false;

    for (const ContinuousSel& s : m_sel->Continuouses())
        if (bitPos >= s.startBit && bitPos <= s.endBit)
            return true;

    const int bpc = BitsPerCell(m_params.view);
    const uint64_t frameIdx = FrameIndexOfBit(bitPos);
    const uint64_t fs = FrameStartBit(frameIdx);
    const uint64_t cellCol = (bitPos >= fs) ? (bitPos - fs) / static_cast<uint64_t>(bpc) : 0;

    for (const RectSel& r : m_sel->Rects()) {
        if (frameIdx >= r.frame0 && frameIdx <= r.frame1 &&
            cellCol >= r.cell0 && cellCol <= r.cell1)
            return true;
    }
    for (uint64_t row : m_sel->Rows())
        if (row == frameIdx) return true;
    for (uint64_t col : m_sel->Cols())
        if (col == cellCol) return true;
    return false;
}

bool Renderer::YAxisText(uint64_t rowIdx, wchar_t* buf, size_t bufLen) const {
    if (!m_frame || !buf || bufLen == 0) return false;

    // 横排：行是连续数据流，行号/行宽/行首比特按 colsPerRow 计算
    if (!IsFrameRows()) {
        if (!m_map) return false;
        const uint64_t totalBits = m_map->SizeBits();
        const uint64_t base = std::min<uint64_t>(m_frame->startBit, totalBits);
        const uint64_t rowBits = static_cast<uint64_t>(m_colsPerRow) *
                                 static_cast<uint64_t>(BitsPerCell(m_params.view));
        if (rowBits == 0 || base >= totalBits) return false;
        const uint64_t rows = (totalBits - base + rowBits - 1) / rowBits;
        if (rowIdx >= rows) return false;
        const uint64_t rowStart = base + rowIdx * rowBits;

        uint64_t v = 0;
        switch (m_params.yAxis) {
        case YAxisMode::YFN: v = rowIdx; break;
        case YAxisMode::YFL:
            v = std::min<uint64_t>(rowBits, totalBits - rowStart);
            break;
        case YAxisMode::YFB: v = rowStart; break;
        }
        swprintf(buf, bufLen, L"%llu", static_cast<unsigned long long>(v));
        return true;
    }

    uint64_t frameIdx = rowIdx, start = 0, len = 0;
    if (m_frame->bVariable && m_vframes && m_vframes->Size() > 0) {
        frameIdx = FrameOfRow(rowIdx);
        if (frameIdx >= m_vframes->Size()) return false;
        const VariableFrame& vf = m_vframes->Frames()[static_cast<size_t>(frameIdx)];
        start = vf.startBit;
        len = vf.length;
    } else {
        if (m_visibleActive && m_visible) {
            if (rowIdx >= m_visible->size()) return false;
            frameIdx = (*m_visible)[static_cast<size_t>(rowIdx)];
        }
        len = EffectiveFrameLen(m_colsPerRow);
        start = m_frame->startBit + frameIdx * len;
    }

    uint64_t v = 0;
    switch (m_params.yAxis) {
    case YAxisMode::YFN: v = frameIdx; break;
    case YAxisMode::YFL: v = len;     break;
    case YAxisMode::YFB: v = start;   break;
    }
    swprintf(buf, bufLen, L"%llu", static_cast<unsigned long long>(v));
    return true;
}

bool Renderer::IsFrameRows() const {
    return m_params.layout != ViewLayout::Horizontal &&
           m_params.layout != ViewLayout::GroupH;
}

uint64_t Renderer::FrameOfRow(uint64_t row) const {
    if (m_visibleActive && m_visible) {
        if (row >= m_visible->size())
            return static_cast<uint64_t>(-1);
        return (*m_visible)[static_cast<size_t>(row)];
    }
    return row;
}

uint64_t Renderer::RowStartBit(uint64_t row) const {
    if (!m_frame) return 0;
    if (m_frame->bVariable && m_vframes && m_vframes->Size() > 0) {
        const uint64_t fi = FrameOfRow(row);
        if (fi >= m_vframes->Size()) return m_frame->startBit;
        return m_vframes->Frames()[static_cast<size_t>(fi)].startBit;
    }
    uint64_t frameIdx = row;
    if (m_visibleActive && m_visible) {
        if (row >= m_visible->size()) return m_frame->startBit;
        frameIdx = (*m_visible)[static_cast<size_t>(row)];
    }
    return FrameStartBit(frameIdx);
}

uint64_t Renderer::FrameStartBit(uint64_t frameIndex) const {
    if (!m_frame) return 0;
    if (m_frame->bVariable && m_vframes && m_vframes->Size() > 0) {
        if (frameIndex >= m_vframes->Size()) return m_frame->startBit;
        return m_vframes->Frames()[static_cast<size_t>(frameIndex)].startBit;
    }
    return m_frame->startBit + frameIndex * EffectiveFrameLen(m_colsPerRow);
}

uint64_t Renderer::FrameLenOfRow(uint64_t row) const {
    if (!m_frame) return 0;
    if (m_frame->bVariable && m_vframes && m_vframes->Size() > 0) {
        const uint64_t fi = FrameOfRow(row);
        if (fi >= m_vframes->Size()) return EffectiveFrameLen(m_colsPerRow);
        return m_vframes->Frames()[static_cast<size_t>(fi)].length;
    }
    return EffectiveFrameLen(m_colsPerRow);
}

uint64_t Renderer::FrameIndexOfBit(uint64_t bitPos) const {
    if (!m_frame) return 0;
    if (m_frame->bVariable && m_vframes && m_vframes->Size() > 0) {
        const int64_t idx = m_vframes->IndexOf(bitPos);
        return idx < 0 ? 0 : static_cast<uint64_t>(idx);
    }
    const uint64_t fl = EffectiveFrameLen(m_colsPerRow);
    if (fl == 0 || bitPos <= m_frame->startBit) return 0;
    return (bitPos - m_frame->startBit) / fl;
}

uint64_t Renderer::CellColumnOfBit(uint64_t bitPos) const {
    const uint64_t idx = FrameIndexOfBit(bitPos);
    const uint64_t fs = FrameStartBit(idx);
    if (bitPos < fs) return 0;
    const int bpc = BitsPerCell(m_params.view);
    return (bitPos - fs) / static_cast<uint64_t>(bpc);
}

} // namespace bes
