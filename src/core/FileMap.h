// FileMap.h —— 内存映射文件访问（对应原程序 TFileMap）
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bes {

// 只读/可写的文件访问（对应原程序 TFileMap）。
// 默认是内存映射文件；进入编辑后改为可增长的内存副本（原版引擎的插入/删除
// 会移动后续字节并改变长度，映射视图做不到这一点）。
// 所有对外寻址均以「比特」为单位（与原程序一致：状态栏显示「第X比特 第Y比特」）。
class FileMap {
public:
    FileMap() = default;
    ~FileMap();
    FileMap(const FileMap&) = delete;
    FileMap& operator=(const FileMap&) = delete;

    bool Open(const std::wstring& path, std::wstring* err = nullptr);
    // 编辑模式：整份读入内存副本，允许插入/删除改变长度
    bool OpenEditable(const std::wstring& path, std::wstring* err = nullptr);
    void Close();
    bool IsOpen() const { return m_editable || m_pBase != nullptr; }
    bool IsEditable() const { return m_editable; }
    // 丢弃内存副本，按磁盘内容重新打开
    bool Revert(std::wstring* err = nullptr);
    // 按当前模式（映射/编辑）重新打开，长度与内容回到磁盘状态
    bool Reload(std::wstring* err = nullptr);

    const std::wstring& Path() const { return m_path; }
    uint64_t SizeBytes() const { return m_sizeBytes; }
    uint64_t SizeBits() const { return m_sizeBytes * 8ull; }

    // 读 n (1..32) 个比特，MSB 优先。越界补 0。
    uint32_t ReadBits(uint64_t bitPos, int n) const;
    // 读一个字节（按比特位置，允许非字节对齐）。越界返回 0。
    uint8_t ReadByteAtBit(uint64_t bitPos) const;
    // 读一个字节（按字节偏移）。越界返回 false。
    bool ReadByte(uint64_t byteOff, uint8_t& out) const;

    bool WriteByteAtBit(uint64_t bitPos, uint8_t v);
    bool WriteByte(uint64_t byteOff, uint8_t v);
    // 以下两个仅在编辑模式（内存副本）下可用
    bool InsertBytes(uint64_t byteOff, const uint8_t* p, size_t n);
    bool DeleteBytes(uint64_t byteOff, size_t n);
    // 编辑模式下把内存副本写回原文件；映射模式下刷新视图
    bool Flush();

    // 原始指针（渲染器批量取数用）
    const uint8_t* Base() const { return m_pBase; }

private:
    void SetBaseFromBuf();

    std::wstring m_path;
    uint8_t*     m_pBase = nullptr;
    uint64_t     m_sizeBytes = 0;
    void*        m_hMap = nullptr;
    void*        m_hFile = nullptr;
    bool                    m_editable = false;
    std::vector<uint8_t>    m_buf;
};

} // namespace bes
