// DataTypes.h —— 数据类型 / 显示模式 / 端口类型 等公共枚举
#pragma once

#include <cstdint>

namespace bes {

// 原程序「数据格式」对话框中列出的端口数据类型
enum class DataType {
    Unknown = 0,
    U8,             // 8位无符号整型
    S8,             // 8位有符号整型
    U16,            // 16位无符号整型
    S16,            // 16位有符号整型
    S16Motorola,    // 16位有符号整型(摩托罗拉格式)
    U32,            // 32位无符号整型
    S32,            // 32位有符号整型
    S64,            // 64位有符号整型
    F32,            // 32位浮点
    F64,            // 64位浮点
    Str,            // 字符串
    EOI,            // EOI [分块结束标志]
    Eof             // EOF [数据结束标志]
};

const wchar_t* DataTypeName(DataType t);

// 窗口排布（原版工具栏 分页/横排/竖排/分组横排/分组竖排，主窗体全局状态
// [form+0x498]：PageBtnClick=0、HorizBtnClick=1、VertBtnClick=2、
// PageHorizBtnClick=3、PageVertBtnClick=4，五个处理函数都遍历全部停靠站点，
// 故排布作用于所有视图而非当前文档）
enum class ViewLayout {
    Paged = 0,      // 分页：一屏一个视图，用标签切换
    Horizontal,     // 横排：全部视图并排
    Vertical,       // 竖排：全部视图堆叠
    GroupH,         // 分组横排：组内视图并排
    GroupV          // 分组竖排：组内视图堆叠
};

const wchar_t* ViewLayoutName(ViewLayout l);

// 纵轴显示（原版 YFN/YFL/YFB）
enum class YAxisMode {
    YFN = 0,   // 帧号
    YFL,       // 帧长
    YFB        // 比特位置
};

// 显示模式（对应原程序 ViewModeComboBox 的 5 项）
enum class ViewMode {
    BitBitmap = 0,  // 比特位图
    BitChar,        // 等长帧比特字符
    Hex,            // 等长帧十六进制
    Char,           // 字符方式
    Pixel,          // 位图
    HexChar         // 十六进制字符对应（十六进制 + ASCII 并排）
};

const wchar_t* ViewModeName(ViewMode m);

// 端口类型（构件属性）
enum class PortKind {
    DataIn = 0,    // 数据流输入
    DataOut,       // 数据流输出
    StatusIn,      // 状态输入
    StatusOut      // 状态输出
};

const wchar_t* PortKindName(PortKind k);

// 构件类型（RegComInfo 的 comType）
// 反汇编：BesCom.dll 0x5DAFF8 起 4 项 PChar 表，依次为
// 普通构件 / 数据源构件 / 通用显示构件 / 探头构件；紧随其后的 4 项
// （0x5DB008）是端口类型表。「局级系统构件」不是类型名，而是对导出了
// RegComNum/RegComInfo/CreateInstance 的构件 DLL 的称呼。
enum class ComponentKind {
    Normal = 0,        // 普通构件
    DataSource,        // 数据源构件
    GeneralDisplay,    // 通用显示构件
    Probe              // 探头构件
};

const wchar_t* ComponentKindName(ComponentKind k);

// 界面风格（原程序：传统风格 / VS.NET风格 / Office风格）
enum class LookStyle {
    Traditional = 0,
    VSNet,
    Office
};

} // namespace bes
