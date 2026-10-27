// Selection.h —— 选区模型（连续 / 块 / 行 / 列）与选区存盘读入
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace bes {

enum class SelectMode {
    None = 0,
    Continuous,  // 连续选取
    Rect,        // 块选取
    Row,         // 行选取
    Col          // 列选取
};

const wchar_t* SelectModeName(SelectMode m);

// 一个矩形选区：按「帧号 + 帧内格列」定位（闭区间）。
// 原程序块选取是二维的，列以显示格（字节/比特）为单位。
struct RectSel {
    uint64_t frame0 = 0, cell0 = 0;   // 左上
    uint64_t frame1 = 0, cell1 = 0;   // 右下（闭区间）
};

// 连续选区的一段（左端、右端，闭区间）
struct ContinuousSel { uint64_t startBit = 0, endBit = 0; };

// 选区集合 —— 兼容原程序「连续选取 / 块选取 / 行选取 / 列选取」与
// 「清除当前选取 / 清除全部选取」以及选区参数存盘/读入。
class SelectionSet {
public:
    void Clear() { m_continuous.clear(); m_rects.clear(); m_rows.clear(); m_cols.clear(); }
    void ClearAll() { Clear(); m_mode = SelectMode::None; }

    SelectMode Mode() const { return m_mode; }
    void SetMode(SelectMode m) { m_mode = m; }

    // 连续选区：原版是 TList，每次拖动提交一条 24 字节记录并 Sort（比较器 0x4033AC
    // 按记录首字段升序），所以同一模式下可以累积多段互不相连的区间。此处保持同样的
    // 「按起点排序、互不合并」语义。
    void AddContinuous(uint64_t a, uint64_t b) {
        ContinuousSel s;
        s.startBit = a < b ? a : b;   // 不用 std::min：避免 windows.h 的 min 宏
        s.endBit   = a < b ? b : a;
        auto it = std::upper_bound(m_continuous.begin(), m_continuous.end(), s.startBit,
            [](uint64_t v, const ContinuousSel& c) { return v < c.startBit; });
        m_continuous.insert(it, s);
        m_mode = SelectMode::Continuous;
    }
    // 单段写入（供读入旧文件与测试使用）：清空后写入
    void SetContinuous(uint64_t startBit, uint64_t endBit) {
        m_continuous.clear();
        AddContinuous(startBit, endBit);
        m_mode = SelectMode::Continuous;
    }
    void ClearContinuous() { m_continuous.clear(); }

    // 矩形选区（可多块）
    void AddRect(const RectSel& r) { m_rects.push_back(r); m_mode = SelectMode::Rect; }
    void ClearRects() { m_rects.clear(); }

    // 行选区：以帧为单位（原版按区间合并存放，这里展开为行号集合）
    void AddRow(uint64_t frameIndex) { m_rows.push_back(frameIndex); m_mode = SelectMode::Row; }
    // 单击反选：已选中则该行取消，否则加入。返回操作后该行是否被选中。
    bool ToggleRow(uint64_t frameIndex) {
        auto it = std::lower_bound(m_rows.begin(), m_rows.end(), frameIndex);
        if (it != m_rows.end() && *it == frameIndex) {
            m_rows.erase(it);
            return false;
        }
        m_rows.insert(it, frameIndex);
        m_mode = SelectMode::Row;
        return true;
    }
    void AddRowRange(uint64_t f0, uint64_t f1) {
        if (f1 < f0) std::swap(f0, f1);
        for (uint64_t f = f0; ; ++f) {
            m_rows.push_back(f);
            if (f == f1) break;
        }
        std::sort(m_rows.begin(), m_rows.end());
        m_rows.erase(std::unique(m_rows.begin(), m_rows.end()), m_rows.end());
        m_mode = SelectMode::Row;
    }
    void ClearRows() { m_rows.clear(); }

    // 列选区：以帧内显示格列为单位
    void AddCol(uint64_t col) { m_cols.push_back(col); m_mode = SelectMode::Col; }
    // 单击反选：已选中则该列取消，否则加入。返回操作后该列是否被选中。
    bool ToggleCol(uint64_t col) {
        auto it = std::lower_bound(m_cols.begin(), m_cols.end(), col);
        if (it != m_cols.end() && *it == col) {
            m_cols.erase(it);
            return false;
        }
        m_cols.insert(it, col);
        m_mode = SelectMode::Col;
        return true;
    }
    void AddColRange(uint64_t c0, uint64_t c1) {
        if (c1 < c0) std::swap(c0, c1);
        for (uint64_t c = c0; ; ++c) {
            m_cols.push_back(c);
            if (c == c1) break;
        }
        std::sort(m_cols.begin(), m_cols.end());
        m_cols.erase(std::unique(m_cols.begin(), m_cols.end()), m_cols.end());
        m_mode = SelectMode::Col;
    }
    void ClearCols() { m_cols.clear(); }

    // 清除某个模式的列表（原版 0x4404CC(obj, dl=0)：切换模式时只清上一模式）
    void ClearMode(SelectMode m) {
        switch (m) {
        case SelectMode::Continuous: ClearContinuous(); break;
        case SelectMode::Rect:       ClearRects(); break;
        case SelectMode::Row:        ClearRows(); break;
        case SelectMode::Col:        ClearCols(); break;
        default:                     break;   // 模式 -1：没有「上一模式的列表」可清
        }
    }

    bool Empty() const {
        return m_continuous.empty() && m_rects.empty() && m_rows.empty() && m_cols.empty();
    }

    // 基础统计（不换算帧/列）
    uint64_t SelectedBits() const;
    // 统计被选中的比特数：
    //   frameLenBits —— 每帧比特数
    //   bitsPerCell  —— 每格比特数（1/8）
    //   frameCount   —— 当前文档总帧数
    uint64_t SelectedBitsEx(uint64_t frameLenBits, int bitsPerCell, uint64_t frameCount) const;

    // 原版 .slp 二进制格式（反汇编 BesCom.dll 的 Load@0x440624 / Save@0x4407AC 确认）：
    //   3 字节 ASCII "slp"，随后 4 个段，段内为 Int32 记录数 + 记录数 × 24 字节（小端）。
    //   两段读写都是通用循环：对象 +4/+8/+0x0C/+0x10 四个 TList，按该顺序各写一段，
    //   每条记录是所存 24 字节记录的裸内存。四个列表由「选取」按钮的 Tag（= 选取模式）分派
    //   （SelectBtnClick@0x40AC88 取 [Sender+0xC] 调 vtbl+0x10；0x444924 按模式 0..3 落到
    //   +4/+8/+0x0C/+0x10），模式取自 DFM：0=连续选取、1=块选取、2=行选取、3=列选取，故
    //   段序 = 连续 / 块 / 行 / 列。记录统一为 {Int64 p1; Int32 c1; Int64 p2; Int32 c2}：
    //   连续原样拷贝四个待定参数；块把 (p1,c1)/(p2,c2) 归一化成左上/右下；行只写 Int64 区间
    //   （c1/c2 强制 0）；列只写 Int32 区间（p1/p2 强制 0）。
    std::vector<uint8_t> SaveSlp() const;
    bool LoadSlp(const std::vector<uint8_t>& data, std::wstring* err = nullptr);

    // 旧版文本格式（legacy_mfc 的选区存盘仍在使用）
    std::wstring Serialize() const;
    // 反序列化（选区参数读入）
    bool Deserialize(const std::wstring& text, std::wstring* err = nullptr);

    const std::vector<RectSel>& Rects() const { return m_rects; }
    const std::vector<uint64_t>& Rows() const { return m_rows; }
    const std::vector<uint64_t>& Cols() const { return m_cols; }
    const std::vector<ContinuousSel>& Continuouses() const { return m_continuous; }
    bool HasContinuous() const { return !m_continuous.empty(); }
    // 仅用于「把全部连续区间当作一个整体」的场景（如整字节复制）；
    // 渲染/输出请遍历 Continuouses()。
    uint64_t ContinuousStart() const { return m_continuous.empty() ? 0 : m_continuous.front().startBit; }
    uint64_t ContinuousEnd()   const { return m_continuous.empty() ? 0 : m_continuous.back().endBit; }

private:
    SelectMode         m_mode = SelectMode::None;
    std::vector<ContinuousSel> m_continuous;
    std::vector<RectSel>   m_rects;
    std::vector<uint64_t>  m_rows;
    std::vector<uint64_t>  m_cols;
};

} // namespace bes
