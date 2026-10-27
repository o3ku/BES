// Selection.cpp
#include "core/Selection.h"
#include <sstream>
#include <algorithm>
#include <array>
#include <cstdint>

namespace bes {

const wchar_t* SelectModeName(SelectMode m) {
    switch (m) {
    case SelectMode::Continuous: return L"连续选取";
    case SelectMode::Rect:       return L"块选取";
    case SelectMode::Row:        return L"行选取";
    case SelectMode::Col:        return L"列选取";
    default:                     return L"无";
    }
}

uint64_t SelectionSet::SelectedBits() const {
    uint64_t total = 0;
    for (const ContinuousSel& s : m_continuous)
        total += s.endBit - s.startBit + 1;
    for (const RectSel& r : m_rects) {
        if (r.cell1 >= r.cell0 && r.frame1 >= r.frame0)
            total += (r.cell1 - r.cell0 + 1) * (r.frame1 - r.frame0 + 1);
    }
    total += m_rows.size();
    total += m_cols.size();
    return total;
}

uint64_t SelectionSet::SelectedBitsEx(uint64_t frameLenBits, int bitsPerCell,
                                      uint64_t frameCount) const {
    if (bitsPerCell < 1) bitsPerCell = 1;
    uint64_t total = 0;

    for (const ContinuousSel& s : m_continuous)
        total += s.endBit - s.startBit + 1;

    for (const RectSel& r : m_rects) {
        if (r.cell1 < r.cell0 || r.frame1 < r.frame0) continue;
        uint64_t rows = r.frame1 - r.frame0 + 1;
        if (frameCount > 0 && r.frame0 >= frameCount) continue;
        if (frameCount > 0 && r.frame1 >= frameCount)
            rows = frameCount - r.frame0;
        const uint64_t cols = r.cell1 - r.cell0 + 1;
        total += rows * cols * static_cast<uint64_t>(bitsPerCell);
    }

    uint64_t rowCount = 0;
    for (uint64_t row : m_rows) {
        if (frameCount == 0 || row < frameCount) ++rowCount;
    }
    total += rowCount * frameLenBits;

    uint64_t colCount = 0;
    for (uint64_t col : m_cols) {
        (void)col;
        ++colCount;
    }
    total += colCount * frameCount * static_cast<uint64_t>(bitsPerCell);

    return total;
}

namespace {

constexpr size_t kSlpRecord = 24;

void PutI32(std::vector<uint8_t>& v, int32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 0xFF));
}
void PutI64(std::vector<uint8_t>& v, int64_t x) {
    for (int i = 0; i < 8; ++i) v.push_back(static_cast<uint8_t>((x >> (8 * i)) & 0xFF));
}
template <class T>
bool Take(const std::vector<uint8_t>& b, size_t& p, T& out) {
    if (p + sizeof(T) > b.size()) return false;
    uint64_t acc = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        acc |= static_cast<uint64_t>(b[p + i]) << (8 * i);
    p += sizeof(T);
    out = static_cast<T>(acc);
    return true;
}

// 一条 24 字节记录：{Int64 p1; Int32 c1; Int64 p2; Int32 c2}
void PutRecord(std::vector<uint8_t>& v, int64_t p1, int32_t c1, int64_t p2, int32_t c2) {
    PutI64(v, p1); PutI32(v, c1); PutI64(v, p2); PutI32(v, c2);
}

void PutSection(std::vector<uint8_t>& v, const std::vector<std::array<int64_t, 4>>& recs) {
    PutI32(v, static_cast<int32_t>(recs.size()));
    for (const auto& r : recs)
        PutRecord(v, r[0], static_cast<int32_t>(r[1]), r[2], static_cast<int32_t>(r[3]));
}

} // namespace

std::vector<uint8_t> SelectionSet::SaveSlp() const {
    std::vector<uint8_t> out;
    out.push_back('s'); out.push_back('l'); out.push_back('p');

    // 段 0：连续选取（每段一条记录；原版的两个 Int32 附属字段写 0）
    std::vector<std::array<int64_t, 4>> cont;
    for (const ContinuousSel& s : m_continuous)
        cont.push_back({ static_cast<int64_t>(s.startBit), 0,
                         static_cast<int64_t>(s.endBit), 0 });
    PutSection(out, cont);

    // 段 1：块选取（左上/右下两个「行+列」角标）
    std::vector<std::array<int64_t, 4>> rects;
    for (const RectSel& r : m_rects)
        rects.push_back({ static_cast<int64_t>(r.frame0), static_cast<int64_t>(r.cell0),
                          static_cast<int64_t>(r.frame1), static_cast<int64_t>(r.cell1) });
    PutSection(out, rects);

    // 段 2：行选取（原版按 Int64 行号区间合并存放；每行退化为 row1=row2 的记录，
    // 读取端按 row1..row2 展开，两种写法互容）
    std::vector<std::array<int64_t, 4>> rows;
    for (uint64_t r : m_rows) rows.push_back({ static_cast<int64_t>(r), 0,
                                               static_cast<int64_t>(r), 0 });
    PutSection(out, rows);

    // 段 3：列选取（Int32 起 + Int32 止 的闭区间）
    std::vector<std::array<int64_t, 4>> cols;
    for (uint64_t c : m_cols)
        cols.push_back({ 0, static_cast<int64_t>(c), 0, static_cast<int64_t>(c) });
    PutSection(out, cols);

    return out;
}

bool SelectionSet::LoadSlp(const std::vector<uint8_t>& data, std::wstring* err) {
    auto reject = [this, err] {
        Clear();
        if (err) *err = L"不是选取参数文件！";
        return false;
    };
    if (data.size() < 3 || data[0] != 's' || data[1] != 'l' || data[2] != 'p')
        return reject();

    Clear();
    size_t p = 3;
    for (int section = 0; section < 4; ++section) {
        int32_t count = 0;
        if (!Take(data, p, count) || count < 0) return reject();
        for (int32_t i = 0; i < count; ++i) {
            const size_t recStart = p;
            int64_t p1 = 0, p2 = 0;
            int32_t c1 = 0, c2 = 0;
            if (!Take(data, p, p1) || !Take(data, p, c1) ||
                !Take(data, p, p2) || !Take(data, p, c2) || p != recStart + kSlpRecord)
                return reject();
            switch (section) {
            case 0: {
                // 连续选取：原版按列表保存多段，读入后按起点排序保持多段
                const uint64_t lo = static_cast<uint64_t>(std::max<int64_t>(0, std::min(p1, p2)));
                const uint64_t hi = static_cast<uint64_t>(std::max<int64_t>(0, std::max(p1, p2)));
                AddContinuous(lo, hi);
                break;
            }
            case 1: {
                RectSel r;
                r.frame0 = static_cast<uint64_t>(std::max<int64_t>(0, std::min(p1, p2)));
                r.frame1 = static_cast<uint64_t>(std::max<int64_t>(0, std::max(p1, p2)));
                r.cell0  = static_cast<uint64_t>(std::max<int64_t>(0, std::min<int64_t>(c1, c2)));
                r.cell1  = static_cast<uint64_t>(std::max<int64_t>(0, std::max<int64_t>(c1, c2)));
                m_rects.push_back(r);
                break;
            }
            case 2:
                for (int64_t r = std::max<int64_t>(0, std::min(p1, p2));
                     r <= std::max(p1, p2); ++r)
                    m_rows.push_back(static_cast<uint64_t>(r));
                break;
            default:
                for (int32_t c = std::max<int32_t>(0, std::min(c1, c2));
                     c <= std::max(c1, c2); ++c)
                    m_cols.push_back(static_cast<uint64_t>(c));
                break;
            }
        }
    }

    if (!m_continuous.empty()) m_mode = SelectMode::Continuous;
    else if (!m_rects.empty()) m_mode = SelectMode::Rect;
    else if (!m_rows.empty())  m_mode = SelectMode::Row;
    else if (!m_cols.empty())  m_mode = SelectMode::Col;
    else m_mode = SelectMode::None;
    return true;
}

std::wstring SelectionSet::Serialize() const {
    std::wostringstream os;
    os << L"BES-SELECTION 1.1\r\n";
    os << L"Mode=" << static_cast<int>(m_mode) << L"\r\n";
    for (const ContinuousSel& s : m_continuous)
        os << L"Continuous=" << s.startBit << L"," << s.endBit << L"\r\n";
    os << L"RectCount=" << m_rects.size() << L"\r\n";
    for (const RectSel& r : m_rects) {
        os << L"Rect=" << r.frame0 << L"," << r.cell0 << L","
           << r.frame1 << L"," << r.cell1 << L"\r\n";
    }
    os << L"RowCount=" << m_rows.size() << L"\r\n";
    for (uint64_t v : m_rows) os << L"Row=" << v << L"\r\n";
    os << L"ColCount=" << m_cols.size() << L"\r\n";
    for (uint64_t v : m_cols) os << L"Col=" << v << L"\r\n";
    return os.str();
}

bool SelectionSet::Deserialize(const std::wstring& text, std::wstring* err) {
    Clear();
    std::wistringstream is(text);
    std::wstring line, header;
    if (!std::getline(is, header)) {
        if (err) *err = L"不是选取参数文件！";
        return false;
    }
    while (!header.empty() && (header.back() == L'\r' || header.back() == L'\n'))
        header.pop_back();
    if (header.rfind(L"BES-SELECTION", 0) != 0) {
        if (err) *err = L"不是选取参数文件！";
        return false;
    }
    const bool oldFormat = (header.find(L"1.1") == std::wstring::npos);

    auto split = [](const std::wstring& s, std::vector<uint64_t>& out) {
        std::wstring cur;
        for (wchar_t c : s) {
            if (c == L',') { out.push_back(_wcstoui64(cur.c_str(), nullptr, 10)); cur.clear(); }
            else cur.push_back(c);
        }
        if (!cur.empty()) out.push_back(_wcstoui64(cur.c_str(), nullptr, 10));
    };

    while (std::getline(is, line)) {
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            line.pop_back();
        if (line.empty()) continue;
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = line.substr(0, eq);
        std::wstring val = line.substr(eq + 1);
        if (key == L"Mode") {
            m_mode = static_cast<SelectMode>(_wtoi(val.c_str()));
        } else if (key == L"Continuous") {
            std::vector<uint64_t> v; split(val, v);
            if (v.size() == 2)
                AddContinuous(v[0], v[1]);
        } else if (key == L"Rect") {
            std::vector<uint64_t> v; split(val, v);
            if (v.size() == 4) {
                if (oldFormat) {
                    // 1.0 的 Rect 是比特范围：退化为连续选区
                    AddContinuous(std::min(v[0], v[2]), std::max(v[1], v[3]));
                } else {
                    RectSel r;
                    r.frame0 = std::min(v[0], v[2]);
                    r.cell0  = std::min(v[1], v[3]);
                    r.frame1 = std::max(v[0], v[2]);
                    r.cell1  = std::max(v[1], v[3]);
                    m_rects.push_back(r);
                }
            }
        } else if (key == L"Row") {
            m_rows.push_back(_wcstoui64(val.c_str(), nullptr, 10));
        } else if (key == L"Col") {
            m_cols.push_back(_wcstoui64(val.c_str(), nullptr, 10));
        }
    }
    return true;
}

} // namespace bes
