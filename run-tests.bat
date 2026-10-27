@echo off
chcp 65001 >nul
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\BES

cmake --preset msvc-x64-release >nul
cmake --build --preset release
if not "%ERRORLEVEL%"=="0" (echo BUILD FAILED & exit /b 1)

set PATH=D:\local\Qt5\5.15.2\msvc2019_64\bin;%PATH%
set QT_QPA_PLATFORM=offscreen

echo.
echo ===== 核心引擎单元测试 =====
build\qt-msvc-x64-release\test_core.exe
rem 崩溃返回负数退出码（如 -1073741819），errorlevel 1 判不出来，必须用 NEQ 0
if not "%ERRORLEVEL%"=="0" (echo CORE TESTS FAILED & exit /b 1)

echo.
echo ===== Qt 界面冒烟测试（offscreen） =====
build\qt-msvc-x64-release\test_ui.exe
if not "%ERRORLEVEL%"=="0" (echo UI TESTS FAILED & exit /b 1)

echo.
echo ALL TESTS PASSED
endlocal
