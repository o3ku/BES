// ComponentHost.cpp —— 仅 64 位构件宿主
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "plugin/ComponentHost.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace bes {

namespace {

std::wstring AnsiToWide(const char* s) {
    if (!s || !*s) return std::wstring();
    const int n = ::MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_ACP, 0, s, -1, &out[0], n);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}

std::string WideToAnsi(const std::wstring& s) {
    if (s.empty()) return std::string();
    const int n = ::WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string out(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, &out[0], n, nullptr, nullptr);
    out.resize(static_cast<size_t>(n - 1));
    return out;
}

// 实例信息结构内的定长字符字段（可能不带 NUL）
std::wstring AnsiField(const std::string& blob, size_t off, size_t len) {
    const char* s = blob.data() + off;
    size_t n = 0;
    while (n < len && s[n] != '\0') ++n;
    return AnsiToWide(std::string(s, n).c_str());
}

int32_t ReadI32(const std::string& blob, size_t off) {
    int32_t v = 0;
    std::memcpy(&v, blob.data() + off, sizeof(v));
    return v;
}

// 解析实例上报的注册信息（布局见 ComponentHost.h / InstanceInfo）
bool ParseInfoBlob(const std::string& blob, InstanceInfo* out) {
    if (blob.size() < 0x58) return false;
    InstanceInfo info;
    info.author  = AnsiField(blob, 0x00, 20);
    info.version = AnsiField(blob, 0x14, 10);
    info.desc    = AnsiField(blob, 0x1E, 50);
    info.comType = ReadI32(blob, 0x50);
    const int32_t count = ReadI32(blob, 0x54);
    if (count < 0) return false;
    // 原版 OKBtnClick：size < 0x58 + count*0x3C → 「构件注册信息不正确！」
    if (blob.size() < 0x58 + static_cast<size_t>(count) * 0x3C) return false;
    for (int32_t i = 0; i < count; ++i) {
        const size_t o = 0x58 + static_cast<size_t>(i) * 0x3C;
        PortInfo p;
        p.index        = ReadI32(blob, o + 0x28);
        p.rawDataType  = ReadI32(blob, o + 0x2C);
        p.maxCount     = ReadI32(blob, o + 0x30);
        p.defaultCount = ReadI32(blob, o + 0x34);
        const int32_t kind = ReadI32(blob, o + 0x38);
        p.kind = (kind >= 1 && kind <= 4)
                     ? static_cast<PortKind>(kind - 1) : PortKind::DataIn;
        p.dataType = (p.rawDataType >= 0 && p.rawDataType <= 13)
                         ? static_cast<DataType>(p.rawDataType) : DataType::Unknown;
        p.variableCount = p.maxCount != 1;
        info.ports.push_back(p);
    }
    if (out) *out = std::move(info);
    return true;
}

// 读取实例信息：info(inst, nullptr, 0, &size) → info(inst, buf, size, &size)。
// vtable 索引 18（32 位字节偏移 0x48）在 64 位下为 0x90，按索引取即可。
// 构件是外部代码，越界/坏指针只可能以 SEH 异常形式暴露，故整段受 __try 保护。
bool ReadBlobSEH(void* inst, std::string* blob) {
    if (!inst) return false;
    typedef DWORD (WINAPI* InfoFn)(void*, void*, DWORD, DWORD*);
    void** vt = *reinterpret_cast<void***>(inst);
    InfoFn info = reinterpret_cast<InfoFn>(vt[18]);
    if (!info) return false;
    DWORD size = 0;
    bool ok = false;
    __try {
        if (info(inst, nullptr, 0, &size) == 0 && size >= 0x58 && size <= 0x100000) {
            void* buf = ::LocalAlloc(LPTR, size);
            if (buf) {
                if (info(inst, buf, size, &size) == 0) {
                    blob->assign(static_cast<const char*>(buf), size);
                    ok = true;
                }
                ::LocalFree(buf);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    return ok;
}

// vtable 索引 1（32 位 0x04 / 64 位 0x08）：释放实例
void ReleaseInstanceSEH(void* inst) {
    if (!inst) return;
    typedef void (WINAPI* RelFn)(void*);
    void** vt = *reinterpret_cast<void***>(inst);
    RelFn rel = reinterpret_cast<RelFn>(vt[1]);
    if (!rel) return;
    __try {
        rel(inst);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH * 4] = { 0 };
    const DWORD n = ::GetModuleFileNameW(nullptr, path, MAX_PATH * 4 - 1);
    if (n == 0 || n >= MAX_PATH * 4 - 1) return std::wstring();
    std::wstring full(path, n);
    const size_t sep = full.find_last_of(L"\\/");
    return sep == std::wstring::npos ? std::wstring() : full.substr(0, sep);
}

std::wstring FileLeaf(const std::wstring& path) {
    const size_t sep = path.find_last_of(L"\\/");
    return sep == std::wstring::npos ? path : path.substr(sep + 1);
}

// 原版 TComCustomForm.BrowseBtnClick 的探测流程：LoadLibrary → 取三个导出 →
// RegComNum → 逐个 RegComInfo（1 基），探测完即 FreeLibrary，不登记到宿主。
// RegComInfo 的两个缓冲区在原版栈上分别占 0x6C / 0x64 字节，这里同样预留 128。
bool ProbeAbi(const std::wstring& path, std::vector<ComponentInfo>* out,
              std::wstring* err) {
    if (out) out->clear();
    if (ComponentHost::Is32BitDll(path)) {
        if (err) *err = L"本版本仅支持 64 位构件。\n"
                         L"该 DLL 是 32 位构件，32 位支持已取消，请使用 64 位构件版本。";
        return false;
    }
    HMODULE h = ::LoadLibraryW(path.c_str());
    if (!h) {
        // 原版：LoadLibrary 失败与任一导出缺失报同一句
        if (err) *err = L"不是局级系统构件！";
        return false;
    }
    auto num  = reinterpret_cast<DWORD (WINAPI*)(int*)>(::GetProcAddress(h, "RegComNum"));
    auto info = reinterpret_cast<DWORD (WINAPI*)(DWORD, char*, char*, int*)>(
                    ::GetProcAddress(h, "RegComInfo"));
    auto inst = reinterpret_cast<DWORD (WINAPI*)(const char*, void**, void*, void*)>(
                    ::GetProcAddress(h, "CreateInstance"));
    if (!num || !info || !inst) {
        if (err) *err = L"不是局级系统构件！";
        ::FreeLibrary(h);
        return false;
    }
    int count = 0;
    if (num(&count) != 0 || count <= 0) {
        if (err) *err = L"没有可用构件！";
        ::FreeLibrary(h);
        return false;
    }
    std::vector<ComponentInfo> list;
    for (int i = 1; i <= count; ++i) {
        // bufA = 注册名（CreateInstance 用），bufB = 界面显示名
        char regName[128] = { 0 };
        char display[128] = { 0 };
        int  reserved = 0;
        if (info(static_cast<DWORD>(i), regName, display, &reserved) != 0) continue;
        if (regName[0] == '\0' && display[0] == '\0') continue;
        ComponentInfo ci;
        ci.dllPath = path;
        ci.regNameAnsi = regName;
        ci.name = AnsiToWide(display[0] ? display : regName);
        list.push_back(ci);
    }
    ::FreeLibrary(h);
    if (list.empty()) {
        if (err) *err = L"没有可用构件！";
        return false;
    }
    if (out) *out = list;
    return true;
}

} // namespace

ComponentHost::~ComponentHost() {
    m_instances.clear();
    for (Loaded* l : m_loaded) {
        if (l->module) ::FreeLibrary(l->module);
        delete l;
    }
    m_loaded.clear();
}

bool ComponentHost::Is32BitDll(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    IMAGE_DOS_HEADER dos{};
    f.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!f || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

    f.seekg(dos.e_lfanew, std::ios::beg);
    DWORD sig = 0;
    f.read(reinterpret_cast<char*>(&sig), sizeof(sig));
    if (!f || sig != IMAGE_NT_SIGNATURE) return false;

    IMAGE_FILE_HEADER fh{};
    f.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    if (!f) return false;
    return fh.Machine == IMAGE_FILE_MACHINE_I386;
}

ComponentHost::Loaded* ComponentHost::FindLoaded(const std::wstring& path) const {
    for (Loaded* l : m_loaded)
        if (_wcsicmp(l->path.c_str(), path.c_str()) == 0)
            return l;
    return nullptr;
}

bool ComponentHost::RegisterDll(const std::wstring& dllPath, std::wstring* err) {
    if (Is32BitDll(dllPath)) {
        if (err) *err = L"本版本仅支持 64 位构件。\n"
                         L"该 DLL 是 32 位构件，32 位支持已取消，请使用 64 位构件版本。";
        return false;
    }

    Loaded* l = FindLoaded(dllPath);
    if (!l) {
        HMODULE h = ::LoadLibraryW(dllPath.c_str());
        if (!h) {
            const DWORD e = ::GetLastError();
            if (err) {
                if (e == ERROR_BAD_EXE_FORMAT)
                    *err = L"构件 DLL 与宿主位数不匹配（仅支持 64 位构件）。";
                else
                    *err = L"构件创建失败！（错误码 " + std::to_wstring(e) + L"）";
            }
            return false;
        }
        l = new Loaded();
        l->path = dllPath;
        l->module = h;
        l->RegComNum  = reinterpret_cast<DWORD (WINAPI*)(int*)>(
                            ::GetProcAddress(h, "RegComNum"));
        l->RegComInfo = reinterpret_cast<DWORD (WINAPI*)(DWORD, char*, char*, int*)>(
                            ::GetProcAddress(h, "RegComInfo"));
        l->CreateInstance = reinterpret_cast<DWORD (WINAPI*)(const char*, void**, void*, void*)>(
                            ::GetProcAddress(h, "CreateInstance"));
        l->ShowModal = reinterpret_cast<DWORD (WINAPI*)(const char*)>(
                            ::GetProcAddress(h, "ShowModal"));
        m_loaded.push_back(l);
    }

    if (!l->RegComNum || !l->RegComInfo) {
        if (err) *err = L"构件注册信息不正确！";
        return false;
    }

    int count = 0;
    if (l->RegComNum(&count) != 0 || count <= 0) {
        if (err) *err = L"构件注册信息不正确！";
        return false;
    }

    // 原版 ABI 的 RegComInfo 以 1 为起始索引
    int added = 0;
    for (int i = 1; i <= count; ++i) {
        char regName[128] = { 0 };
        char display[128] = { 0 };
        int  reserved = 0;
        if (l->RegComInfo(static_cast<DWORD>(i), regName, display, &reserved) != 0)
            continue;
        if (regName[0] == '\0' && display[0] == '\0')
            continue;

        ComponentInfo ci;
        ci.dllPath = dllPath;
        ci.regNameAnsi = regName;
        ci.name = AnsiToWide(display[0] ? display : regName);

        // 重复注册同一构件时覆盖原记录（DLL 重新探测后端口等以新结果为准）
        bool replaced = false;
        for (ComponentInfo& old : m_components) {
            if (old.name == ci.name) {
                old = ci;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            m_components.push_back(ci);
        ++added;
    }

    if (added == 0) {
        if (err) *err = L"构件注册信息不正确（未返回构件信息）！";
        return false;
    }
    return true;
}

bool ComponentHost::UnregisterByName(const std::wstring& name) {
    for (size_t i = 0; i < m_components.size(); ++i) {
        if (m_components[i].name == name) {
            m_components.erase(m_components.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false;
}

ComponentInfo* ComponentHost::FindMutableByName(const std::wstring& name) {
    for (ComponentInfo& c : m_components)
        if (c.name == name)
            return &c;
    return nullptr;
}

const ComponentInfo* ComponentHost::FindByName(const std::wstring& name) const {
    for (const ComponentInfo& c : m_components)
        if (c.name == name)
            return &c;
    return nullptr;
}

bool ComponentHost::HasComponentAbi(const std::wstring& dllPath) const {
    const Loaded* l = FindLoaded(dllPath);
    return l && l->RegComNum && l->RegComInfo && l->CreateInstance;
}

bool ComponentHost::PeekAbi(const std::wstring& path,
                            std::vector<ComponentInfo>* out, std::wstring* err) {
    return ProbeAbi(path, out, err);
}

ComponentHost::InstanceInfoStatus ComponentHost::QueryInstanceInfo(
    const std::wstring& dllPath, const std::string& regNameAnsi,
    InstanceInfo* out, std::wstring* err) {
    HMODULE h = ::LoadLibraryW(dllPath.c_str());
    auto create = h ? reinterpret_cast<DWORD (WINAPI*)(const char*, void**, void*, void*)>(
                          ::GetProcAddress(h, "CreateInstance"))
                    : nullptr;
    if (!create) {
        if (h) ::FreeLibrary(h);
        if (err) *err = L"构件创建失败！";
        return InstanceInfoStatus::CreateFailed;
    }

    // 原版只检查 CreateInstance 的输出指针，返回值不参与判断
    void* inst = nullptr;
    create(regNameAnsi.c_str(), &inst, nullptr, nullptr);
    if (!inst) {
        ::FreeLibrary(h);
        if (err) *err = L"构件创建失败！";
        return InstanceInfoStatus::CreateFailed;
    }

    std::string blob;
    const bool read = ReadBlobSEH(inst, &blob);
    ReleaseInstanceSEH(inst);
    ::FreeLibrary(h);

    InstanceInfo info;
    if (!read || !ParseInfoBlob(blob, &info)) {
        if (err) *err = L"构件注册信息不正确！";
        return InstanceInfoStatus::BadInfo;
    }
    if (out) *out = std::move(info);
    return InstanceInfoStatus::Ok;
}

std::wstring ComponentHost::ComDir() {
    const std::wstring dir = ExeDir();
    if (dir.empty()) return std::wstring(L"com");
    return dir + L"\\com";
}

bool ComponentHost::RegisterDllCopied(const std::wstring& srcPath,
                                      std::wstring* copiedPath, std::wstring* err) {
    const std::wstring dir = ComDir();
    std::wstring dst = dir + L"\\" + FileLeaf(srcPath);
    if (!_wcsicmp(dst.c_str(), srcPath.c_str())) {
        if (copiedPath) *copiedPath = dst;
        return RegisterDll(srcPath, err);
    }
    ::CreateDirectoryW(dir.c_str(), nullptr);
    // 原版 AddBtnClick：CopyFile(源, com\文件名, FALSE)
    if (!::CopyFileW(srcPath.c_str(), dst.c_str(), FALSE)) {
        if (err) *err = L"复制构件文件失败！（错误码 " +
                         std::to_wstring(::GetLastError()) + L"）";
        return false;
    }
    if (copiedPath) *copiedPath = dst;
    return RegisterDll(dst, err);
}

std::vector<std::wstring> ComponentHost::DllPaths() const {
    std::vector<std::wstring> out;
    for (const ComponentInfo& c : m_components) {
        if (std::find(out.begin(), out.end(), c.dllPath) == out.end())
            out.push_back(c.dllPath);
    }
    return out;
}

bool ComponentHost::CreateInstance(const std::wstring& name, ComponentInstance* out,
                                   void* p2, void* p3, std::wstring* err) {
    const ComponentInfo* ci = FindByName(name);
    if (!ci) {
        if (err) *err = L"没有可用构件！";
        return false;
    }
    Loaded* l = FindLoaded(ci->dllPath);
    if (!l || !l->CreateInstance) {
        if (err) *err = L"构件创建失败！";
        return false;
    }
    std::string regName = ci->regNameAnsi;
    if (regName.empty())
        regName = WideToAnsi(ci->name);

    void* inst = nullptr;
    if (l->CreateInstance(regName.c_str(), &inst, p2, p3) != 0 || !inst) {
        if (err) *err = L"构件创建失败！";
        return false;
    }
    m_instances.push_back(static_cast<ComponentInstance>(inst));
    *out = inst;
    return true;
}

void ComponentHost::DestroyInstance(ComponentInstance inst) {
    for (size_t i = 0; i < m_instances.size(); ++i) {
        if (m_instances[i] == inst) {
            m_instances.erase(m_instances.begin() + static_cast<long>(i));
            return;
        }
    }
}

bool ComponentHost::ShowModal(const std::wstring& name, const std::wstring& file,
                              std::wstring* err) {
    const ComponentInfo* ci = FindByName(name);
    if (!ci) {
        if (err) *err = L"没有可用构件！";
        return false;
    }
    Loaded* l = FindLoaded(ci->dllPath);
    if (!l || !l->ShowModal) {
        if (err) *err = L"该构件未导出 ShowModal，无法打开构件界面。";
        return false;
    }
    const std::string ansi = WideToAnsi(file);
    const DWORD r = l->ShowModal(file.empty() ? nullptr : ansi.c_str());
    if (r != 0) {
        if (err) *err = L"构件返回错误（代码 " + std::to_wstring(r) + L"）。";
        return false;
    }
    return true;
}

} // namespace bes
