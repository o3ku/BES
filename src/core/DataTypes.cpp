// DataTypes.cpp
#include "core/DataTypes.h"

namespace bes {

const wchar_t* DataTypeName(DataType t) {
    switch (t) {
    case DataType::U8:          return L"8位无符号整型";
    case DataType::S8:          return L"8位有符号整型";
    case DataType::U16:         return L"16位无符号整型";
    case DataType::S16:         return L"16位有符号整型";
    case DataType::S16Motorola: return L"16位有符号整型(摩托罗拉格式)";
    case DataType::U32:         return L"32位无符号整型";
    case DataType::S32:         return L"32位有符号整型";
    case DataType::S64:         return L"64位有符号整型";
    case DataType::F32:         return L"32位浮点";
    case DataType::F64:         return L"64位浮点";
    case DataType::Str:         return L"字符串";
    case DataType::EOI:         return L"EOI [分块结束标志]";
    case DataType::Eof:         return L"EOF [数据结束标志]";
    default:                    return L"未知数据类型";
    }
}

const wchar_t* ViewLayoutName(ViewLayout l) {
    switch (l) {
    case ViewLayout::Paged:      return L"分页";
    case ViewLayout::Horizontal: return L"横排";
    case ViewLayout::Vertical:   return L"竖排";
    case ViewLayout::GroupH:     return L"分组横排";
    case ViewLayout::GroupV:     return L"分组竖排";
    default:                     return L"分页";
    }
}

const wchar_t* ViewModeName(ViewMode m) {
    switch (m) {
    case ViewMode::BitBitmap: return L"比特位图";
    case ViewMode::BitChar:   return L"等长帧比特字符";
    case ViewMode::Hex:       return L"等长帧十六进制";
    case ViewMode::Char:      return L"字符方式";
    case ViewMode::Pixel:     return L"位图";
    case ViewMode::HexChar:   return L"十六进制字符对应";
    default:                  return L"";
    }
}

const wchar_t* PortKindName(PortKind k) {
    switch (k) {
    case PortKind::DataIn:   return L"数据流输入";
    case PortKind::DataOut:  return L"数据流输出";
    case PortKind::StatusIn: return L"状态输入";
    case PortKind::StatusOut:return L"状态输出";
    default:                 return L"";
    }
}

const wchar_t* ComponentKindName(ComponentKind k) {
    // 原版表序：普通/数据源/通用显示/探头（BesCom.dll 0x5DAFF8）
    switch (k) {
    case ComponentKind::Normal:         return L"普通构件";
    case ComponentKind::DataSource:     return L"数据源构件";
    case ComponentKind::GeneralDisplay: return L"通用显示构件";
    case ComponentKind::Probe:          return L"探头构件";
    default:                            return L"";
    }
}

} // namespace bes
