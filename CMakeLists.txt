cmake_minimum_required(VERSION 3.20)
project(BESQt VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# BES 浏览/编辑/选取 —— Qt 重写版
#   引擎层 100% 复用 MFC 版 core/（纯 C++，无框架依赖）
#   UI 层基于 Qt5 Widgets
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets PrintSupport)

set(BES_CORE_SOURCES
  src/core/DataTypes.cpp
  src/core/FileMap.cpp
  src/core/FrameModel.cpp
  src/core/Selection.cpp
  src/core/Bookmark.cpp
  src/core/Editor.cpp
  src/core/Renderer.cpp
  src/core/SearchEngine.cpp
  src/core/OutputWriter.cpp
  src/core/FlowEngine.cpp
  src/core/CrashLog.cpp
)

set(BES_PLUGIN_SOURCES
  src/plugin/ComponentHost.cpp
)

set(BES_APP_SOURCES
  src/app/BesApp.h            src/app/BesApp.cpp
  src/app/Actions.h           src/app/Actions.cpp
  src/app/DocState.h
  src/app/BesDoc.h            src/app/BesDoc.cpp
  src/app/BesView.h           src/app/BesView.cpp
  src/app/MainWindow.h        src/app/MainWindow.cpp
  src/app/EventPane.h         src/app/EventPane.cpp
  src/app/GroupPane.h         src/app/GroupPane.cpp
  src/app/ViewArea.h          src/app/ViewArea.cpp
  src/app/ToolbarIcons.h      src/app/ToolbarIcons.cpp
  src/dlg/DlgOutputSet.h      src/dlg/DlgOutputSet.cpp
  src/dlg/DlgParamSet.h       src/dlg/DlgParamSet.cpp
  src/dlg/DlgPosition.h       src/dlg/DlgPosition.cpp
  src/dlg/DlgPrint.h          src/dlg/DlgPrint.cpp
  src/dlg/DlgFilter.h         src/dlg/DlgFilter.cpp
  src/dlg/DlgSearchUnique.h   src/dlg/DlgSearchUnique.cpp
  src/dlg/DlgSearchFl.h       src/dlg/DlgSearchFl.cpp
  src/dlg/DlgComponent.h      src/dlg/DlgComponent.cpp
  src/dlg/DlgVarFrame.h       src/dlg/DlgVarFrame.cpp
  src/dlg/DlgComSelect.h      src/dlg/DlgComSelect.cpp
  src/dlg/DlgProgress.h       src/dlg/DlgProgress.cpp
  src/dlg/DlgProcess.h        src/dlg/DlgProcess.cpp
  src/dlg/DlgGroupItems.h     src/dlg/DlgGroupItems.cpp
  src/dlg/DlgCustomize.h      src/dlg/DlgCustomize.cpp
  src/dlg/DlgAbout.h          src/dlg/DlgAbout.cpp
)

set(BES_QT_SOURCES
  src/app/main.cpp
  ${BES_APP_SOURCES}
)

add_executable(bes WIN32
  ${BES_CORE_SOURCES}
  ${BES_PLUGIN_SOURCES}
  ${BES_QT_SOURCES}
  src/res/resources.qrc
)

target_include_directories(bes PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bes PRIVATE
  Qt5::Widgets
  Qt5::PrintSupport
)

if(MSVC)
  target_compile_options(bes PRIVATE /utf-8 /W4 /permissive- /wd4820 /wd4996 /wd4267 /wd4244)
else()
  target_compile_options(bes PRIVATE -Wall -Wextra)
endif()

set_target_properties(bes PROPERTIES
  OUTPUT_NAME "bes"
  WIN32_EXECUTABLE TRUE
)

# ---------------------------------------------------------------------------
# 构件 ABI 测试 DLL（64 位；模拟原版 4 参数 CreateInstance）
# ---------------------------------------------------------------------------
add_library(test_plugin SHARED tests/test_plugin.cpp tests/test_plugin.def)
set_target_properties(test_plugin PROPERTIES PREFIX "" OUTPUT_NAME "test_plugin")
if(MSVC)
  target_compile_options(test_plugin PRIVATE /utf-8 /W3)
endif()

# ---------------------------------------------------------------------------
# Qt 界面冒烟测试（offscreen）
# ---------------------------------------------------------------------------
add_executable(test_ui
  tests/test_ui.cpp
  ${BES_CORE_SOURCES}
  ${BES_PLUGIN_SOURCES}
  ${BES_APP_SOURCES}
  src/res/resources.qrc
)
target_include_directories(test_ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_ui PRIVATE Qt5::Widgets Qt5::PrintSupport)
if(MSVC)
  target_compile_options(test_ui PRIVATE /utf-8 /W3 /wd4996)
  target_link_options(test_ui PRIVATE /SUBSYSTEM:CONSOLE)
endif()
add_dependencies(test_ui test_plugin)
add_custom_command(TARGET test_ui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:test_plugin> $<TARGET_FILE_DIR:test_ui>
  VERBATIM)

# ---------------------------------------------------------------------------
# 核心引擎单元测试
# ---------------------------------------------------------------------------
add_executable(test_core
  tests/test_core.cpp
  src/core/DataTypes.cpp
  src/core/FileMap.cpp
  src/core/FrameModel.cpp
  src/core/Selection.cpp
  src/core/Bookmark.cpp
  src/core/Editor.cpp
  src/core/Renderer.cpp
  src/core/SearchEngine.cpp
  src/core/OutputWriter.cpp
  src/core/FlowEngine.cpp
  src/core/CrashLog.cpp
)
target_include_directories(test_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
  target_compile_options(test_core PRIVATE /utf-8 /W4 /wd4996)
  target_link_options(test_core PRIVATE /SUBSYSTEM:CONSOLE)
endif()
