// Renderer.h —— 绘制引擎（对应原程序 IDrawEngine 的 6 种实现）
#pragma once

#include <cstdint>
#include <vector>

#include "core/DataTypes.h"
#include "core/FileMap.h"
#include "core/FrameModel.h"
#include "core/Selection.h"

namespace bes {

// 一个「格」的显示信息。
// Hex 模式下 1 格 = 1 字节 = 2 个十六进制字符；
// HexChar 模式下 1 格 = 1 字节 = 2 个十六进制字符 + 1 个 ASCII 字符。
struct Cell {
    int  x = 0;            // 屏幕列
    int  y = 0;            // 屏幕行
    int  w = 0;            // 宽(px)
    int  h = 0;            // 高(px)
    uint64_t bitPos = 0;   // 对应数据的起始比特
    int  bitLen = 0;       // 该格占用的比特数
    bool isSet = false;    // bit 模式下该比特是否为 1
    uint8_t byteVal = 0;
    wchar_t ch = 0;        // 主显示字符（高 nibble / 字符 / 比特）
    wchar_t ch2 = 0;       // 次显示字符（低 nibble）
    wchar_t ch3 = 0;       // HexChar 模式的 ASCII 字符
    bool frameStart = false; // 该格是否为帧首格（纵轴标注用）
};

// 渲染参数（对应「参数设置」对话框）
struct RenderParams {
    ViewMode   view = ViewMode::Hex;
    ViewLayout layout = ViewLayout::Paged;  // 布局：分页/横排/竖排/分组横排/分组竖排
    bool       showTitle = true;            // 标题栏（原版「标题」按钮，默认选中）
    YAxisMode  yAxis = YAxisMode::YFN;      // 纵轴显示：帧号/帧长/比特位置
    bool       tabFollow = false;           // TAB跟随：滚动时按 TAB 宽度吸附
    bool       highBitFirst = true;   // 高位在前(0-7)；false = 低位在前(7-0)，每字节内比特倒序
    bool       invert = false;        // 取反
    int        xStep = 1;             // X方向浏览步进（格）
    int        yStep = 1;             // Y方向浏览步进（行）
    int        xTab = 8;              // X方向TAB宽度（格）
    int        yTab = 4;              // Y方向TAB宽度（行）
    bool       showXTab = false;
    bool       showYTab = false;
    bool       showRuler = true;
    int        zoom = 1;              // 1=100%, 2=200%...
};

// 颜色方案（原程序参数设置里的颜色）
struct ColorScheme {
    uint32_t body       = 0x00800080u;  // 0x00BBGGRR，原版默认紫色正文
    uint32_t background = 0x00FFFFFFu;
    uint32_t ruler      = 0x00800000u;  // 深蓝
    uint32_t tab        = 0x00C0C0C0u;  // TAB 分割线
    uint32_t marker     = 0x000000FFu;  // 红
    uint32_t bookmark   = 0x00800000u;
    uint32_t selection  = 0x00FF0000u;  // 蓝
    uint32_t coord      = 0x00808080u;
    uint32_t errFrame   = 0x008080FFu;
};

// 单个文档的渲染上下文。
// 坐标模型：行 = 帧（或横排模式下的数据流行），列 = 格（字节/比特）。
// 长帧可通过 hCellOffset 水平平移；firstRow 为垂直滚动行号。
class Renderer {
public:
    static int BitsPerCell(ViewMode m);
    static int CharsPerCell(ViewMode m);

    void SetData(const FileMap* map) { m_map = map; }
    void SetFrame(const FrameModel* fm) { m_frame = fm; }
    void SetSelection(const SelectionSet* sel) { m_sel = sel; }
    void SetParams(const RenderParams& p) { m_params = p; }
    const RenderParams& Params() const { return m_params; }
    void SetColors(const ColorScheme& c) { m_colors = c; }
    const ColorScheme& Colors() const { return m_colors; }

    // 不等长帧 / 过滤后的可见帧列表
    void SetVariableFrames(const VariableFrameSet* v) { m_vframes = v; }
    void SetVisibleFrames(const std::vector<uint64_t>* v, bool active) {
        m_visible = v; m_visibleActive = active;
    }
    void SetColsPerRow(int c) { m_colsPerRow = (c > 0) ? c : 1; }
    int  ColsPerRow() const { return m_colsPerRow; }

    // 帧长：显式设置 > 0 时使用；否则「自动」= 一屏宽（colsPerRow 格）
    uint64_t EffectiveFrameLen(int colsPerRow = 0) const;
    uint64_t CellsPerFrame(int colsPerRow = 0) const;

    struct Geometry {
        uint64_t cellsPerFrame = 1;
        uint64_t totalCells = 0;
        uint64_t rows = 0;
        uint64_t cols = 1;
        bool     variable = false;
    };
    Geometry Compute(int colsPerRow) const;

    // 计算一屏的格子布局。cells 按行优先顺序。
    // firstRow：屏幕第一行的绝对行号；hCellOffset：帧内水平格偏移。
    void Layout(int viewWidth, int viewHeight, int charWidth, int charHeight,
                uint64_t firstRow, uint64_t hCellOffset,
                std::vector<Cell>& cells,
                uint64_t& totalRows, uint64_t& totalCols);

    // 填充单格内容
    void FillCell(Cell& c) const;

    // 屏幕坐标 -> 比特位置（用于鼠标选取）
    bool HitTest(int x, int y, int viewWidth, int viewHeight,
                 int charWidth, int charHeight,
                 uint64_t firstRow, uint64_t hCellOffset,
                 uint64_t& bitPos);

    // 判断某比特是否被选中（连续/矩形/行/列）
    bool IsSelected(uint64_t bitPos) const;

    // 行对应的纵轴文字（YFN=帧号 / YFL=帧长 / YFB=比特位置）
    bool YAxisText(uint64_t rowIdx, wchar_t* buf, size_t bufLen) const;

    // 当前布局是否按帧分行
    bool IsFrameRows() const;

    // 行首比特 / 比特所在帧号 / 比特所在格列
    uint64_t RowStartBit(uint64_t row) const;          // 行 -> 起始比特（含过滤映射）
    uint64_t FrameStartBit(uint64_t frameIndex) const; // 帧号 -> 起始比特（不含行映射）
    uint64_t FrameIndexOfBit(uint64_t bitPos) const;
    uint64_t CellColumnOfBit(uint64_t bitPos) const;
    uint64_t FrameLenOfRow(uint64_t row) const;

private:
    // 显示行 -> 帧号（过滤生效时按可见帧列表映射）
    uint64_t FrameOfRow(uint64_t row) const;

    const FileMap*          m_map = nullptr;
    const FrameModel*       m_frame = nullptr;
    const SelectionSet*     m_sel = nullptr;
    const VariableFrameSet* m_vframes = nullptr;
    const std::vector<uint64_t>* m_visible = nullptr;
    bool                    m_visibleActive = false;
    RenderParams            m_params;
    ColorScheme             m_colors;
    int                     m_colsPerRow = 80;
};

} // namespace bes
