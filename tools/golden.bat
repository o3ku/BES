@echo off
setlocal
cd /d D:\BES

if not exist tools\golden mkdir tools\golden
python tools\make_sample.py build\golden_sample.dat
if errorlevel 1 (echo SAMPLE FAILED & exit /b 1)

set BES_APP_SUFFIX=_golden
set PATH=D:\local\Qt5\5.15.2\msvc2019_64\bin;%PATH%

build\qt-msvc-x64-release\bes.exe --screenshot tools\golden\current.png build\golden_sample.dat
if errorlevel 1 (echo CAPTURE FAILED & exit /b 1)

if not exist tools\golden\baseline.png (
  copy /y tools\golden\current.png tools\golden\baseline.png >nul
  echo : tools\golden\baseline.png
  exit /b 0
)

powershell -NoProfile -Command ^
  "$a=(Get-FileHash 'tools/golden/current.png').Hash; $b=(Get-FileHash 'tools/golden/baseline.png').Hash; if($a -eq $b){Write-Host 'GOLDEN OK'; exit 0}else{Write-Host ('GOLDEN DIFF: ' + $a + ' != ' + $b); exit 1}"
if errorlevel 1 (
  echo  tools\golden\current.png
  exit /b 1
)
exit /b 0
