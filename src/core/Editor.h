// Editor.h —— 字节编辑与撤销/重做（对应原版引擎的编辑接口）
//
// 原版工具栏「进行编辑」（EditBtn，bsChecked）在 EditBtnClick@0x40B724 里只切换引擎
// 编辑态（editIface vtbl[+0x24]），进入时不做任何前置校验；退出时若后台任务在跑则拒绝，
// 若脏标志 [engine+0x1a4] 置位则弹「文件内容已改变，保存吗？」(Yes/No/Cancel)。
// 剪切/复制/粘贴/撤销/重做分别对应 editIface vtbl[+0x28..+0x38]，回写按 500000 字节分块。
// 这里用纯 C++ 实现同一层语义，供 Qt 与 MFC 两个前端共用。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/FileMap.h"

namespace bes {

class BinaryEditor {
public:
    explicit BinaryEditor(FileMap& map) : m_map(map) {}

    // 改写（覆盖）：越界部分丢弃。映射模式与编辑模式都可用。
    bool Overwrite(uint64_t byteOff, const uint8_t* p, size_t n);
    // 插入/删除会移动后续字节，只在编辑模式（可增长内存副本）下可用
    bool Insert(uint64_t byteOff, const uint8_t* p, size_t n);
    bool Remove(uint64_t byteOff, size_t n);

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    bool Undo();
    bool Redo();

    bool Dirty() const { return m_dirty; }
    bool Save(std::wstring* err = nullptr);
    // 丢弃全部未保存改动，回到磁盘内容
    bool Discard(std::wstring* err = nullptr);
    // 清空撤销/重做栈与脏标志（换文件时用，不动数据）
    void Reset() { m_undo.clear(); m_redo.clear(); m_dirty = false; }
    size_t UndoDepth() const { return m_undo.size(); }

private:
    struct Op {
        enum Kind { Overwrite, Insert, Delete } kind = Overwrite;
        uint64_t           byteOff = 0;
        std::vector<uint8_t> before;  // 改写=改前内容，删除=被删内容
        std::vector<uint8_t> after;   // 改写=改后内容，插入=插入内容
    };

    bool Inverse(const Op& op, std::wstring* err = nullptr);
    bool Forward(const Op& op, std::wstring* err = nullptr);

    FileMap&          m_map;
    std::vector<Op>   m_undo;
    std::vector<Op>   m_redo;
    bool              m_dirty = false;
};

} // namespace bes
