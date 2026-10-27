// Bookmark.cpp
#include "core/Bookmark.h"
#include <sstream>

namespace bes {

void BookmarkSet::Set(uint64_t bitPos, const std::wstring& name) {
    m_marks[bitPos] = name;
}

void BookmarkSet::RemoveAt(uint64_t bitPos) {
    m_marks.erase(bitPos);
}

bool BookmarkSet::Next(uint64_t from, uint64_t& out) const {
    if (m_marks.empty()) return false;
    auto it = m_marks.upper_bound(from);
    if (it == m_marks.end()) it = m_marks.begin();
    out = it->first;
    return true;
}

bool BookmarkSet::Prev(uint64_t from, uint64_t& out) const {
    if (m_marks.empty()) return false;
    auto it = m_marks.lower_bound(from);
    if (it == m_marks.begin()) it = m_marks.end();
    --it;
    out = it->first;
    return true;
}

std::wstring BookmarkSet::Serialize() const {
    std::wostringstream os;
    os << L"BES-BOOKMARK 1.0\r\n";
    os << L"Count=" << m_marks.size() << L"\r\n";
    for (const auto& kv : m_marks)
        os << L"Mark=" << kv.first << L"\t" << kv.second << L"\r\n";
    return os.str();
}

bool BookmarkSet::Deserialize(const std::wstring& text, std::wstring* err) {
    Clear();
    std::wistringstream is(text);
    std::wstring line, header;
    if (!std::getline(is, header)) {
        if (err) *err = L"不是书签文件！";
        return false;
    }
    while (!header.empty() && (header.back() == L'\r' || header.back() == L'\n'))
        header.pop_back();
    if (header.rfind(L"BES-BOOKMARK", 0) != 0) {
        if (err) *err = L"不是书签文件！";
        return false;
    }
    while (std::getline(is, line)) {
        while (!line.empty() && (line.back() == L'\r' || line.back() == L'\n'))
            line.pop_back();
        if (line.rfind(L"Mark=", 0) != 0) continue;
        std::wstring rest = line.substr(5);
        size_t tab = rest.find(L'\t');
        uint64_t pos = _wcstoui64(rest.c_str(), nullptr, 10);
        std::wstring name = (tab == std::wstring::npos) ? L"" : rest.substr(tab + 1);
        m_marks[pos] = name;
    }
    return true;
}

} // namespace bes
