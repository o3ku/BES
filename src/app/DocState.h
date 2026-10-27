// DocState.h —— 文档共享状态
#pragma once

#include <vector>

#include "core/FrameModel.h"
#include "core/Renderer.h"
#include "core/Selection.h"
#include "core/Bookmark.h"
#include "core/SearchEngine.h"

struct DocState
{
    bes::FrameModel   frame;
    bes::RenderParams params;
    bes::ColorScheme  colors;
    bes::SelectionSet sel;
    bes::BookmarkSet  marks;

    // 不等长帧（「不等长帧查看/处理」产生）
    bes::VariableFrameSet varFrames;

    // 单步模式（工具栏「单步」，只对数据流输入有效）
    bool singleStep = false;

    // 编辑模式（原版 EditBtn 的引擎编辑态；退出时按脏标志询问是否保存）
    bool editMode = false;
    // 十六进制键入：已收到的高位半字节（-1 = 无）
    int  pendingNibble = -1;

    // 帧过滤（「分析 → 过滤」产生）
    bes::FrameFilter         filter;
    std::vector<uint64_t>    visibleFrames;   // 过滤后可见帧号（升序）
    bool                     filterActive = false;
};
