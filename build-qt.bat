@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo VCVARS FAILED & exit /b 1)
cd /d D:\BES
cmake --preset msvc-x64-release
if errorlevel 1 (echo CONFIGURE FAILED & exit /b 1)
cmake --build --preset release
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
