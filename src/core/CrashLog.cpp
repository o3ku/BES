// CrashLog.cpp
#include "core/CrashLog.h"
#include <windows.h>
#include <cstdarg>

namespace bes {

namespace {

void AppendToLog(const wchar_t* file, const wchar_t* text) {
    wchar_t path[MAX_PATH]{};
    ::GetTempPathW(MAX_PATH, path);
    ::wcscat_s(path, file);
    HANDLE h = ::CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    ::WriteFile(h, text, static_cast<DWORD>(wcslen(text) * sizeof(wchar_t)), &written, nullptr);
    ::CloseHandle(h);
}

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    wchar_t buf[2048]{};
    if (ep && ep->ExceptionRecord) {
        EXCEPTION_RECORD* er = ep->ExceptionRecord;
        wsprintfW(buf,
            L"[CRASH] code=0x%08lX addr=%p\r\n"
            L"  module=%s\r\n"
            L"  rva=0x%llX\r\n",
            er->ExceptionCode,
            er->ExceptionAddress,
            L"?",
            (unsigned long long)0);
        AppendToLog(L"bes_crash.log", buf);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void InstallCrashHandler() {
    ::SetUnhandledExceptionFilter(CrashHandler);
}

void Trace(const wchar_t* fmt, ...) {
    wchar_t buf[1024]{};
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    wcscat_s(buf, L"\r\n");
    AppendToLog(L"bes_trace.log", buf);
}

} // namespace bes
