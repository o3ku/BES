// Editor.cpp
#include "core/Editor.h"

namespace bes {

bool BinaryEditor::Overwrite(uint64_t byteOff, const uint8_t* p, size_t n) {
    if (!m_map.IsOpen() || !p || n == 0)
        return false;
    const uint64_t size = m_map.SizeBytes();
    if (byteOff >= size)
        return false;
    if (n > size - byteOff)
        n = static_cast<size_t>(size - byteOff);

    Op op;
    op.kind   = Op::Overwrite;
    op.byteOff = byteOff;
    op.before.resize(n);
    op.after.assign(p, p + n);
    for (size_t i = 0; i < n; ++i)
        m_map.ReadByte(byteOff + i, op.before[i]);
    for (size_t i = 0; i < n; ++i)
        if (!m_map.WriteByte(byteOff + i, op.after[i])) return false;

    m_undo.push_back(std::move(op));
    m_redo.clear();
    m_dirty = true;
    return true;
}

bool BinaryEditor::Insert(uint64_t byteOff, const uint8_t* p, size_t n) {
    if (!m_map.IsEditable() || !p || n == 0)
        return false;
    if (byteOff > m_map.SizeBytes())
        return false;
    if (!m_map.InsertBytes(byteOff, p, n))
        return false;

    Op op;
    op.kind   = Op::Insert;
    op.byteOff = byteOff;
    op.after.assign(p, p + n);
    m_undo.push_back(std::move(op));
    m_redo.clear();
    m_dirty = true;
    return true;
}

bool BinaryEditor::Remove(uint64_t byteOff, size_t n) {
    if (!m_map.IsEditable() || n == 0)
        return false;
    const uint64_t size = m_map.SizeBytes();
    if (byteOff > size || n > size - byteOff)
        return false;

    Op op;
    op.kind   = Op::Delete;
    op.byteOff = byteOff;
    op.before.resize(n);
    for (size_t i = 0; i < n; ++i)
        m_map.ReadByte(byteOff + i, op.before[i]);
    if (!m_map.DeleteBytes(byteOff, n))
        return false;

    m_undo.push_back(std::move(op));
    m_redo.clear();
    m_dirty = true;
    return true;
}

bool BinaryEditor::Forward(const Op& op, std::wstring* err) {
    switch (op.kind) {
    case Op::Overwrite:
        for (size_t i = 0; i < op.after.size(); ++i)
            if (!m_map.WriteByte(op.byteOff + i, op.after[i])) {
                if (err) *err = L"写入位置超出文件长度！";
                return false;
            }
        return true;
    case Op::Insert:
        return m_map.InsertBytes(op.byteOff, op.after.data(), op.after.size());
    default:
        return m_map.DeleteBytes(op.byteOff, op.before.size());
    }
}

bool BinaryEditor::Inverse(const Op& op, std::wstring* err) {
    switch (op.kind) {
    case Op::Overwrite:
        for (size_t i = 0; i < op.before.size(); ++i)
            if (!m_map.WriteByte(op.byteOff + i, op.before[i])) {
                if (err) *err = L"写入位置超出文件长度！";
                return false;
            }
        return true;
    case Op::Insert:
        return m_map.DeleteBytes(op.byteOff, op.after.size());
    default:
        return m_map.InsertBytes(op.byteOff, op.before.data(), op.before.size());
    }
}

bool BinaryEditor::Undo() {
    if (m_undo.empty())
        return false;
    Op op = m_undo.back();
    m_undo.pop_back();
    if (!Inverse(op)) {
        m_undo.push_back(std::move(op));
        return false;
    }
    m_redo.push_back(std::move(op));
    m_dirty = true;
    return true;
}

bool BinaryEditor::Redo() {
    if (m_redo.empty())
        return false;
    Op op = m_redo.back();
    m_redo.pop_back();
    if (!Forward(op)) {
        m_redo.push_back(std::move(op));
        return false;
    }
    m_undo.push_back(std::move(op));
    m_dirty = true;
    return true;
}

bool BinaryEditor::Save(std::wstring* err) {
    if (!m_map.Flush()) {
        if (err) *err = L"文件不可写！";
        return false;
    }
    m_dirty = false;
    return true;
}

bool BinaryEditor::Discard(std::wstring* err) {
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    // 保持当前打开方式：编辑态下丢弃后仍需可增长缓冲
    return m_map.Reload(err);
}

} // namespace bes
