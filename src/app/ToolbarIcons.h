// ToolbarIcons.h —— 工具栏图标索引（与 src/res/toolbar.png 一一对应）
#pragma once

#include <QVector>
#include <QIcon>

// toolbar.png 为 49 个 16x16 图标的横向长条
namespace tbico {

QVector<QIcon> LoadIcons();
enum {
    New = 0,        // 新建
    Open = 1,       // 打开
    Save = 2,       // 保存
    Print = 3,      // 打印
    Cut = 4,        // 剪切
    Copy = 5,       // 复制
    Paste = 6,      // 粘贴
    Undo = 7,       // 撤销
    Redo = 8,       // 重做
    Step = 9,       // 单步
    Refresh = 10,   // 刷新
    Page = 11,      // 分页
    Horiz = 12,     // 横排
    Vert = 13,      // 竖排
    GroupHoriz = 14,// 分组横排
    GroupVert = 15, // 分组竖排
    Title = 16,     // 标题
    Bin = 17,       // 二进制
    Hex = 18,       // 十六进制
    Text = 19,      // 文本
    Bmp = 20,       // 位图
    Filter = 21,    // 过滤
    Not = 22,       // 取反
    Reverse = 23,   // 倒序
    Param = 24,     // 参数
    Ruler = 25,     // 标尺
    XTab = 26,      // XTAB
    YTab = 27,      // YTAB
    TabLock = 28,   // TAB跟随
    LinkScroll = 29,// 联动浏览
    SetStart = 30,  // 设为起点
    ContSel = 31,   // 连续选取
    RectSel = 32,   // 块选取
    RowSel = 33,    // 行选取
    ColSel = 34,    // 列选取
    ZoomOut = 35,   // 放大（原版命名：ZoomOutBtn=放大）
    ZoomIn = 36,    // 缩小（原版命名：ZoomInBtn=缩小）
    ZoomReset = 37, // 恢复
    SelParam = 38,  // 选取参数
    Output = 39,    // 数据输出
    SaveAs = 40,    // 另存为
    SearchFl = 41,  // 帧长查找
    SearchStr = 42, // 独特码查找
    QuitSearch = 43,// 退出查找状态
    BookMark = 44,  // 切换书签
    BmNext = 45,    // 下一标签
    BmPrev = 46,    // 上一标签
    BmClear = 47,   // 清除标签
    Goto = 48,      // 定位
};
} // namespace tbico
