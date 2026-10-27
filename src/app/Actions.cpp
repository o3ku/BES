// Actions.cpp —— 构建全部命令并连接到 MainWindow 动作
#include "app/Actions.h"
#include "app/MainWindow.h"
#include "app/BesDoc.h"
#include "app/BesView.h"
#include "app/ToolbarIcons.h"

#include <algorithm>

#include <QKeySequence>
#include <QMenu>
#include <QRegularExpression>
#include <QWidgetAction>

using namespace tbico;

namespace {

struct Def {
    int id;
    const char* text;        // 中文标题（含快捷键标记）
    int  icon;               // tbico 枚举，-1 = 无
    const char* shortcut;    // 可空
};

const QVector<Def> kDefs = {
    // 文件
    { CMD_FILE_NEW,      "新建(&N)",           New,    "Ctrl+N" },
    { CMD_FILE_OPEN,     "打开(&O)...",        Open,   "Ctrl+O" },
    { CMD_FILE_SAVE,     "保存(&S)",           Save,   "Ctrl+S" },
    { CMD_FILE_SAVE_AS,  "另存为(&A)...",      SaveAs, nullptr },
    { CMD_FILE_PRINT,    "打印当前文档(&P)",   Print,  "Ctrl+P" },
    { CMD_FILE_CLOSE,    "关闭(&C)",           -1,     nullptr },
    { CMD_FILE_CLOSE_ALL,"全部关闭(&A)",       -1,     nullptr },
    { CMD_APP_EXIT,      "退出(&X)",           -1,     nullptr },
    // 编辑
    { CMD_EDIT_CUT,      "剪切(&T)",           Cut,    "Ctrl+X" },
    { CMD_EDIT_COPY,     "复制(&C)",           Copy,   "Ctrl+C" },
    { CMD_EDIT_PASTE,    "粘贴(&P)",           Paste,  "Ctrl+V" },
    { CMD_EDIT_UNDO,     "撤销(&U)",           Undo,   "Ctrl+Z" },
    // 原版 DFM 的重做快捷键是 Shift+Ctrl+Z（不是 Qt 默认的 Ctrl+Y）
    { CMD_EDIT_REDO,     "重做(&R)",           Redo,   "Ctrl+Shift+Z" },
    { CMD_EDIT_MODE,     "进行编辑",           -1,     nullptr },
    // 查看子命令
    { CMD_VIEW_ZOOM_IN,       "放大(&Z)",       ZoomIn,      "Ctrl+=" },
    { CMD_VIEW_ZOOM_OUT,      "缩小(&O)",       ZoomOut,     "Ctrl+-" },
    { CMD_VIEW_ZOOM_RESET,    "恢复(&R)",       ZoomReset,   nullptr },
    { CMD_VIEW_RULER,         "标尺",           Ruler,       nullptr },
    { CMD_VIEW_GROUP_HORIZ,   "分组横排(&H)",   GroupHoriz,  nullptr },
    { CMD_VIEW_GROUP_VERT,    "分组竖排(&V)",   GroupVert,   nullptr },
    { CMD_VIEW_GROUP,         "窗口分组(&G)",   -1,          nullptr },
    { CMD_VIEW_LINK,          "联动(&L)",       LinkScroll,  nullptr },
    { CMD_VIEW_TITLE,         "标题(&T)",       Title,       nullptr },
    { CMD_VIEW_PAGE,          "分页(&P)",       Page,        nullptr },
    { CMD_VIEW_HORIZ,         "横排(&H)",       Horiz,       nullptr },
    { CMD_VIEW_VERT,          "竖排(&V)",       Vert,        nullptr },
    { CMD_VIEW_MODE_YFN,      "帧号",           -1,          nullptr },
    { CMD_VIEW_MODE_YFL,      "帧长",           -1,          nullptr },
    { CMD_VIEW_MODE_YFB,      "起始比特",       -1,          nullptr },
    // 选取
    { CMD_SELECT_CONTINUOUS,    "连续选取(&C)",  ContSel,   nullptr },
    { CMD_SELECT_RECT,          "块选取(&R)",    RectSel,   nullptr },
    { CMD_SELECT_ROW,           "行选取(&L)",    RowSel,    nullptr },
    { CMD_SELECT_COL,           "列选取(&O)",    ColSel,    nullptr },
    { CMD_SELECT_LOAD,          "选取参数读入(&R)...", SelParam, nullptr },
    { CMD_SELECT_SAVE,          "选取参数存盘(&S)...", -1,    nullptr },
    { CMD_SELECT_CLEAR_CURRENT, "清除当前选取(&C)", BmClear, "Del" },
    { CMD_SELECT_CLEAR_ALL,     "清除全部选取(&A)", -1,      nullptr },
    // 处理
    { CMD_ANALYZE_PARAM,         "参数",              Param,   nullptr },
    { CMD_ANALYZE_ITERATE,       "迭代",              -1,      nullptr },
    { CMD_ANALYZE_STEP,          "单步",              Step,    nullptr },
    { CMD_ANALYZE_VARFRAME,      "不等长帧查看...",   -1,      nullptr },
    { CMD_PROCESS_UNIQUE,        "不等长帧处理(&P)",  SearchStr, nullptr },
    { CMD_ANALYZE_GOTO,          "定位(&G)...",       Goto,    nullptr },
    { CMD_ANALYZE_SEARCH_UNIQUE, "独特码查找(&U)...", SearchStr, "F7" },
    { CMD_ANALYZE_SEARCH_FL,     "帧长查找(&L)...",   SearchFl, "F6" },
    { CMD_ANALYZE_QUIT_SEARCH,   "退出查找状态(&X)",  QuitSearch, nullptr },
    { CMD_ANALYZE_FILTER,        "过滤(&F)...",       Filter,  nullptr },
    // 分析
    { CMD_VIEW_MODE_BITMAP, "二进制(&B)", Bin,   nullptr },
    { CMD_VIEW_MODE_HEX,    "十六进制(&H)", Hex,  nullptr },
    { CMD_VIEW_MODE_CHAR,   "文本(&T)",    Text,  nullptr },
    { CMD_VIEW_MODE_PIXEL,  "位图(&M)",    Bmp,   nullptr },
    { CMD_VIEW_MODE_HEXCHAR, "十六进制字符对应(&X)", -1, nullptr },
    // 识别
    { CMD_IDS_DATA_IN,   "数据流输入(&I)",   -1, nullptr },
    { CMD_IDS_FORMAT_IN, "数据格式输入(&F)", -1, nullptr },
    { CMD_IDS_DATA_OUT,  "数据流输出(&O)",   -1, nullptr },
    // 定制（原版 CustomComSubItem 只挂 ComManageBtn「管理...」与 FastExecuteBtn「快速执行...」）
    { CMD_CUSTOM_COMPONENT, "管理...",            -1,      nullptr },
    { CMD_CUSTOM_FAST_EXEC, "快速执行...",        Refresh, nullptr },
    // 窗口
    { CMD_WINDOW_NEW, "新建窗口(&N)", -1, nullptr },
    // 帮助
    { CMD_APP_ABOUT, "关于...", -1, nullptr },
    // 快捷操作栏（Bar3）
    { CMD_SET_START,        "设为起点",       SetStart,  nullptr },
    { CMD_DATA_OUTPUT,      "数据输出...",    Output,    nullptr },
    { CMD_BOOKMARK_TOGGLE,  "切换书签",       BookMark,  nullptr },
    { CMD_BOOKMARK_NEXT,    "下一标签",       BmNext,    nullptr },
    { CMD_BOOKMARK_PREV,    "上一标签",       BmPrev,    nullptr },
    { CMD_BOOKMARK_CLEAR,   "清除标签",       BmClear,   nullptr },
    // 格式栏（Bar2）附加
    { CMD_DATA_NOT,     "取反",  Not,     nullptr },
    { CMD_DATA_REVERSE, "倒序",  Reverse, nullptr },
    { CMD_VIEW_HTAB,    "XTAB",  XTab,    nullptr },
    { CMD_VIEW_VTAB,    "YTAB",  YTab,    nullptr },
    { CMD_TAB_LOCK,     "TAB跟随", TabLock, nullptr },
    { CMD_VIEW_REFRESH, "刷新",  Refresh, "F5" },
    { CMD_VIEW_EVENT,   "事件(&E)", -1, nullptr },
};

} // namespace

Actions::Actions(MainWindow* mw, QObject* parent)
    : QObject(parent), m_mw(mw)
{
    Build();
}

void Actions::Build()
{
    const QVector<QIcon> icons = tbico::LoadIcons();

    for (const Def& d : kDefs) {
        if (m_actions.contains(d.id))
            continue;   // 首个定义生效
        QAction* a = new QAction(QString::fromUtf8(d.text), this);
        // 工具栏按钮文字按原版只显示短标题：去掉“(&X)”助记符和对话框省略号
        QString iconText = a->text();
        iconText.remove(QRegularExpression(QStringLiteral("\\(&[^)]\\)")));
        iconText.remove(QStringLiteral("..."));
        a->setIconText(iconText);
        if (d.icon >= 0 && d.icon < icons.size())
            a->setIcon(icons[d.icon]);
        if (d.id == CMD_DATA_REVERSE)
            a->setToolTip(QStringLiteral("选中为高位在前（0-7）未选为低位在前（7-0）"));
        if (d.id == CMD_VIEW_LINK)
            a->setToolTip(QStringLiteral("联动浏览"));
        // 原版四个选取按钮是 bsChecked（GroupIndex=2，AllowAllUp），可取消勾选
        if (d.id >= CMD_SELECT_CONTINUOUS && d.id <= CMD_SELECT_COL)
            a->setCheckable(true);
        if (d.id == CMD_ANALYZE_STEP)
            a->setToolTip(QStringLiteral("切换单步和连续状态\n只对数据流输入有效"));
        if (d.shortcut)
            a->setShortcut(QKeySequence(QString::fromLatin1(d.shortcut)));

        const int id = d.id;
        connect(a, &QAction::triggered, this, [this, id]() {
            MainWindow* mw = m_mw;

            switch (id) {
            // ---- 文件 ----
            case CMD_FILE_OPEN:  mw->DoOpen(); break;
            case CMD_FILE_SAVE:  mw->DoSave(); break;
            case CMD_FILE_SAVE_AS: mw->DoSaveAs(); break;
            case CMD_FILE_PRINT: mw->DoPrint(); break;
            case CMD_FILE_CLOSE_ALL: mw->DoCloseAll(); break;
            case CMD_APP_EXIT:   mw->close(); break;
            case CMD_APP_ABOUT:  mw->DoAbout(); break;

            // ---- 编辑（原版为引擎接口调用，非空壳） ----
            case CMD_EDIT_CUT:   mw->DoCut();   break;
            case CMD_EDIT_COPY:  mw->DoCopy();  break;
            case CMD_EDIT_PASTE: mw->DoPaste(); break;
            case CMD_EDIT_MODE:  mw->ToggleEditMode(); break;

            // ---- 选取 ----
            case CMD_SELECT_CONTINUOUS: mw->DoSelectMode(bes::SelectMode::Continuous); break;
            case CMD_SELECT_RECT:       mw->DoSelectMode(bes::SelectMode::Rect); break;
            case CMD_SELECT_ROW:        mw->DoSelectMode(bes::SelectMode::Row); break;
            case CMD_SELECT_COL:        mw->DoSelectMode(bes::SelectMode::Col); break;
            case CMD_SELECT_SAVE:       mw->DoSaveSelection(); break;
            case CMD_SELECT_LOAD:       mw->DoLoadSelection(); break;
            case CMD_SELECT_CLEAR_CURRENT: mw->DoClearCurrentSelection(); break;
            case CMD_SELECT_CLEAR_ALL:     mw->DoClearAllSelection(); break;

            // ---- 查看 ----
            case CMD_VIEW_ZOOM_IN: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.zoom = std::min(16, doc->State().params.zoom * 2);
                    if (auto* v = mw->ActiveView()) v->UpdateScrollRange();
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_VIEW_ZOOM_OUT: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.zoom = std::max(1, doc->State().params.zoom / 2);
                    if (auto* v = mw->ActiveView()) v->UpdateScrollRange();
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_VIEW_ZOOM_RESET: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.zoom = 1;
                    if (auto* v = mw->ActiveView()) v->UpdateScrollRange();
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_VIEW_RULER: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.showRuler = !doc->State().params.showRuler;
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_VIEW_MODE_BITMAP: mw->DoSetViewMode(bes::ViewMode::BitBitmap); break;
            case CMD_VIEW_MODE_HEX:    mw->DoSetViewMode(bes::ViewMode::Hex); break;
            case CMD_VIEW_MODE_CHAR:   mw->DoSetViewMode(bes::ViewMode::Char); break;
            case CMD_VIEW_MODE_PIXEL:  mw->DoSetViewMode(bes::ViewMode::Pixel); break;
            case CMD_VIEW_MODE_HEXCHAR: mw->DoSetViewMode(bes::ViewMode::HexChar); break;

            // ---- 处理 / 分析 ----
            case CMD_ANALYZE_PARAM:  mw->DoParamSet(); break;
            // 原版 DiedaiBtnClick 是空过程（仅 ret），保留为占位，不移动视图
            case CMD_ANALYZE_ITERATE: break;
            case CMD_ANALYZE_STEP: {
                if (auto* doc = mw->ActiveDoc()) {
                    // 原版单步按钮只切换单步/连续模式，不移动视图；仅对数据流输入有效
                    doc->State().singleStep = !doc->State().singleStep;
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_ANALYZE_GOTO:    mw->DoGoto(); break;
            case CMD_ANALYZE_SEARCH_UNIQUE: mw->DoSearchUnique(); break;
            case CMD_ANALYZE_SEARCH_FL:     mw->DoSearchFrameLength(); break;
            case CMD_ANALYZE_QUIT_SEARCH:   mw->DoQuitSearch(); break;
            case CMD_ANALYZE_FILTER:        mw->DoFilter(); break;


            // ---- 定制 ----
            case CMD_CUSTOM_COMPONENT: mw->DoComponentManage(); break;
            case CMD_CUSTOM_FAST_EXEC: mw->DoFastExecute(); break;
            case CMD_VIEW_REFRESH:     mw->DoRefresh(); break;

            case CMD_DATA_OUTPUT: mw->DoOutputSet(); break;

            // ---- 书签 ----
            case CMD_BOOKMARK_TOGGLE: mw->DoToggleBookmark(); break;
            case CMD_BOOKMARK_NEXT:   mw->DoNextBookmark(); break;
            case CMD_BOOKMARK_PREV:   mw->DoPrevBookmark(); break;
            case CMD_BOOKMARK_CLEAR:  mw->DoClearBookmarks(); break;

            // ---- 快捷操作 ----
            case CMD_SET_START: mw->DoSetStart(); break;

            // ---- 窗口排布（原版 [form+0x498]：全局状态，一次对全部视图生效）----
            case CMD_VIEW_PAGE:        mw->SetViewLayout(bes::ViewLayout::Paged);      break;
            case CMD_VIEW_HORIZ:       mw->SetViewLayout(bes::ViewLayout::Horizontal); break;
            case CMD_VIEW_VERT:        mw->SetViewLayout(bes::ViewLayout::Vertical);   break;
            case CMD_VIEW_GROUP_HORIZ: mw->SetViewLayout(bes::ViewLayout::GroupH);     break;
            case CMD_VIEW_GROUP_VERT:  mw->SetViewLayout(bes::ViewLayout::GroupV);     break;
            case CMD_VIEW_TITLE: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.showTitle = !doc->State().params.showTitle;
                    mw->UpdateAllViews();
                }
                break;
            }

            // ---- 纵轴显示 ----
            case CMD_VIEW_MODE_YFN:
            case CMD_VIEW_MODE_YFL:
            case CMD_VIEW_MODE_YFB: {
                if (auto* doc = mw->ActiveDoc()) {
                    if (id == CMD_VIEW_MODE_YFN)      doc->State().params.yAxis = bes::YAxisMode::YFN;
                    else if (id == CMD_VIEW_MODE_YFL) doc->State().params.yAxis = bes::YAxisMode::YFL;
                    else                              doc->State().params.yAxis = bes::YAxisMode::YFB;
                    if (auto* v = mw->ActiveView()) v->UpdateScrollRange();
                    mw->UpdateAllViews();
                }
                break;
            }

            // ---- XTAB/YTAB/TAB跟随 ----
            case CMD_VIEW_HTAB: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.showXTab = !doc->State().params.showXTab;
                    if (doc->State().params.xTab < 1) doc->State().params.xTab = 8;
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_VIEW_VTAB: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.showYTab = !doc->State().params.showYTab;
                    if (doc->State().params.yTab < 1) doc->State().params.yTab = 4;
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_TAB_LOCK: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.tabFollow = !doc->State().params.tabFollow;
                    mw->UpdateAllViews();
                }
                break;
            }

            // ---- 取反/倒序 ----
            case CMD_DATA_NOT: {
                if (auto* doc = mw->ActiveDoc()) {
                    doc->State().params.invert = !doc->State().params.invert;
                    mw->UpdateAllViews();
                }
                break;
            }
            case CMD_DATA_REVERSE: {
                if (auto* doc = mw->ActiveDoc()) {
                    // 原版「倒序」与参数设置「高位在前(0-7)」是同一选项
                    doc->State().params.highBitFirst = !doc->State().params.highBitFirst;
                    mw->UpdateAllViews();
                }
                break;
            }

            // ---- 联动浏览 / 工具栏 ----
            case CMD_VIEW_LINK:      mw->ToggleLinkBrowse(); break;
            case CMD_VIEW_GROUP:     mw->DoViewGroup();      break;
            case CMD_VIEW_EVENT:     mw->DoToggleEventPane(); break;

            case CMD_FILE_NEW:  mw->DoOpen(); break;
            case CMD_FILE_CLOSE: mw->CloseCurrentTab(); break;
            case CMD_WINDOW_NEW: mw->DuplicateCurrentTab(); break;

            case CMD_EDIT_UNDO:  mw->DoUndo(); break;
            case CMD_EDIT_REDO:  mw->DoRedo(); break;

            case CMD_LOOK_TRADITIONAL:
            case CMD_LOOK_VSNET:
            case CMD_LOOK_OFFICE:
                mw->DoLookStyle(id);
                break;

            // ---- 不等长帧查看 / 处理 ----
            case CMD_ANALYZE_VARFRAME:
                mw->DoVarFrame(false);
                break;
            case CMD_PROCESS_UNIQUE:
                mw->DoVarFrame(true);
                break;

            // ---- 构件相关（依赖插件） ----
            // 原版 TProcessForm 一个窗口覆盖 数据输入 / 数据类型 / 数据输出 三类行，
            // 故三个命令都打开同一个「构件数据流处理」。
            case CMD_IDS_DATA_IN:
            case CMD_IDS_FORMAT_IN:
            case CMD_IDS_DATA_OUT:
                mw->DoProcessFlow();
                break;

            default: break;
            }
        });
        m_actions.insert(d.id, a);
    }
}

QAction* Actions::operator[](int id) const { return Find(id); }
QAction* Actions::Find(int id) const { return m_actions.value(id, nullptr); }
int Actions::IdOf(const QAction* action) const
{
    for (auto it = m_actions.cbegin(); it != m_actions.cend(); ++it)
        if (it.value() == action) return it.key();
    return -1;
}
QList<QAction*> Actions::All() const { return m_actions.values(); }

void Actions::AddRecentFile(const QString&)
{
    // 最近文件菜单由 MainWindow 管理
}
