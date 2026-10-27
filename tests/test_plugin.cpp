// test_plugin.cpp —— 构件 ABI 测试用 DLL（64 位，供 test_ui 使用）
//
// 模拟原版 BesCom.dll 的构件约定：
//   RegComInfo 的第一个缓冲区是注册名（CreateInstance 用），第二个是显示名；
//   CreateInstance 返回的对象 vtable 索引 1 为释放、索引 18 为上报注册信息。
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <string>

namespace {

#ifndef TEST_PLUGIN_NAME
#define TEST_PLUGIN_NAME "TEST.PLUGIN.1"
#endif
#ifndef TEST_PLUGIN_DISPLAY
#define TEST_PLUGIN_DISPLAY "TEST PLUGIN 1"
#endif
#ifndef TEST_PLUGIN_DESC
#define TEST_PLUGIN_DESC "Qt rewrite test plugin"
#endif

// 内联定长结构：0x58 头 + 2 个 0x3C 端口
constexpr DWORD kBlobSize = 0x58 + 2 * 0x3C;

// 命令槽记录（vtable 索引 13）。单进程单实例测试，用 Interlocked 只为对齐类型。
volatile LONG g_cmd = 0;
volatile LONG g_cmdSize = 0;
volatile LONG g_rect[4] = { 0, 0, 0, 0 };
volatile LONG g_cmdCalls = 0;
volatile LONG g_releaseCalls = 0;

void CopyFixed(char* dst, size_t cap, const char* src) {
    size_t n = std::strlen(src);
    if (n >= cap) n = cap - 1;
    std::memcpy(dst, src, n);
}

void WriteI32(unsigned char* base, DWORD off, int v) {
    std::memcpy(base + off, &v, sizeof(v));
}

class TestInstance {
public:
    // vtable 索引 0：占位
    virtual void pad0() {}
    // vtable 索引 1：释放（原版通过 [vtable+4] 调用）
    virtual void release() {
        ::InterlockedIncrement(&g_releaseCalls);
        delete this;
    }

    virtual void pad2() {}
    virtual void pad3() {}
    virtual void pad4() {}
    virtual void pad5() {}
    virtual void pad6() {}
    virtual void pad7() {}
    virtual void pad8() {}
    virtual void pad9() {}
    virtual void pad10() {}
    virtual void pad11() {}
    virtual void pad12() {}

    // vtable 索引 13（原版 +0x34）：命令分派。命令 100 收 16 字节矩形，
    // 返回 -100 表示「不提供自定义视图」（与宿主侧判定一致）。
    virtual int command(int cmd, void* arg, int size) {
        ::InterlockedIncrement(&g_cmdCalls);
        ::InterlockedExchange(&g_cmd, cmd);
        ::InterlockedExchange(&g_cmdSize, size);
        if (cmd == 100 && arg && size >= 16) {
            const int* r = static_cast<const int*>(arg);
            for (int i = 0; i < 4; ++i) ::InterlockedExchange(&g_rect[i], r[i]);
            return 0;
        }
        return -100;
    }

    virtual void pad14() {}
    virtual void pad15() {}
    virtual void pad16() {}
    virtual void pad17() {}

    // vtable 索引 18（原版 +0x48）：上报注册信息
    virtual DWORD info(void* buf, DWORD size, DWORD* outSize) {
        if (outSize) *outSize = kBlobSize;
        if (!buf || size < kBlobSize) return 0;
        unsigned char* p = static_cast<unsigned char*>(buf);
        std::memset(p, 0, kBlobSize);
        CopyFixed(reinterpret_cast<char*>(p + 0x00), 20, "Test Author");
        CopyFixed(reinterpret_cast<char*>(p + 0x14), 10, "1.2.3");
        CopyFixed(reinterpret_cast<char*>(p + 0x1E), 50, TEST_PLUGIN_DESC);
        WriteI32(p, 0x50, 2);   // 构件类型：数据源构件
        WriteI32(p, 0x54, 2);   // 端口数
        const DWORD base = 0x58;
        WriteI32(p, base + 0x28, 1);  // 端口 ID
        WriteI32(p, base + 0x2C, 1);  // 数据类型 1 = 8位无符号整型
        WriteI32(p, base + 0x30, 1);  // 最大端口数量
        WriteI32(p, base + 0x34, 1);  // 缺省端口数量
        WriteI32(p, base + 0x38, 1);  // 端口类型 1 = 数据流输入
        const DWORD base2 = base + 0x3C;
        WriteI32(p, base2 + 0x28, 2);
        WriteI32(p, base2 + 0x2C, 1);
        WriteI32(p, base2 + 0x30, 2);
        WriteI32(p, base2 + 0x34, 1);
        WriteI32(p, base2 + 0x38, 2); // 端口类型 2 = 数据流输出
        return 0;
    }
};

} // namespace

extern "C" __declspec(dllexport) DWORD __stdcall RegComNum(int* n)
{
    if (!n) return 1;
    *n = 1;
    return 0;
}

extern "C" __declspec(dllexport) DWORD __stdcall RegComInfo(DWORD idx, char* regName,
                                                            char* display, int* reserved)
{
    if (idx != 1 || !regName || !display) return 1;
    std::strcpy(regName, TEST_PLUGIN_NAME);
    std::strcpy(display, TEST_PLUGIN_DISPLAY);
    if (reserved) *reserved = 0;
    return 0;
}

extern "C" __declspec(dllexport) DWORD __stdcall CreateInstance(const char* name,
                                                               void** out,
                                                               void* p2, void* p3)
{
    (void)p2; (void)p3;
    if (!name || std::strcmp(name, TEST_PLUGIN_NAME) != 0 || !out)
        return 1;
    *out = new TestInstance();
    return 0;
}

extern "C" __declspec(dllexport) DWORD __stdcall ShowModal(const char* file)
{
    if (!file || !*file) return 0;
    const std::string marker = std::string(file) + ".modal.log";
    HANDLE h = ::CreateFileA(marker.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 2;
    DWORD written = 0;
    ::WriteFile(h, file, static_cast<DWORD>(std::strlen(file)), &written, nullptr);
    ::CloseHandle(h);
    return 0;
}

// 命令槽/释放槽记录：out = {cmd, argSize, rect[4], cmdCalls, releaseCalls}
extern "C" __declspec(dllexport) int __stdcall TestPluginStats(int* out, int cap)
{
    if (!out || cap < 8) return 0;
    out[0] = g_cmd;
    out[1] = g_cmdSize;
    for (int i = 0; i < 4; ++i) out[2 + i] = g_rect[i];
    out[6] = g_cmdCalls;
    out[7] = g_releaseCalls;
    return 8;
}

extern "C" __declspec(dllexport) void __stdcall TestPluginResetStats()
{
    ::InterlockedExchange(&g_cmd, 0);
    ::InterlockedExchange(&g_cmdSize, 0);
    for (int i = 0; i < 4; ++i) ::InterlockedExchange(&g_rect[i], 0);
    ::InterlockedExchange(&g_cmdCalls, 0);
    ::InterlockedExchange(&g_releaseCalls, 0);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
