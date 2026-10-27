#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re

p = 'src/ui/BESMainFrame.cpp'
s = open(p, encoding='utf-8').read()

new_create = '''void BESMainFrame::CreateToolBars() {
    using namespace tbico;

    // ---- Bar0: 菜单栏（文件/编辑/查看/处理/分析/识别/定制/窗口/帮助） ----
    if (!m_wndMenuBar.Create(this))
        return;
    m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() | CBRS_SIZE_DYNAMIC);

    auto btn = [](UINT id, int img, const wchar_t* text) {
        return CMFCToolBarButton(id, img, text);
    };

    // ---- Bar1: 主工具栏（原版 17 项） ----
    if (!m_wndToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE | WS_CHILD | WS_VISIBLE | CBRS_SIZE_DYNAMIC))
        return;
    m_wndToolBar.SetWindowText(L"主工具栏");
    m_wndToolBar.LoadBitmap(IDB_TOOLBARICONS);
    m_wndToolBar.RemoveAllButtons();
    m_wndToolBar.InsertButton(btn(ID_FILE_NEW,      New,    L"新建"));
    m_wndToolBar.InsertButton(btn(ID_FILE_OPEN,     Open,   L"打开"));
    m_wndToolBar.InsertButton(btn(ID_FILE_SAVE,     Save,   L"保存"));
    m_wndToolBar.InsertButton(btn(ID_FILE_PRINT,    Print,  L"打印"));
    m_wndToolBar.InsertSeparator();
    m_wndToolBar.InsertButton(btn(ID_EDIT_CUT,      Cut,    L"剪切"));
    m_wndToolBar.InsertButton(btn(ID_EDIT_COPY,     Copy,   L"复制"));
    m_wndToolBar.InsertButton(btn(ID_EDIT_PASTE,    Paste,  L"粘贴"));
    m_wndToolBar.InsertButton(btn(ID_EDIT_UNDO,     Undo,   L"撤销"));
    m_wndToolBar.InsertButton(btn(ID_EDIT_REDO,     Redo,   L"重做"));
    m_wndToolBar.InsertSeparator();
    m_wndToolBar.InsertButton(btn(ID_ANALYZE_ITERATE, Step, L"单步"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_REFRESH,  Refresh, L"刷新"));
    m_wndToolBar.InsertSeparator();
    m_wndToolBar.InsertButton(btn(ID_VIEW_PAGE,     Page,   L"分页"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_HORIZ,    Horiz,  L"横排"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_VERT,     Vert,   L"竖排"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_GROUP_HORIZ, GroupHoriz, L"分组横排"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_GROUP_VERT,  GroupVert,  L"分组竖排"));
    m_wndToolBar.InsertButton(btn(ID_VIEW_TITLE,    Title,  L"标题"));
    m_wndToolBar.EnableTextLabels(TRUE);
    m_wndToolBar.SetSizes(CSize(40, 36), CSize(16, 16));

    // ---- Bar2: 格式栏（原版 20 项） ----
    if (!m_wndFormatBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE | WS_CHILD | WS_VISIBLE | CBRS_SIZE_DYNAMIC))
        return;
    m_wndFormatBar.SetWindowText(L"格式栏");
    m_wndFormatBar.LoadBitmap(IDB_TOOLBARICONS);
    WireFormatBar();

    // ---- Bar3: 快捷操作（原版 17 项） ----
    if (!m_wndQuickBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE | WS_CHILD | WS_VISIBLE | CBRS_SIZE_DYNAMIC))
        return;
    m_wndQuickBar.SetWindowText(L"快捷操作");
    m_wndQuickBar.LoadBitmap(IDB_TOOLBARICONS);
    m_wndQuickBar.RemoveAllButtons();
    m_wndQuickBar.InsertButton(btn(ID_VIEW_LINK,      LinkScroll, L"联动浏览"));
    m_wndQuickBar.InsertButton(btn(ID_SET_START,      SetStart,   L"设为起点"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_CONTINUOUS, ContSel, L"连续选取"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_RECT,    RectSel,    L"块选取"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_ROW,     RowSel,     L"行选取"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_COL,     ColSel,     L"列选取"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_LOAD,    SelParam,   L"选取参数"));
    m_wndQuickBar.InsertSeparator();
    m_wndQuickBar.InsertButton(btn(ID_DATA_OUTPUT,    Output,     L"数据输出"));
    m_wndQuickBar.InsertButton(btn(ID_FILE_SAVE_AS,   SaveAs,     L"另存为"));
    m_wndQuickBar.InsertSeparator();
    m_wndQuickBar.InsertButton(btn(ID_ANALYZE_SEARCH_FL, SearchFl,  L"帧长查找"));
    m_wndQuickBar.InsertButton(btn(ID_ANALYZE_SEARCH_UNIQUE, SearchStr, L"独特码查找"));
    m_wndQuickBar.InsertButton(btn(ID_ANALYZE_QUIT_SEARCH, QuitSearch, L"退出查找状态"));
    m_wndQuickBar.InsertSeparator();
    m_wndQuickBar.InsertButton(btn(ID_BOOKMARK_TOGGLE, BookMark,  L"切换书签"));
    m_wndQuickBar.InsertButton(btn(ID_BOOKMARK_NEXT,  BmNext,     L"下一标签"));
    m_wndQuickBar.InsertButton(btn(ID_BOOKMARK_PREV,  BmPrev,     L"上一标签"));
    m_wndQuickBar.InsertButton(btn(ID_SELECT_CLEAR_CURRENT, BmClear, L"清除标签"));
    m_wndQuickBar.InsertSeparator();
    m_wndQuickBar.InsertButton(btn(ID_ANALYZE_GOTO,   Goto,       L"定位"));
    m_wndQuickBar.EnableTextLabels(TRUE);
    m_wndQuickBar.SetSizes(CSize(40, 36), CSize(16, 16));

    // ---- 停靠（原版 4 栏均在顶部） ----
    EnableDocking(CBRS_ALIGN_ANY);
    m_wndMenuBar.EnableDocking(CBRS_ALIGN_TOP);
    m_wndToolBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
    m_wndFormatBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
    m_wndQuickBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
    DockPane(&m_wndMenuBar);
    DockPane(&m_wndToolBar);
    DockPane(&m_wndFormatBar);
    DockPane(&m_wndQuickBar);

    RecalcLayout(FALSE);
}'''

new_wire = '''void BESMainFrame::WireFormatBar() {
    using namespace tbico;
    m_wndFormatBar.RemoveAllButtons();

    // 起点 / 帧长 / 步进（原版 FbEdit/FlEdit/StepEdit）
    CMFCToolBarButton lbl1(0, -1, L"起点:"); lbl1.SetStyle(TBBS_DISABLED | TBBS_NOPREFIX);
    m_wndFormatBar.InsertButton(lbl1);
    CMFCToolBarEditBoxButton edt1(IDC_EDIT_START, -1, ES_NUMBER | WS_BORDER, 58);
    m_wndFormatBar.InsertButton(edt1);

    CMFCToolBarButton lbl2(0, -1, L"帧长:"); lbl2.SetStyle(TBBS_DISABLED | TBBS_NOPREFIX);
    m_wndFormatBar.InsertButton(lbl2);
    CMFCToolBarEditBoxButton edt2(IDC_EDIT_FRAMELEN, -1, ES_NUMBER | WS_BORDER, 68);
    m_wndFormatBar.InsertButton(edt2);

    CMFCToolBarButton lbl3(0, -1, L"步进:"); lbl3.SetStyle(TBBS_DISABLED | TBBS_NOPREFIX);
    m_wndFormatBar.InsertButton(lbl3);
    CMFCToolBarEditBoxButton edt3(IDC_EDIT_STEP, -1, ES_NUMBER | WS_BORDER, 45);
    m_wndFormatBar.InsertButton(edt3);

    // 显示模式（原版 ViewModeComboBox，5 项）
    CMFCToolBarButton lbl4(0, -1, L"显示:"); lbl4.SetStyle(TBBS_DISABLED | TBBS_NOPREFIX);
    m_wndFormatBar.InsertButton(lbl4);
    CMFCToolBarComboBoxButton cbo(IDC_CBO_VIEWMODE, -1, CBS_DROPDOWNLIST | WS_VSCROLL, 130);
    cbo.AddItem(L"比特位图");
    cbo.AddItem(L"等长帧比特字符");
    cbo.AddItem(L"等长帧十六进制");
    cbo.AddItem(L"字符方式");
    cbo.AddItem(L"十六进制字符对应");
    cbo.SelectItem(2);
    m_wndFormatBar.InsertButton(cbo);

    auto b = [](UINT id, int img, const wchar_t* t) { return CMFCToolBarButton(id, img, t); };

    // 二进制 / 十六进制 / 文本 / 位图 / 过滤 / 取反 / 倒序 / 参数
    m_wndFormatBar.InsertButton(b(ID_VIEW_MODE_BITMAP, Bin,  L"二进制"));
    m_wndFormatBar.InsertButton(b(ID_VIEW_MODE_HEX,    Hex,  L"十六进制"));
    m_wndFormatBar.InsertButton(b(ID_VIEW_MODE_CHAR,   Text, L"文本"));
    m_wndFormatBar.InsertButton(b(ID_VIEW_MODE_PIXEL,  Bmp,  L"位图"));
    m_wndFormatBar.InsertButton(b(ID_ANALYZE_FILTER,   Filter,  L"过滤"));
    m_wndFormatBar.InsertButton(b(ID_DATA_NOT,         Not,     L"取反"));
    m_wndFormatBar.InsertButton(b(ID_DATA_REVERSE,     Reverse, L"倒序"));
    m_wndFormatBar.InsertButton(b(ID_CUSTOM_PARAM,     Param,   L"参数"));

    // ―― 分组 ―― 标尺 / XTAB / YTAB / TAB跟随 / 纵轴显示
    m_wndFormatBar.InsertSeparator();
    m_wndFormatBar.InsertButton(b(ID_VIEW_RULER, Ruler, L"标尺"));
    m_wndFormatBar.InsertSeparator();
    m_wndFormatBar.InsertButton(b(ID_VIEW_HTAB,  XTab,  L"XTAB"));
    m_wndFormatBar.InsertSeparator();
    m_wndFormatBar.InsertButton(b(ID_VIEW_VTAB,  YTab,  L"YTAB"));
    m_wndFormatBar.InsertSeparator();
    m_wndFormatBar.InsertButton(b(ID_TAB_LOCK,   TabLock, L"TAB跟随"));
    m_wndFormatBar.InsertSeparator();

    // 纵轴显示（YFN/YFL/YFB）
    CMFCToolBarComboBoxButton cboY(IDC_CBO_YDISP, -1, CBS_DROPDOWNLIST, 70);
    cboY.AddItem(L"YFN"); cboY.AddItem(L"YFL"); cboY.AddItem(L"YFB");
    cboY.SelectItem(0);
    m_wndFormatBar.InsertButton(cboY);
    m_wndFormatBar.InsertSeparator();

    // 放大 / 缩小 / 恢复
    m_wndFormatBar.InsertButton(b(ID_VIEW_ZOOM_OUT,   ZoomOut,   L"放大"));
    m_wndFormatBar.InsertButton(b(ID_VIEW_ZOOM_IN,    ZoomIn,    L"缩小"));
    m_wndFormatBar.InsertButton(b(ID_VIEW_ZOOM_RESET, ZoomReset, L"恢复"));

    m_wndFormatBar.EnableTextLabels(TRUE);
    m_wndFormatBar.SetSizes(CSize(40, 36), CSize(16, 16));
}'''

m1 = re.search(r"void BESMainFrame::CreateToolBars\(\) \{[\s\S]*?\n\}", s)
s = s[:m1.start()] + new_create + s[m1.end():]
m2 = re.search(r"void BESMainFrame::WireFormatBar\(\) \{[\s\S]*?\n\}", s)
s = s[:m2.start()] + new_wire + s[m2.end():]
open(p, 'w', encoding='utf-8').write(s)
print('patched')
