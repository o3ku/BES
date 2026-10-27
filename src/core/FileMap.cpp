// FileMap.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "core/FileMap.h"

namespace bes {

FileMap::~FileMap() { Close(); }

void FileMap::Close() {
    // 编辑模式下 m_pBase 指向堆缓冲，不能 UnmapViewOfFile
    if (m_pBase && !m_editable) ::UnmapViewOfFile(m_pBase);
    m_pBase = nullptr;
    if (m_hMap) { ::CloseHandle(m_hMap); m_hMap = nullptr; }
    if (m_hFile) { ::CloseHandle(m_hFile); m_hFile = nullptr; }
    m_sizeBytes = 0;
    m_editable = false;
    m_buf.clear();
    m_path.clear();
}

void FileMap::SetBaseFromBuf() {
    m_pBase = m_buf.empty() ? nullptr : m_buf.data();
    m_sizeBytes = static_cast<uint64_t>(m_buf.size());
}

bool FileMap::OpenEditable(const std::wstring& path, std::wstring* err) {
    // 共享模式必须放行已存在的映射句柄的写权限，否则映射未释放时再打开会 ERROR_SHARING_VIOLATION(32)
    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (err) *err = L"无法打开文件 " + path;
        return false;
    }
    LARGE_INTEGER li{};
    if (!::GetFileSizeEx(hFile, &li)) {
        ::CloseHandle(hFile);
        if (err) *err = L"无法获取文件大小 " + path;
        return false;
    }
    std::vector<uint8_t> buf(static_cast<size_t>(li.QuadPart));
    size_t got = 0;
    while (got < buf.size()) {
        DWORD n = 0;
        if (!::ReadFile(hFile, buf.data() + got,
                        static_cast<DWORD>(buf.size() - got), &n, nullptr) || n == 0)
            break;
        got += n;
    }
    ::CloseHandle(hFile);
    if (got != buf.size()) {
        if (err) *err = L"读取文件失败 " + path;
        return false;
    }
    Close();
    m_editable = true;
    m_buf = std::move(buf);
    m_path = path;
    SetBaseFromBuf();
    return true;
}

bool FileMap::Revert(std::wstring* err) {
    const std::wstring path = m_path;
    Close();
    return Open(path, err);
}

bool FileMap::Reload(std::wstring* err) {
    const std::wstring path = m_path;
    const bool editable = m_editable;
    Close();
    return editable ? OpenEditable(path, err) : Open(path, err);
}

bool FileMap::InsertBytes(uint64_t byteOff, const uint8_t* p, size_t n) {
    if (!m_editable || !p || byteOff > m_sizeBytes)
        return false;
    m_buf.insert(m_buf.begin() + static_cast<ptrdiff_t>(byteOff), p, p + n);
    SetBaseFromBuf();
    return true;
}

bool FileMap::DeleteBytes(uint64_t byteOff, size_t n) {
    if (!m_editable || n == 0 || byteOff > m_sizeBytes || n > m_sizeBytes - byteOff)
        return false;
    m_buf.erase(m_buf.begin() + static_cast<ptrdiff_t>(byteOff),
                m_buf.begin() + static_cast<ptrdiff_t>(byteOff + n));
    SetBaseFromBuf();
    return true;
}

bool FileMap::Open(const std::wstring& path, std::wstring* err) {
    Close();

    HANDLE hFile = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        // 只读重试
        hFile = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            if (err) *err = L"无法打开文件 " + path;
            return false;
        }
    }

    LARGE_INTEGER li{};
    if (!::GetFileSizeEx(hFile, &li)) {
        ::CloseHandle(hFile);
        if (err) *err = L"无法获取文件大小 " + path;
        return false;
    }
    m_sizeBytes = static_cast<uint64_t>(li.QuadPart);

    HANDLE hMap = ::CreateFileMappingW(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!hMap) {
        hMap = ::CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap) {
            ::CloseHandle(hFile);
            if (err) *err = L"无法创建文件映射 " + path;
            return false;
        }
    }

    m_pBase = static_cast<uint8_t*>(::MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 0));
    if (!m_pBase) {
        m_pBase = static_cast<uint8_t*>(::MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    }
    if (!m_pBase) {
        ::CloseHandle(hMap);
        ::CloseHandle(hFile);
        if (err) *err = L"无法映射文件视图 " + path;
        return false;
    }

    m_hMap = hMap;
    m_hFile = hFile;
    m_path = path;
    return true;
}

uint32_t FileMap::ReadBits(uint64_t bitPos, int n) const {
    if (!m_pBase || n <= 0 || n > 32)
        return 0;
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
        uint64_t b = bitPos + static_cast<uint64_t>(i);
        uint64_t byteOff = b >> 3;
        uint32_t bit = static_cast<uint32_t>((b & 7ull) == 0 ? 0 : (b & 7ull));
        // 原程序默认「高位在前」(0-7) 语义：bit7 为最高位
        uint32_t val = 0;
        if (byteOff < m_sizeBytes)
            val = (m_pBase[byteOff] >> (7 - bit)) & 1u;
        v = (v << 1) | val;
    }
    return v;
}

uint8_t FileMap::ReadByteAtBit(uint64_t bitPos) const {
    return static_cast<uint8_t>(ReadBits(bitPos, 8));
}

bool FileMap::ReadByte(uint64_t byteOff, uint8_t& out) const {
    if (!m_pBase || byteOff >= m_sizeBytes)
        return false;
    out = m_pBase[byteOff];
    return true;
}

bool FileMap::WriteByteAtBit(uint64_t bitPos, uint8_t v) {
    if (!m_pBase)
        return false;
    uint64_t byteOff = bitPos >> 3;
    if (byteOff >= m_sizeBytes)
        return false;
    uint32_t shift = static_cast<uint32_t>(bitPos & 7ull);
    uint8_t mask = static_cast<uint8_t>(0xFFu >> shift);
    m_pBase[byteOff] = static_cast<uint8_t>((m_pBase[byteOff] & ~mask) | ((v << shift) & mask));
    return true;
}

bool FileMap::WriteByte(uint64_t byteOff, uint8_t v) {
    if (!m_pBase || byteOff >= m_sizeBytes)
        return false;
    m_pBase[byteOff] = v;
    return true;
}

bool FileMap::Flush() {
    if (m_editable) {
        // 编辑模式：整份写回原文件（只读属性会让 CreateFile 失败 → 调用方提示不可写）
        HANDLE h = ::CreateFileW(m_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        // 原版 SaveModified 按 500000 字节分块回写
        const size_t kBlock = 500000;
        BOOL ok = ::SetFilePointerEx(h, LARGE_INTEGER{}, nullptr, FILE_BEGIN) != FALSE;
        size_t sent = 0;
        while (ok && sent < m_buf.size()) {
            const size_t chunk = m_buf.size() - sent < kBlock ? m_buf.size() - sent : kBlock;
            size_t got = 0;
            while (ok && got < chunk) {
                DWORD n = 0;
                ok = ::WriteFile(h, m_buf.data() + sent + got,
                                 static_cast<DWORD>(chunk - got), &n, nullptr) && n > 0;
                if (ok) got += n;
            }
            if (ok) sent += chunk;
        }
        // 截断尾部（插入/删除后长度可能变短）
        if (ok) {
            LARGE_INTEGER end{};
            end.QuadPart = static_cast<LONGLONG>(sent);
            ok = ::SetFilePointerEx(h, end, nullptr, FILE_BEGIN) && ::SetEndOfFile(h);
        }
        ::CloseHandle(h);
        return ok != FALSE;
    }
    if (!m_pBase)
        return false;
    return ::FlushViewOfFile(m_pBase, 0) != FALSE;
}

} // namespace bes
