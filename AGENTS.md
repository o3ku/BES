# AGENTS.md

This file provides guidance to the AI agent when working with code in this repository.

## Project

Qt5 Widgets rewrite (64-bit, C++17, MSVC) of the legacy BES binary-data browser. The original binaries at the repo root (`bes.exe`, `BesCom.dll`, `bes.cfg`) and `legacy_mfc/` are reference material only — never modify or delete them. Original Delphi forms are extracted in `tools/forms.dfm.txt`; behavior parity is tracked in `FEATURE_COMPARISON.md` and `docs/OPERATION_COMPARISON.md`. Deep design notes live in `QT_NOTES.md`.

## Build / Test

Scripts hardcode VS2019 (`vcvars64.bat`) and Qt 5.15.2 at `D:/local/Qt5/5.15.2/msvc2019_64` (also set in `CMakePresets.json`; presets use Ninja, binary dir `build/qt-msvc-x64-release/`).

- `build-qt.bat` — configure + build `bes.exe`
- `run-tests.bat` — build + `test_core.exe` + `test_ui.exe` (it sets `QT_QPA_PLATFORM=offscreen` and the Qt bin PATH itself)
- `tools\golden.bat` — golden screenshot; first run creates `tools/golden/baseline.png`, later runs compare SHA-256

Adding a source file requires editing `CMakeLists.txt` by hand — nothing is globbed. Core files must be added to the `bes`, `test_core`, AND `test_ui` source lists; app/dlg files to `bes` and `test_ui`. Forgetting one silently builds stale code into that target.

Tests must isolate QSettings from the real user config: set `BES_TEST_UI` (test_ui does this internally) or `BES_APP_SUFFIX` when launching `bes.exe` (golden.bat uses `_golden`).

## Architecture

- `src/core/` is the engine layer and must stay pure C++17 with **no Qt or framework dependencies** — `test_core` compiles it without Qt, and it is shared verbatim with the MFC version. UI-facing state belongs in `src/app/DocState.h`.
- Tabs are Views, not Docs; get the active document via `view->Doc()`.
- 64-bit only. 32-bit plugin DLLs are intentionally rejected — probe the PE Machine field before `LoadLibrary` (32-bit gives error 193).

## Plugin ABI (confirmed by disassembly — do not "fix")

`CreateInstance` is a **4-parameter** stdcall; `RegComInfo` takes a **1-based** index and writes **ANSI(GBK)** fixed-length buffers. Full signatures in `QT_NOTES.md`.

## Code style

- Sources are UTF-8 with Chinese comments and UI strings (MSVC `/utf-8` flag). Keep comments and user-facing text in Chinese to match.
- Colors are `uint32_t` in `0x00BBGGRR` (COLORREF) layout, not 0xRRGGBB. Include windows.h with `NOMINMAX`.

## Qt gotchas

- `menuBar()->setNativeMenuBar(false)` — native menus don't render over RDP.
- Status-bar text goes through a permanent `QLabel`, not `QStatusBar::showMessage` (it hides permanent widgets).
- Screenshots use `widget->grab()`; `PrintWindow` cannot capture Qt windows. Screenshot mode must clear the event-pane timestamps to keep hashes stable.
- Hex modes draw **two nibbles per byte**; drawing one silently drops half the data.
