// Bookmark.h —— 书签（标记）
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bes {

// 原程序「书签」：可设置/清除/下一个/上一个，附带名称。
// 书签位置用绝对比特地址表示。
class BookmarkSet {
public:
    void Clear() { m_marks.clear(); }
    void Set(uint64_t bitPos, const std::wstring& name = L"");
    void RemoveAt(uint64_t bitPos);
    bool Has(uint64_t bitPos) const { return m_marks.find(bitPos) != m_marks.end(); }
    size_t Size() const { return m_marks.size(); }

    // 下一个 / 上一个（按位置排序；wrap around）
    bool Next(uint64_t from, uint64_t& out) const;
    bool Prev(uint64_t from, uint64_t& out) const;

    const std::map<uint64_t, std::wstring>& All() const { return m_marks; }

    std::wstring Serialize() const;
    bool Deserialize(const std::wstring& text, std::wstring* err = nullptr);

private:
    std::map<uint64_t, std::wstring> m_marks;
};

} // namespace bes
