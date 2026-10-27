// ComponentHost.h —— 「构件」插件宿主（仅 64 位）
//
// 兼容原程序 BesCom.dll 暴露的构件 ABI（「分析应用层接口版本 2.0」）：
//   DWORD __stdcall RegComNum(int* n);
//   DWORD __stdcall RegComInfo(DWORD idx, char regName[], char displayName[],
//                              int* reserved);                          // idx 从 1 开始
//   DWORD __stdcall CreateInstance(const char* regName, void** out,
//                                  void* p2, void* p3);
//   DWORD __stdcall ShowModal(const char* file);
//
// 反汇编 TComSelectForm.OKBtnClick（0x417E80）证实：RegComInfo 的第一个缓冲区
// 是注册名（传给 CreateInstance），第二个才是界面显示名；第 4 个参数写入后
// 从未被读取，是遗留参数。两个缓冲区在原版栈上分别为 0x6C / 0x64 字节。
//
// 构件的作者/版本/说明/类型/端口来自 CreateInstance 返回实例的 vtable：
//   info   = vtable 字节偏移 +0x48（索引 18）→ 读取内联定长结构
//   release= vtable 字节偏移 +0x04（索引 1）
// 32 位下索引 18 即 0x48，64 位需按指针宽度换算为 0x90；这里按索引取。
//
// 名称/描述为 ANSI(GBK)。本版本只支持 64 位构件；32 位 DLL 会被明确拒绝。
#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/DataTypes.h"

namespace bes {

// 一个物理端口的描述（偏移相对端口记录起点，见 InstanceInfo）
struct PortInfo {
    int         index = 0;         // 端口 ID                    (+0x28)
    PortKind    kind = PortKind::DataIn;                  // +0x38（1 基）
    DataType    dataType = DataType::Unknown;             // +0x2C 映射后的枚举
    int         rawDataType = 0;   // +0x2C 原始数值；0xFF = LSAS_TYPE_VOID
    int         defaultCount = 0;  // 缺省端口数量              (+0x34)
    int         maxCount = 0;      // 最大端口数量              (+0x30)
    bool        variableCount = false; // 数量可变（maxCount != 1）
};

// 构件实例上报的注册信息（CreateInstance 后经实例 vtable 索引 18 读取）。
// 结构内全部为定长字符数组/整数，与指针对齐无关：
//   +0x00 作者[20]  +0x14 版本[10]  +0x1E 简要说明[50]
//   +0x50 构件类型（1 基）  +0x54 端口数
//   +0x58 起端口记录，步长 0x3C
struct InstanceInfo {
    std::wstring author;
    std::wstring version;
    std::wstring desc;
    int comType = 0;               // 1 基 1..4
    std::vector<PortInfo> ports;
};

// 一个构件（插件）的注册信息
struct ComponentInfo {
    std::wstring dllPath;
    std::wstring name;                 // RegComInfo 的显示名（第二个缓冲区）
    std::wstring desc;                 // 实例信息中的简要说明
    std::wstring author;               // 实例信息中的作者
    std::wstring version;              // 实例信息中的版本
    std::string  regNameAnsi;          // RegComInfo 的注册名（第一个缓冲区）
    ComponentKind kind = ComponentKind::Normal;
    int  physicalPortCount = 0;
    int  comType = 0;                  // 1 基（1 普通 / 2 数据源 / 3 通用显示 / 4 探头）
    std::vector<PortInfo> ports;
    std::vector<std::pair<std::wstring, std::wstring>> extra;
};

using ComponentInstance = void*;

class ComponentHost {
public:
    ComponentHost() = default;
    ~ComponentHost();
    ComponentHost(const ComponentHost&) = delete;
    ComponentHost& operator=(const ComponentHost&) = delete;

    // 注册一个 64 位构件 DLL（加载并调用其 RegComNum/RegComInfo）
    bool RegisterDll(const std::wstring& dllPath, std::wstring* err = nullptr);
    bool UnregisterByName(const std::wstring& name);

    const std::vector<ComponentInfo>& Components() const { return m_components; }
    std::vector<ComponentInfo>& ComponentsMutable() { return m_components; }
    ComponentInfo* FindMutableByName(const std::wstring& name);

    size_t Count() const { return m_components.size(); }
    const ComponentInfo* FindByName(const std::wstring& name) const;

    // 已注册 DLL 列表（用于持久化）
    std::vector<std::wstring> DllPaths() const;

    // 检测 DLL 是否为 32 位（用于给出明确拒绝信息）
    static bool Is32BitDll(const std::wstring& path);

    // DLL 是否具备完整构件 ABI（RegComNum/RegComInfo/CreateInstance）。
    // 原版快速执行在任一导出缺失时报「不是局级系统构件！」。
    // 需先 RegisterDll（未加载时返回 false）。
    bool HasComponentAbi(const std::wstring& dllPath) const;

    // 探测文件是否为「局级系统构件」：需同时导出 RegComNum/RegComInfo/
    // CreateInstance，且 RegComNum 返回正数（原版 TComCustomForm.BrowseBtnClick）。
    // 缺导出报「不是局级系统构件！」，导出齐全但没有构件报「没有可用构件！」。
    // out 非空时返回按 RegComInfo 枚举的构件信息（不登记到宿主）。
    static bool PeekAbi(const std::wstring& path,
                        std::vector<ComponentInfo>* out = nullptr,
                        std::wstring* err = nullptr);

    // 创建一次临时实例并读取其上报的注册信息（原版 ComInfoText /
    // TComSelectForm.OKBtnClick 的 CreateInstance + info + release 流程）。
    // 不登记到宿主，返回后实例已释放。
    enum class InstanceInfoStatus {
        Ok = 0,
        CreateFailed,   // CreateInstance 失败/缺导出 → 「构件创建失败！」
        BadInfo         // 结构尺寸/端口数不合法 → 「构件注册信息不正确！」
    };
    static InstanceInfoStatus QueryInstanceInfo(const std::wstring& dllPath,
                                                const std::string& regNameAnsi,
                                                InstanceInfo* out,
                                                std::wstring* err = nullptr);

    // 构件目录：<宿主 exe 目录>\com（原版用 GetModuleFileName + 'com\' 拼出，
    // 「添加」把选中的构件 DLL 复制进去）
    static std::wstring ComDir();

    // 原版「添加」流程：复制到 com\ 后注册副本；返回实际注册用的路径。
    bool RegisterDllCopied(const std::wstring& srcPath,
                           std::wstring* copiedPath = nullptr,
                           std::wstring* err = nullptr);

    // 创建实例。p2/p3 原程序用于传入宿主上下文，可为空。
    bool CreateInstance(const std::wstring& name, ComponentInstance* out,
                        void* p2 = nullptr, void* p3 = nullptr,
                        std::wstring* err = nullptr);
    void DestroyInstance(ComponentInstance inst);

    // 调用构件的 ShowModal(const char* file)；file 为空时传空指针
    bool ShowModal(const std::wstring& name, const std::wstring& file,
                   std::wstring* err = nullptr);

    // 构件选项：缓存大小(M字节) / 块大小(K字节) / 临时文件目录
    struct Options {
        int         cacheMB = 16;
        int         blockKB = 64;
        std::wstring tempDir;
    };
    Options& Opts() { return m_opts; }
    const Options& Opts() const { return m_opts; }

private:
    struct Loaded {
        std::wstring path;
        HMODULE module = nullptr;
        DWORD (WINAPI* RegComNum)(int*) = nullptr;
        DWORD (WINAPI* RegComInfo)(DWORD, char*, char*, int*) = nullptr;
        DWORD (WINAPI* CreateInstance)(const char*, void**, void*, void*) = nullptr;
        DWORD (WINAPI* ShowModal)(const char*) = nullptr;
    };
    Loaded* FindLoaded(const std::wstring& path) const;

    std::vector<ComponentInfo>     m_components;
    std::vector<Loaded*>           m_loaded;
    std::vector<ComponentInstance> m_instances;
    Options                        m_opts;
};

} // namespace bes
