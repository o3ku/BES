// test_core.cpp —— 核心引擎单元测试（布局/十六进制/选区/搜索/输出/不等长帧）
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

#include "core/FileMap.h"
#include "core/Renderer.h"
#include "core/FrameModel.h"
#include "core/FlowEngine.h"
#include "core/Selection.h"
#include "core/Bookmark.h"
#include "core/Editor.h"
#include "core/OutputWriter.h"
#include "core/SearchEngine.h"

using namespace bes;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++g_fail; } \
                              else printf("ok  : %s\n", msg); } while(0)

static bool MakeFile(const std::wstring& path, const std::vector<uint8_t>& data) {
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    ::WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &wr, nullptr);
    ::CloseHandle(h);
    return true;
}

static uint64_t FindHit(const std::vector<SearchHit>& hits, uint64_t bit) {
    for (const SearchHit& h : hits)
        if (h.bitPos == bit) return 1;
    return 0;
}

int main()
{
    wchar_t tmpDir[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, tmpDir);
    std::wstring path = std::wstring(tmpDir) + L"bes_test.dat";

    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<uint8_t>((i * 7 + (i >> 3)) & 0xFF);
    data[0] = 0x01; data[1] = 0x23; data[2] = 0x45; data[3] = 0x67;
    data[4] = 0x89; data[5] = 0xAB; data[6] = 0xCD; data[7] = 0xEF;
    // 40 比特独特码（测试 >32 比特不再截断）
    data[25] = 0x1A; data[26] = 0xCF; data[27] = 0xFC; data[28] = 0x1D; data[29] = 0xFF;
    data[35] = 0x1A; data[36] = 0xCF; data[37] = 0xFC; data[38] = 0x1D; data[39] = 0x00;
    // 四相模糊：0xF0 是 0x0F 的按位取反
    data[50] = 0xF0;
    // 帧长模板：第 40 字节处重复前 4 字节
    data[40] = data[0]; data[41] = data[1]; data[42] = data[2]; data[43] = data[3];

    if (!MakeFile(path, data)) { printf("cannot create test file\n"); return 2; }

    FileMap map;
    std::wstring err;
    CHECK(map.Open(path, &err), "FileMap::Open");
    CHECK(map.SizeBytes() == 1024, "SizeBytes == 1024");
    CHECK(map.SizeBits() == 8192, "SizeBits == 8192");
    CHECK(map.ReadByteAtBit(0) == 0x01, "ReadByteAtBit(0) == 0x01");
    CHECK(map.ReadByteAtBit(8) == 0x23, "ReadByteAtBit(8) == 0x23");
    CHECK(map.ReadBits(0, 8) == 0x01, "ReadBits(0,8) == 0x01");

    FrameModel fm;
    fm.frameLen = 64;      // 每帧 8 字节
    fm.startBit = 0;
    fm.step = 1;

    RenderParams p;
    p.view = ViewMode::Hex;

    // ---- 布局：8 字节/帧 → 每帧 8 格 ----
    {
        Renderer r;
        r.SetData(&map); r.SetFrame(&fm); r.SetParams(p);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        CHECK(cells.size() == 160, "Layout: 20 rows x 8 cols = 160 cells");
        CHECK(cols == 8, "Layout: totalCols == 8 (cells per frame)");
        CHECK(rows == 128, "Layout: totalRows == 128 (frames)");
        CHECK(cells[0].bitPos == 0 && cells[1].bitPos == 8,
              "Layout: cell bit positions step by 8");
        CHECK(cells[0].w == 30, "Hex cell width = 3 chars (2 nibbles + space)");
    }

    // ---- 自动帧长：frameLen=0 → 一屏宽为一帧，可上下滚动 ----
    {
        FrameModel autoFm = fm; autoFm.frameLen = 0;
        Renderer r;
        r.SetData(&map); r.SetFrame(&autoFm); r.SetParams(p);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        // 800/(10*3)=26 格/行；1024 字节 → 40 行
        CHECK(cols == 26, "auto frame: cols per row = 26");
        CHECK(rows == 40, "auto frame: 1024/26 -> 40 rows");
        CHECK(cells.size() == 520, "auto frame: 20*26 cells");
        CHECK(cells[0].bitPos == 0 && cells[25].bitPos == 25 * 8,
              "auto frame: last cell of row");
    }

    // ---- 缩放：zoom=2 → 字符尺寸翻倍, 每行格数减半 ----
    {
        RenderParams pz = p; pz.zoom = 2;
        Renderer r;
        r.SetData(&map); r.SetFrame(&fm); r.SetParams(pz);
        std::vector<Cell> c1; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, c1, rows, cols);
        CHECK(c1[0].w == 60, "zoom=2: Hex cell width 60");
        CHECK(c1[0].h == 40, "zoom=2: cell height 40");
        CHECK(cols == 8, "zoom=2: frame still 8 cells");
    }

    // ---- 水平偏移：长帧可平移 ----
    {
        FrameModel wide = fm; wide.frameLen = 128;   // 16 字节/帧
        Renderer r;
        r.SetData(&map); r.SetFrame(&wide); r.SetParams(p);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 5, cells, rows, cols);
        CHECK(!cells.empty() && cells[0].bitPos == 5 * 8,
              "hCellOffset=5 -> first bit = 40");
        CHECK(cells[0].frameStart == false, "offset cell is not frame start");
        uint64_t bit = 0;
        CHECK(r.HitTest(0, 0, 800, 400, 10, 20, 0, 5, bit) && bit == 40,
              "HitTest honours hCellOffset");
    }

    // ---- X方向步进与 HitTest 一致 ----
    {
        RenderParams px = p; px.xStep = 2;
        Renderer r;
        r.SetData(&map); r.SetFrame(&fm); r.SetParams(px);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        CHECK(!cells.empty() && cells[1].bitPos == 16, "xStep=2: cells skip every 2 bytes");
        uint64_t bit = 0;
        CHECK(r.HitTest(30, 0, 800, 400, 10, 20, 0, 0, bit) && bit == 16,
              "xStep=2: HitTest maps 2nd column to byte 2");
    }

    // ---- Y方向步进与 HitTest 一致 ----
    {
        RenderParams py = p; py.yStep = 2;
        Renderer r;
        r.SetData(&map); r.SetFrame(&fm); r.SetParams(py);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        CHECK(cells.size() >= 9 && cells[8].bitPos == 128,
              "yStep=2: 2nd displayed row = frame 2");
        uint64_t bit = 0;
        CHECK(r.HitTest(0, 20, 800, 400, 10, 20, 0, 0, bit) && bit == 128,
              "yStep=2: HitTest 2nd row = frame 2");
    }

    // ---- 倒序 / 取反（倒序 = 高位在前选项关闭，即每字节内 7-0） ----
    {
        RenderParams pr = p; pr.highBitFirst = false;
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetParams(pr);
        Cell c; c.bitPos = 0; c.bitLen = 8;
        r.FillCell(c);
        CHECK(c.byteVal == 0x80, "low bit first: 0x01 -> 0x80");

        RenderParams pi = p; pi.invert = true;
        Renderer ri; ri.SetData(&map); ri.SetFrame(&fm); ri.SetParams(pi);
        Cell ci; ci.bitPos = 0; ci.bitLen = 8;
        ri.FillCell(ci);
        CHECK(ci.byteVal == 0xFE, "invert: 0x01 -> 0xFE");
    }

    // ---- 十六进制：一个字节两位 ----
    {
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetParams(p);
        Cell c; c.bitPos = 0; c.bitLen = 8;
        r.FillCell(c);
        CHECK(c.ch == L'0' && c.ch2 == L'1', "Hex 0x01 -> '0','1'");
        RenderParams pl = p; pl.highBitFirst = false;
        Renderer rl; rl.SetData(&map); rl.SetFrame(&fm); rl.SetParams(pl);
        Cell c2; c2.bitPos = 0; c2.bitLen = 8;
        rl.FillCell(c2);
        CHECK(c2.ch == L'8' && c2.ch2 == L'0',
              "low bit first: 0x01 -> '8','0' (nibble order fixed)");
    }

    // ---- 比特字符：0 -> '.'，1 -> '1'（与原版一致） ----
    {
        RenderParams pb = p; pb.view = ViewMode::BitChar;
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetParams(pb);
        Cell c; c.bitPos = 0; c.bitLen = 1;   // 0x01 的最高位 = 0
        r.FillCell(c);
        CHECK(c.ch == L'.', "BitChar 0 renders '.'");
        Cell c2; c2.bitPos = 7; c2.bitLen = 1; // 0x01 的最低位 = 1
        r.FillCell(c2);
        CHECK(c2.ch == L'1', "BitChar 1 renders '1'");

        // 倒序只反转每字节内部的比特，不跨字节
        RenderParams pb2 = pb; pb2.highBitFirst = false;
        Renderer r2; r2.SetData(&map); r2.SetFrame(&fm); r2.SetParams(pb2);
        Cell c3; c3.bitPos = 0; c3.bitLen = 1;   // 显示 0x01 的最低位 = 1
        r2.FillCell(c3);
        CHECK(c3.ch == L'1', "low bit first: first bit cell is LSB");
        Cell c4; c4.bitPos = 7; c4.bitLen = 1;   // 显示 0x01 的最高位 = 0
        r2.FillCell(c4);
        CHECK(c4.ch == L'.', "low bit first: 8th bit cell is MSB");
        Cell c5; c5.bitPos = 8; c5.bitLen = 1;   // 下一字节仍取自身最低位 = 1
        r2.FillCell(c5);
        CHECK(c5.ch == L'1', "low bit first does not cross byte boundary");
    }

    // ---- HexChar：高/低 nibble + ASCII ----
    {
        RenderParams ph = p; ph.view = ViewMode::HexChar;
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetParams(ph);
        Cell c; c.bitPos = 0; c.bitLen = 8;
        r.FillCell(c);
        CHECK(c.ch == L'0' && c.ch2 == L'1', "HexChar 0x01 -> 01");
        CHECK(c.ch3 == L'.', "HexChar non-printable -> '.'");
        Cell c2; c2.bitPos = 8; c2.bitLen = 8;   // 0x23 '#'
        r.FillCell(c2);
        CHECK(c2.ch == L'2' && c2.ch2 == L'3' && c2.ch3 == L'#',
              "HexChar 0x23 -> 23 #");
        CHECK(Renderer::CharsPerCell(ViewMode::HexChar) == 4,
              "HexChar spans 4 chars");
    }

    // ---- 选区：连续 / 行 / 列 ----
    {
        SelectionSet sel;
        sel.SetMode(SelectMode::Continuous);
        sel.SetContinuous(0, 15);
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetSelection(&sel); r.SetParams(p);
        CHECK(r.IsSelected(0),  "IsSelected: continuous start");
        CHECK(r.IsSelected(15), "IsSelected: continuous end");
        CHECK(!r.IsSelected(16), "IsSelected: outside continuous");

        SelectionSet selRow;
        selRow.SetMode(SelectMode::Row);
        selRow.AddRow(1);
        Renderer r2; r2.SetData(&map); r2.SetFrame(&fm); r2.SetSelection(&selRow); r2.SetParams(p);
        CHECK(r2.IsSelected(64),  "row 1 covers frame 1");
        CHECK(!r2.IsSelected(8),  "row 1 excludes frame 0");

        SelectionSet selCol;
        selCol.SetMode(SelectMode::Col);
        selCol.AddCol(0);
        Renderer r3; r3.SetData(&map); r3.SetFrame(&fm); r3.SetSelection(&selCol); r3.SetParams(p);
        CHECK(r3.IsSelected(0),  "col 0 bit 0");
        CHECK(r3.IsSelected(64), "col 0 bit 64 (frame 1)");
        CHECK(!r3.IsSelected(8), "col 0 excludes bit 8 (col 1)");
    }

    // ---- 块选取：二维（行=帧, 列=字节）----
    {
        SelectionSet sel;
        sel.SetMode(SelectMode::Rect);
        RectSel rr; rr.frame0 = 0; rr.cell0 = 0; rr.frame1 = 1; rr.cell1 = 1;
        sel.AddRect(rr);
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetSelection(&sel); r.SetParams(p);
        CHECK(r.IsSelected(0),  "rect: frame0 cell0");
        CHECK(r.IsSelected(8),  "rect: frame0 cell1");
        CHECK(!r.IsSelected(16), "rect: frame0 cell2 excluded");
        CHECK(r.IsSelected(64), "rect: frame1 cell0");
        CHECK(r.IsSelected(72), "rect: frame1 cell1");
        CHECK(!r.IsSelected(128), "rect: frame2 excluded");
        CHECK(sel.SelectedBitsEx(64, 8, 128) == 32,
              "rect selected bits = 2 rows * 2 cells * 8");
    }

    // ---- 连续选区可累积多段（原版 TList + Sort@0x4033AC）----
    {
        SelectionSet sel;
        sel.AddContinuous(32, 39);   // 第二次拖动先到后面
        sel.AddContinuous(0, 7);     // 第一次拖动
        CHECK(sel.Continuouses().size() == 2,
              "continuous keeps one record per drag");
        CHECK(sel.ContinuousStart() == 0 && sel.ContinuousEnd() == 39,
              "continuous list is sorted by start bit");
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetSelection(&sel); r.SetParams(p);
        CHECK(r.IsSelected(3) && r.IsSelected(35), "both continuous ranges render");
        CHECK(!r.IsSelected(20), "gap between continuous ranges stays unselected");
        CHECK(sel.SelectedBits() == 16, "selected bits sum over continuous ranges");

        sel.AddRowRange(3, 1);
        sel.AddRowRange(2, 4);
        CHECK(sel.Rows().size() == 4 && sel.Rows().front() == 1 &&
              sel.Rows().back() == 4, "row range union is sorted and unique");
        sel.AddColRange(0, 2);
        sel.AddColRange(2, 3);
        CHECK(sel.Cols().size() == 4, "col range union is sorted and unique");

        CHECK(!sel.ToggleRow(2) && sel.Rows().size() == 3,
              "ToggleRow removes an already selected row");
        CHECK(sel.ToggleRow(2) && sel.Rows().size() == 4 && sel.Rows()[1] == 2,
              "ToggleRow re-adds a row in sorted order");
        CHECK(!sel.ToggleCol(0) && sel.Cols().size() == 3,
              "ToggleCol removes an already selected column");
        CHECK(sel.ToggleCol(0) && sel.Cols().size() == 4 && sel.Cols().front() == 0,
              "ToggleCol re-adds a column in sorted order");

        sel.AddContinuous(0, 1);
        sel.ClearMode(SelectMode::Continuous);
        CHECK(sel.Continuouses().empty() && sel.Rows().size() == 4 && sel.Cols().size() == 4,
              "ClearMode only clears the given mode's list");
    }

    // ---- 不等长帧布局 ----
    {
        FrameModel vf = fm; vf.bVariable = true;
        VariableFrameSet frames;
        frames.Add(0, 64);
        frames.Add(64, 128);
        Renderer r;
        r.SetData(&map); r.SetFrame(&vf); r.SetParams(p); r.SetVariableFrames(&frames);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        CHECK(rows == 2, "var frame: 2 rows");
        CHECK(cols == 16, "var frame: widest frame 16 cells");
        CHECK(!cells.empty() && cells[0].bitPos == 0, "var frame row0 start 0");
        bool found64 = false;
        for (const Cell& c : cells) if (c.bitPos == 64) found64 = true;
        CHECK(found64, "var frame row1 starts at 64");
        wchar_t buf[32];
        CHECK(r.YAxisText(1, buf, 32) && std::wstring(buf) == L"1", "var YFN row1 = 1");
    }

    // ---- 过滤：可见帧映射 ----
    {
        std::vector<uint64_t> visible{1, 3};
        Renderer r;
        r.SetData(&map); r.SetFrame(&fm); r.SetParams(p);
        r.SetVisibleFrames(&visible, true);
        std::vector<Cell> cells; uint64_t rows=0, cols=0;
        r.Layout(800, 400, 10, 20, 0, 0, cells, rows, cols);
        CHECK(rows == 2, "filter: 2 visible rows");
        CHECK(!cells.empty() && cells[0].bitPos == 64, "filter: first row = frame 1");
        wchar_t buf[32];
        CHECK(r.YAxisText(0, buf, 32) && std::wstring(buf) == L"1", "filter YFN = 1");
        CHECK(r.RowStartBit(0) == 64, "filter: row 0 start = frame 1 (64)");
        CHECK(r.RowStartBit(1) == 192, "filter: row 1 start = frame 3 (192)");

        SelectionSet sel;
        sel.SetMode(SelectMode::Rect);
        RectSel rr; rr.frame0 = 1; rr.cell0 = 0; rr.frame1 = 3; rr.cell1 = 1;
        sel.AddRect(rr);
        r.SetSelection(&sel);
        CHECK(r.IsSelected(64),  "filter rect: frame1 cell0 selected");
        CHECK(r.IsSelected(192), "filter rect: frame3 cell0 selected");
        CHECK(!r.IsSelected(256), "filter rect: frame4 excluded");
        CHECK(r.CellColumnOfBit(72) == 1, "filter CellColumnOfBit(72)=1");
    }

    // ---- 纵轴 ----
    {
        Renderer r; r.SetData(&map); r.SetFrame(&fm); r.SetParams(p);
        wchar_t buf[32];
        CHECK(r.YAxisText(0, buf, 32) && std::wstring(buf) == L"0", "YFN row 0 = '0'");
        RenderParams py = p; py.yAxis = YAxisMode::YFL;
        Renderer ry; ry.SetData(&map); ry.SetFrame(&fm); ry.SetParams(py);
        CHECK(ry.YAxisText(0, buf, 32) && std::wstring(buf) == L"64", "YFL = 64");
        py.yAxis = YAxisMode::YFB;
        Renderer rb; rb.SetData(&map); rb.SetFrame(&fm); rb.SetParams(py);
        CHECK(rb.YAxisText(1, buf, 32) && std::wstring(buf) == L"64", "YFB row1 = 64");

        // 排布取值即原版主窗体全局字段 [form+0x498]：PageBtnClick 写 0、
        // HorizBtnClick 写 1、VertBtnClick 写 2、PageHorizBtnClick 写 3、
        // PageVertBtnClick 写 4（五个处理函数的立即数）。
        CHECK(static_cast<int>(ViewLayout::Paged) == 0 &&
              static_cast<int>(ViewLayout::Horizontal) == 1 &&
              static_cast<int>(ViewLayout::Vertical) == 2 &&
              static_cast<int>(ViewLayout::GroupH) == 3 &&
              static_cast<int>(ViewLayout::GroupV) == 4 &&
              std::wstring(ViewLayoutName(ViewLayout::GroupV)) == L"分组竖排",
              "ViewLayout 编码与 [form+0x498] 一一对应");

        // 本版本扩展：横排/分组横排时行是连续数据流（原版排布只重排窗口，不改字节流）
        RenderParams ph = p;
        ph.layout = ViewLayout::Horizontal;
        ph.view = ViewMode::Hex;
        ph.yAxis = YAxisMode::YFN;
        Renderer rh; rh.SetData(&map); rh.SetFrame(&fm); rh.SetParams(ph);
        rh.SetColsPerRow(8);   // 8 字节 = 64 比特/行
        CHECK(rh.YAxisText(2, buf, 32) && std::wstring(buf) == L"2",
              "horizontal YFN row2 = 2");
        ph.yAxis = YAxisMode::YFL;
        Renderer rhl; rhl.SetData(&map); rhl.SetFrame(&fm); rhl.SetParams(ph);
        rhl.SetColsPerRow(8);
        CHECK(rhl.YAxisText(0, buf, 32) && std::wstring(buf) == L"64",
              "horizontal YFL = 64");
        ph.yAxis = YAxisMode::YFB;
        Renderer rhb; rhb.SetData(&map); rhb.SetFrame(&fm); rhb.SetParams(ph);
        rhb.SetColsPerRow(8);
        CHECK(rhb.YAxisText(3, buf, 32) && std::wstring(buf) == L"192",
              "horizontal YFB row3 = 192");
        CHECK(!rhb.YAxisText(128, buf, 32), "horizontal row past end rejected");
    }

    // ---- 书签 ----
    {
        BookmarkSet bm;
        bm.Set(100); bm.Set(200); bm.Set(300);
        uint64_t out = 0;
        CHECK(bm.Next(100, out) && out == 200, "bookmark Next");
        CHECK(bm.Prev(200, out) && out == 100, "bookmark Prev");
        CHECK(bm.Has(100) && !bm.Has(400), "bookmark Has");
    }

    // ---- 独特码：超过 32 比特仍完整比较 ----
    {
        UniqueCodeSpec spec;
        spec.code = { 0x1A, 0xCF, 0xFC, 0x1D, 0xFF };
        spec.lengthBits = 40;
        std::atomic<bool> stop{false};
        std::vector<SearchHit> hits;
        SearchEngine eng;
        CHECK(eng.FindUniqueCode(map, spec, 0, false, hits, nullptr, stop), "unique search runs");
        CHECK(FindHit(hits, 200), "unique 40-bit hit at 200");
        CHECK(!FindHit(hits, 280), "unique 40-bit does not match first 32 bits only");
    }

    // ---- 独特码：去四相模糊 ----
    {
        UniqueCodeSpec spec;
        spec.code = { 0x0F };
        spec.removeAmbiguity = true;
        std::atomic<bool> stop{false};
        std::vector<SearchHit> hits;
        SearchEngine eng;
        eng.FindUniqueCode(map, spec, 0, false, hits, nullptr, stop);
        CHECK(FindHit(hits, 400), "ambiguity: inverse 0xF0 matched");

        UniqueCodeSpec noAmb = spec; noAmb.removeAmbiguity = false;
        std::vector<SearchHit> hits2;
        eng.FindUniqueCode(map, noAmb, 0, false, hits2, nullptr, stop);
        CHECK(!FindHit(hits2, 400), "no ambiguity: 0xF0 not matched");
    }

    // ---- 帧长查找：同步列（全0/1列） ----
    {
        const std::wstring syncPath = std::wstring(tmpDir) + L"bes_sync.dat";
        std::vector<uint8_t> syncData(512);
        uint32_t rnd = 0x12345678;
        for (size_t i = 0; i < syncData.size(); ++i) {
            rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;
            syncData[i] = static_cast<uint8_t>(rnd & 0xFF);
        }
        // 每 8 字节一帧，帧首字节固定 → 64 比特帧长有 8 个全0/1列
        for (size_t f = 0; f + 8 <= syncData.size(); f += 8)
            syncData[f] = 0xEB;
        CHECK(MakeFile(syncPath, syncData), "sync sample created");

        FileMap syncMap;
        std::wstring syncErr;
        CHECK(syncMap.Open(syncPath, &syncErr), "sync map opens");

        std::atomic<bool> stop{false};
        FrameLengthSpec spec;
        spec.startLenBits = 2;
        spec.rowCount = 16;
        spec.minSyncCols = 4;
        FrameLengthResult res;
        SearchEngine eng;
        CHECK(eng.FindFrameLength(syncMap, spec, res, nullptr, stop),
              "sync frame length runs");
        CHECK(res.frameLenBits == 64, "sync frame length finds 64 bits");
        CHECK(res.syncColumns >= 8, "sync frame length reports uniform columns");

        spec.startLenBits = 65;   // 从下一帧长开始
        FrameLengthResult res2;
        eng.FindFrameLength(syncMap, spec, res2, nullptr, stop);
        CHECK(res2.frameLenBits != 64, "sync frame length honors start length");
        ::DeleteFileW(syncPath.c_str());
    }

    // ---- 帧模型 ----
    {
        FrameModel f2; f2.frameLen = 64; f2.startBit = 0;
        CHECK(f2.FrameOf(64) == 1, "FrameOf(64) == 1");
        CHECK(f2.BitInFrame(70) == 6, "BitInFrame(70) == 6");
        f2.step = 8;
        CHECK(f2.FrameCount(640) == 10, "FrameCount uses frameLen, not step");
        f2.step = 0x7FFFFFFF;
        f2.SanitizeStep();
        CHECK(f2.step == 1, "SanitizeStep clamps huge step");
    }

    // ---- 输出：块选取按行列输出 ----
    {
        SelectionSet sel;
        RectSel rr; rr.frame0 = 0; rr.cell0 = 0; rr.frame1 = 1; rr.cell1 = 1;
        sel.AddRect(rr);
        OutputOptions opt;
        opt.target = OutputTarget::File;
        opt.filePath = (std::wstring(tmpDir) + L"bes_out.dat");
        opt.wholeData = false;
        opt.rectSel = true;
        opt.bitsPerCell = 8;
        uint64_t written = 0;
        OutputWriter w;
        CHECK(w.Write(map, fm, sel, opt, written, &err), "OutputWriter rect write");
        // 2 帧 * 2 字节 = 4 字节
        CHECK(written == 4, "OutputWriter rect bytes = 4");
        DeleteFileW(opt.filePath.c_str());
    }

    // ---- 选区序列化 ----
    {
        SelectionSet sel;
        sel.SetMode(SelectMode::Rect);
        RectSel rr; rr.frame0 = 2; rr.cell0 = 3; rr.frame1 = 4; rr.cell1 = 5;
        sel.AddRect(rr);
        sel.SetContinuous(100, 200);
        SelectionSet out;
        CHECK(out.Deserialize(sel.Serialize(), &err), "selection deserialize");
        CHECK(out.HasContinuous() && out.ContinuousStart() == 100, "selection continuous roundtrip");
        CHECK(out.Rects().size() == 1 && out.Rects()[0].frame0 == 2 &&
              out.Rects()[0].cell1 == 5, "selection rect roundtrip");
    }

    // ---- 原版 .slp 二进制格式：3 字节 "slp" + 4 段（Int32 数量 + 24 字节记录）----
    {
        SelectionSet sel;
        sel.AddRow(7);
        sel.AddRow(8);
        RectSel rr; rr.frame0 = 2; rr.cell0 = 3; rr.frame1 = 4; rr.cell1 = 5;
        sel.AddRect(rr);
        sel.SetContinuous(100, 200);   // 连续段最后写入 -> 模式为连续
        sel.AddCol(1);
        const std::vector<uint8_t> blob = sel.SaveSlp();
        CHECK(blob[0] == 's' && blob[1] == 'l' && blob[2] == 'p', ".slp magic = raw \"slp\"");
        // 段序 = 连续 / 块 / 行 / 列（按钮 Tag 0..3 → 对象 +4/+8/+0x0C/+0x10 四个列表的顺序）
        // 3 + (4+24) 连续 + (4+24) 块 + (4+2×24) 行 + (4+24) 列
        CHECK(blob.size() == 3 + 28 + 28 + 52 + 28, ".slp section sizes = count + 24-byte records");
        auto countAt = [&blob](size_t off) {
            return static_cast<int32_t>(blob[off] | (blob[off + 1] << 8) |
                                        (blob[off + 2] << 16) | (blob[off + 3] << 24));
        };
        CHECK(countAt(3) == 1 && countAt(31) == 1 && countAt(59) == 2 && countAt(111) == 1,
              ".slp 段计数：连续 1 / 块 1 / 行 2 / 列 1");
        // 连续记录：Int64 起点在 +0，Int64 终点在 +0x0C，两个 Int32 为 0
        CHECK(blob[7] == 100 && blob[8] == 0 && blob[19] == 200 && blob[27] == 0,
              ".slp continuous record = Int64 pair, Int32 fields zero");
        // 行记录：行号是 64 位（+0 起），列字段留空
        CHECK(blob[63] == 7 && blob[71] == 0 && blob[87] == 8,
              ".slp row record = Int64 row, Int32 col unused");

        SelectionSet back;
        CHECK(back.LoadSlp(blob, &err), ".slp load");
        CHECK(back.Rows().size() == 2 && back.Rows()[0] == 7 && back.Rows()[1] == 8,
              ".slp rows roundtrip");
        CHECK(back.Rects().size() == 1 && back.Rects()[0].frame0 == 2 &&
              back.Rects()[0].cell0 == 3 && back.Rects()[0].frame1 == 4 &&
              back.Rects()[0].cell1 == 5, ".slp rect roundtrip");
        CHECK(back.HasContinuous() && back.ContinuousStart() == 100 &&
              back.ContinuousEnd() == 200, ".slp continuous roundtrip");
        CHECK(back.Cols().size() == 1 && back.Cols()[0] == 1, ".slp cols roundtrip");
        CHECK(back.Mode() == SelectMode::Continuous, ".slp mode inferred from sections");

        std::vector<uint8_t> bad = blob;
        bad[0] = 'B';
        std::wstring badErr;
        SelectionSet tmp;
        CHECK(!tmp.LoadSlp(bad, &badErr), ".slp rejects wrong magic");
        CHECK(badErr == L"不是选取参数文件！", ".slp rejection text");
        bad = blob;
        bad.resize(20);   // 截断
        CHECK(!tmp.LoadSlp(bad, &badErr), ".slp rejects truncated data");
    }

    // ---- .slp 连续段保存多段（每次拖动一条记录）----
    {
        SelectionSet sel;
        sel.AddContinuous(200, 300);
        sel.AddContinuous(100, 150);
        const std::vector<uint8_t> blob = sel.SaveSlp();
        // 3 + 连续(4+2×24) + 块(4) + 行(4) + 列(4)
        CHECK(blob.size() == 3 + 4 + 2 * 24 + 4 + 4 + 4,
              ".slp multi-range continuous size");
        const int32_t count = static_cast<int32_t>(
            blob[3] | (blob[4] << 8) | (blob[5] << 16) | (blob[6] << 24));
        CHECK(count == 2, ".slp writes every continuous range");
        auto i64At = [&blob](size_t off) {
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v |= static_cast<uint64_t>(blob[off + i]) << (8 * i);
            return static_cast<int64_t>(v);
        };
        CHECK(i64At(7) == 100 && i64At(19) == 150 &&
              i64At(31) == 200 && i64At(43) == 300,
              ".slp continuous records keep start/end");

        SelectionSet back;
        CHECK(back.LoadSlp(blob, &err), ".slp multi-range load");
        CHECK(back.Continuouses().size() == 2 &&
              back.Continuouses()[0].startBit == 100 &&
              back.Continuouses()[1].startBit == 200,
              ".slp roundtrip keeps multiple sorted continuous ranges");
    }

    // ---- 编辑：可增长内存副本 + 改写/插入/删除 + 撤销/重做 + 落盘 ----
    {
        const std::wstring epath = std::wstring(tmpDir) + L"bes_edit.dat";
        const std::vector<uint8_t> seed = { 0, 1, 2, 3, 4, 5, 6, 7 };
        CHECK(MakeFile(epath, seed), "edit sample created");

        FileMap em;
        CHECK(em.OpenEditable(epath, &err), "edit sample opens editable");
        CHECK(em.IsEditable() && em.SizeBytes() == 8, "editable buffer is growable");

        BinaryEditor ed(em);
        const uint8_t one = 0xEE;
        const uint8_t pair[2] = { 0xA0, 0xA1 };
        CHECK(ed.Overwrite(1, &one, 1), "overwrite byte");
        CHECK(em.ReadByteAtBit(8) == 0xEE, "overwritten byte reads back");
        CHECK(ed.Dirty(), "overwrite marks document dirty");
        CHECK(ed.Undo(), "undo overwrite");
        CHECK(em.ReadByteAtBit(8) == 1, "undo restores previous byte");
        CHECK(ed.Redo(), "redo overwrite");
        CHECK(em.ReadByteAtBit(8) == 0xEE, "redo reapplies byte");

        CHECK(ed.Insert(0, pair, 2), "insert bytes");
        CHECK(em.SizeBytes() == 10 && em.ReadByteAtBit(0) == 0xA0, "insert shifts existing data");
        CHECK(ed.Remove(9, 1), "delete byte");
        CHECK(em.SizeBytes() == 9, "delete shrinks data");
        CHECK(ed.Undo() && ed.Undo() && em.SizeBytes() == 8, "undo delete then undo insert");
        CHECK(em.ReadByteAtBit(8) == 0xEE, "earlier overwrite survives undo stack");

        CHECK(ed.Save(&err), "save writes buffer back to the file");
        CHECK(!ed.Dirty(), "save clears dirty flag");
        ed.Overwrite(0, &one, 1);
        CHECK(em.ReadByteAtBit(0) == 0xEE, "edit after save");
        CHECK(ed.Discard(&err), "discard reverts to on-disk content");
        CHECK(em.SizeBytes() == 8 && em.ReadByteAtBit(0) == 0 &&
              em.ReadByteAtBit(8) == 0xEE, "discard restores saved bytes");
        CHECK(em.IsEditable(), "discard keeps the editing buffer");
        CHECK(!ed.Dirty() && ed.UndoDepth() == 0, "discard clears dirty and undo stack");
        CHECK(ed.Insert(0, pair, 2) && em.SizeBytes() == 10, "insert still works after discard");
        CHECK(ed.Remove(0, 2) && em.SizeBytes() == 8, "delete still works after discard");
        CHECK(ed.Undo() && ed.Undo() && em.SizeBytes() == 8, "undo insert and delete");

        // 映射方式（未进入编辑态）：只能就地改写，插入/删除被拒绝
        FileMap mm;
        CHECK(mm.Open(epath, &err), "same file opens mapped");
        BinaryEditor med(mm);
        CHECK(!med.Insert(0, pair, 2), "insert requires editable mode");
        CHECK(!med.Remove(0, 1), "delete requires editable mode");
        CHECK(med.Overwrite(7, pair, 2), "tail overwrite clamps to file length");
        CHECK(mm.SizeBytes() == 8 && mm.ReadByteAtBit(56) == 0xA0,
              "clamped overwrite writes only the in-range byte");
        mm.Close();
        DeleteFileW(epath.c_str());
    }

    // ---- 帧过滤：多条件“与”、数值低字节在前、CRC 帧尾校验 ----
    {
        const std::wstring fpath = std::wstring(tmpDir) + L"bes_filter.dat";
        const std::vector<uint8_t> fdata = {
            0x12, 0x34, 0x0E, 0xC9,   // CRC 通过
            0x12, 0x34, 0x00, 0x01,   // CRC 失败
            0x56, 0x78, 0x46, 0x89,   // CRC 通过
            0x56, 0x78, 0x00, 0x03    // CRC 失败
        };
        CHECK(MakeFile(fpath, fdata), "filter sample created");
        FileMap fmap;
        CHECK(fmap.Open(fpath, &err), "filter sample opens");

        FrameModel ffm;
        ffm.startBit = 0;
        ffm.frameLen = 32;
        SearchEngine feng;
        std::atomic<bool> fstop{false};
        std::vector<uint64_t> idx;

        FrameFilter ff;
        FilterCondition fl;
        fl.kind = FilterKind::FrameLength;
        fl.frameBits = 32;
        fl.frameOp = CmpOp::Eq;
        ff.conditions.push_back(fl);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) && idx.size() == 4,
              "filter frame length == 32 matches 4");

        FilterCondition fv;
        fv.kind = FilterKind::Value;
        fv.valuePos = 0;
        fv.valueBits = 16;
        fv.value = 0x3412;
        fv.valueOp = CmpOp::Eq;
        ff.conditions.assign(1, fv);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) &&
              idx.size() == 2 && idx[0] == 0 && idx[1] == 1,
              "filter value reads low byte first");

        FilterCondition fc;
        fc.kind = FilterKind::Crc;
        fc.crcPass = true;
        ff.conditions.assign(1, fc);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) &&
              idx.size() == 2 && idx[0] == 0 && idx[1] == 2,
              "filter CRC pass checks tail");

        fc.crcPass = false;
        ff.conditions.assign(1, fc);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) &&
              idx.size() == 2 && idx[0] == 1 && idx[1] == 3,
              "filter CRC fail checks tail");

        ff.conditions = { fv, fc };
        fc.crcPass = true;
        ff.conditions[1] = fc;
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) &&
              idx.size() == 1 && idx[0] == 0,
              "filter conditions are ANDed");

        FilterCondition fb;
        fb.kind = FilterKind::Value;
        fb.valuePos = 0;
        fb.valueBits = 1;
        fb.value = 0;
        fb.valueOp = CmpOp::Eq;
        ff.conditions.assign(1, fb);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) && idx.size() == 4,
              "filter bit value uses bit position");

        fb.value = 1;
        ff.conditions.assign(1, fb);
        CHECK(feng.ApplyFilter(fmap, ffm, ff, idx, nullptr, fstop) && idx.empty(),
              "filter bit value mismatch");

        fmap.Close();
        DeleteFileW(fpath.c_str());
    }

    // ---- 构件数据流输入管线（FlowEngine：选区 → 端口字节）----
    {
        struct MemPort : IFlowPort {
            std::vector<uint8_t> bytes;
            std::vector<size_t>  writes;
            int  begins = 0, ends = 0;
            FlowParams seen;
            void Begin(const FlowParams& p) override { ++begins; seen = p; }
            bool Write(const uint8_t* d, size_t n) override
            { bytes.insert(bytes.end(), d, d + n); writes.push_back(n); return true; }
            void End(const FlowParams&) override { ++ends; }
        };

        FlowEngine eng;
        std::wstring ferr;
        FlowParams all;                       // mode = None（全部数据）
        CHECK(!eng.Start(map, fm, SelectionSet{}, all, &ferr),
              "未绑定端口时拒绝开始");
        CHECK(ferr == L"没有可用的数据流输入端口！", "端口缺失的提示文案");

        MemPort port;
        eng.BindPort(&port);
        FileMap empty;
        CHECK(!eng.Start(empty, fm, SelectionSet{}, all, &ferr) &&
              ferr == L"数据长度为0！", "空数据提示「数据长度为0！」");

        CHECK(eng.Start(map, fm, SelectionSet{}, all, &ferr), "全部数据推送成功");
        CHECK(port.bytes == data && eng.state() == FlowState::Finished,
              "全部数据 = 整个文件字节，状态转 Finished");
        CHECK(port.begins == 1 && port.ends == 1, "Begin/End 各一次");
        CHECK(eng.bytesWritten() == 1024 && eng.totalBytes() == 1024, "计数为总字节");

        // 块上限 [ctx+0x8c] = 0x10000 → 一次写完；改小后按块切分
        port.bytes.clear(); port.writes.clear();
        FlowParams chunked = all; chunked.blockBytes = 100;
        CHECK(eng.Start(map, fm, SelectionSet{}, chunked, &ferr) &&
              port.writes.size() == 11 && port.writes.back() == 24,
              "块上限 100 → 11 块，末块 24 字节");

        // 行选取：帧 1 与帧 3（每帧 8 字节）
        port.bytes.clear();
        SelectionSet rows;
        rows.AddRow(1); rows.AddRow(3);
        FlowParams p2; p2.mode = SelectMode::Row;
        CHECK(eng.Start(map, fm, rows, p2, &ferr) && port.bytes.size() == 16 &&
              port.bytes[0] == data[8] && port.bytes[8] == data[24],
              "行选取只推送被选帧的字节");

        // 反选：行 0 之外全部 → 8..1024
        port.bytes.clear();
        FlowParams p3 = p2; p3.deselect = true;
        SelectionSet oneRow; oneRow.AddRow(0);
        CHECK(eng.Start(map, fm, oneRow, p3, &ferr) && port.bytes.size() == 1016 &&
              port.bytes.front() == data[8],
              "反选取补集：跳过首帧后推剩余字节");

        // 块选取：帧 0..1、格 1..2（每格 1 字节）→ 相邻块合并
        port.bytes.clear(); port.writes.clear();
        SelectionSet rects;
        RectSel r; r.frame0 = 0; r.cell0 = 1; r.frame1 = 1; r.cell1 = 2;
        rects.AddRect(r);
        FlowParams p4; p4.mode = SelectMode::Rect;
        CHECK(eng.Start(map, fm, rects, p4, &ferr) && port.writes.size() == 2 &&
              port.bytes[0] == data[1] && port.bytes[2] == data[9],
              "块选取逐帧成段（宽 = 格数 × 每格比特）");

        // 列选取：格 0 → 每帧 1 字节，128 帧
        port.bytes.clear();
        SelectionSet cols; cols.AddCol(0);
        FlowParams p5; p5.mode = SelectMode::Col;
        CHECK(eng.Start(map, fm, cols, p5, &ferr) && port.bytes.size() == 128 &&
              port.bytes[1] == data[8],
              "列选取按帧步进取样");

        // 连续选取按比特区间换算成字节（8..23 位 → 字节 1..2）
        port.bytes.clear();
        SelectionSet cont; cont.SetContinuous(8, 23);
        FlowParams p6; p6.mode = SelectMode::Continuous;
        CHECK(eng.Start(map, fm, cont, p6, &ferr) && port.bytes.size() == 2 &&
              port.bytes[0] == data[1], "连续选取换算为覆盖的字节");

        // 比特模式：每格 1 比特
        port.bytes.clear();
        SelectionSet bitCols; bitCols.AddCol(0);
        FlowParams p7 = p6; p7.mode = SelectMode::Col; p7.bitsPerCell = 1;
        p7.param = L"列选取";
        CHECK(eng.Start(map, fm, bitCols, p7, &ferr) && port.bytes.size() == 128 &&
              eng.params().param == L"列选取",
              "比特模式下每格仍占 1 字节容器，参数随运行保留");

        // 停止：块间回调里停 → 少于总量、状态 Stopped
        port.bytes.clear();
        FlowParams stopParams = all; stopParams.blockBytes = 100;
        eng.SetPump([&eng](const FlowEngine& e) {
            if (e.bytesWritten() >= 300) const_cast<FlowEngine&>(e).Stop();
        });
        CHECK(!eng.Start(map, fm, SelectionSet{}, stopParams, &ferr) &&
              eng.state() == FlowState::Stopped &&
              eng.bytesWritten() >= 300 && eng.bytesWritten() < 1024,
              "停止：进度保留且少于总量");

        // 暂停/继续：回调里暂停，暂停循环里继续
        eng.SetPump([&eng](const FlowEngine& e) {
            if (e.bytesWritten() == 100) const_cast<FlowEngine&>(e).Pause();
            if (e.state() == FlowState::Paused) const_cast<FlowEngine&>(e).Resume();
        });
        port.bytes.clear();
        CHECK(eng.Start(map, fm, SelectionSet{}, stopParams, &ferr) &&
              eng.bytesWritten() == 1024 && eng.state() == FlowState::Finished,
              "暂停后继续：搬运不丢数据");
        CHECK(eng.comCommand() == 4, "继续 = 构件命令 4（0x41623C）");

        // 退出请求：回调里 RequestExit → 中断且不算完成
        eng.SetPump([&eng](const FlowEngine& e) {
            if (e.bytesWritten() == 100) const_cast<FlowEngine&>(e).RequestExit();
        });
        CHECK(!eng.Start(map, fm, SelectionSet{}, stopParams, &ferr) &&
              eng.exitRequested() && eng.state() == FlowState::Stopped &&
              eng.bytesWritten() == 100,
              "退出请求中断搬运（[ctx+0x18]）");
        eng.SetPump(nullptr);

        // 复位：回到空闲、计数清零（构件命令 5）
        eng.Reset();
        CHECK(eng.state() == FlowState::Idle && eng.bytesWritten() == 0 &&
              eng.comCommand() == 5, "复位 = 命令 5 并回空闲");

        // 端口写入失败 → 「端口输入失败！」
        struct BadPort : IFlowPort {
            bool Write(const uint8_t*, size_t) override { return false; }
        } bad;
        eng.BindPort(&bad);
        CHECK(!eng.Start(map, fm, SelectionSet{}, all, &ferr) &&
              ferr == L"端口输入失败！", "端口写失败提示");
    }

    map.Close();
    DeleteFileW(path.c_str());

    if (g_fail == 0) printf("\nALL PASSED (0 failures)\n");
    else printf("\n%d FAILURES\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
