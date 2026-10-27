// CrashLog.h —— 未处理异常捕获（写文件，便于定位崩溃）
#pragma once

#include <windows.h>
#include <cstdio>

namespace bes {

// 安装未处理异常过滤器：崩溃时把异常信息写到 %TEMP%\bes_crash.log
void InstallCrashHandler();

// 简单的日志点（写 %TEMP%\bes_trace.log）
void Trace(const wchar_t* fmt, ...);

} // namespace bes
