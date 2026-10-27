// MainWindow.cpp
#include "app/MainWindow.h"
#include "app/Actions.h"
#include "app/BesDoc.h"
#include "app/BesView.h"
#include "app/EventPane.h"
#include "app/GroupPane.h"
#include "app/ViewArea.h"
#include "app/BesApp.h"
#include "app/ToolbarIcons.h"
#include "dlg/DlgOutputSet.h"
#include "dlg/DlgParamSet.h"
#include "dlg/DlgPosition.h"
#include "dlg/DlgPrint.h"
#include "dlg/DlgFilter.h"
#include "dlg/DlgSearchUnique.h"
#include "dlg/DlgSearchFl.h"
#include "dlg/DlgComponent.h"
#include "dlg/DlgVarFrame.h"
#include "dlg/DlgComSelect.h"
#include "dlg/DlgProcess.h"
#include "dlg/DlgGroupItems.h"
#include "dlg/DlgAbout.h"
#include "dlg/DlgCustomize.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QSet>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabBar>
#include <QTime>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

using namespace bes;

namespace {
inline bool ByteUnitView(ViewMode v) {
    return v == ViewMode::Hex || v == ViewMode::Char ||
           v == ViewMode::Pixel || v == ViewMode::HexChar;
}
inline int UnitOf(ViewMode v) { return ByteUnitView(v) ? 8 : 1; }

// 原版格式栏「显示」下拉框只有 5 项（ViewModeComboBox，无位图）；
// 位图（原版 mode 7）只能由保存的视图流经魔数 0x1234ABCD 恢复，
// 本版保留为工具栏/菜单扩展，故不在下拉框中回显。
inline int ComboIndexFromView(ViewMode v)
{
    if (v == ViewMode::Pixel) return -1;
    if (v == ViewMode::HexChar) return 4;
    return static_cast<int>(v);
}
inline ViewMode ViewFromComboIndex(int idx)
{
    return idx == 4 ? ViewMode::HexChar : static_cast<ViewMode>(idx);
}
inline void FillViewModeCombo(QComboBox* c)
{
    c->addItems({
        QStringLiteral("比特位图"),
        QStringLiteral("等长帧比特字符"),
        QStringLiteral("等长帧十六进制"),
        QStringLiteral("字符方式"),
        QStringLiteral("十六进制字符对应"),
    });
}

// 构件端口在本版本中以临时文件承载，保证「输出到端口」链路可用。
class TempFilePort : public IOutputPort {
public:
    explicit TempFilePort(const QString& path)
    {
        m_file.open(path.toStdWString(), std::ios::binary | std::ios::trunc);
    }
    bool Write(const uint8_t* data, size_t len) override
    {
        if (!m_file.is_open()) return false;
        m_file.write(reinterpret_cast<const char*>(data),
                     static_cast<std::streamsize>(len));
        return m_file.good();
    }
    void Close() override { if (m_file.is_open()) m_file.close(); }
    bool IsOpen() const { return m_file.is_open(); }
private:
    std::ofstream m_file;
};

// 构件的「数据流输入端口」同样以临时文件承载（与 TempFilePort 对称）：
// 数据流引擎把选区字节原样写入，端口只收裸字节，数据类型/优先级不作为文件内容。
class FlowInputFile : public IFlowPort {
public:
    explicit FlowInputFile(const QString& path)
    {
        m_file.open(path.toStdWString(), std::ios::binary | std::ios::trunc);
    }
    bool Write(const uint8_t* data, size_t len) override
    {
        if (!m_file.is_open()) return false;
        m_file.write(reinterpret_cast<const char*>(data),
                     static_cast<std::streamsize>(len));
        return m_file.good();
    }
    void Close() override { if (m_file.is_open()) m_file.close(); }
    bool IsOpen() const { return m_file.is_open(); }
private:
    std::ofstream m_file;
};
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("BES 浏览/编辑/选取 1.0"));
    resize(1366, 800);

    m_area = new ViewArea(this);
    m_area->setTabsClosable(true);
    m_area->setMovable(true);
    m_area->setDocumentMode(true);
    setCentralWidget(m_area);
    connect(m_area, &ViewArea::currentChanged,
            this, &MainWindow::OnTabChanged);
    connect(m_area, &ViewArea::tabCloseRequested,
            this, &MainWindow::CloseTab);
    // 右键 tab：加入已有分组 / 新建分组（Tab 拖拽之外的等价操作）
    m_area->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_area->tabBar(), &QTabBar::customContextMenuRequested, this,
            [this](const QPoint& pos){
        const int idx = m_area->tabBar()->tabAt(pos);
        if (idx < 0) return;
        QMenu menu(this);
        QAction* aNew = menu.addAction(QStringLiteral("新建分组..."));
        QMenu* join = menu.addMenu(QStringLiteral("加入分组"));
        QList<QAction*> groupActs;
        for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
            QAction* a = join->addAction(it.key());
            a->setData(it.key());
            groupActs.push_back(a);
        }
        if (groupActs.isEmpty())
            join->setEnabled(false);
        QAction* chosen = menu.exec(m_area->tabBar()->mapToGlobal(pos));
        if (!chosen) return;
        if (chosen == aNew) {
            OnGroupAddRequested();
            return;
        }
        for (QAction* a : groupActs) {
            if (a == chosen) {
                QVector<int>& v = m_groups[a->data().toString()];
                if (!v.contains(idx)) v.push_back(idx);
                SyncGroupPane();
                break;
            }
        }
    });

    connect(m_area->tabBar(), &QTabBar::tabMoved, this, [this](int from, int to){
        if (from == to) return;
        for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
            for (int& idx : it.value()) {
                if (idx == from) {
                    idx = to;
                } else if (from < to && idx > from && idx <= to) {
                    --idx;
                } else if (from > to && idx >= to && idx < from) {
                    ++idx;
                }
            }
        }
        SyncGroupPane();
    });

    auto* scNext = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")), this);
    connect(scNext, &QShortcut::activated, this, [this]{
        const int n = m_area->count();
        if (n > 1) m_area->setCurrentIndex((m_area->currentIndex() + 1) % n);
    });
    auto* scPrev = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Tab")), this);
    connect(scPrev, &QShortcut::activated, this, [this]{
        const int n = m_area->count();
        if (n > 1) m_area->setCurrentIndex((m_area->currentIndex() - 1 + n) % n);
    });

    m_actions = new Actions(this, this);

    CreateToolBars();
    CreateMenuBar();
    CreateStatusBar();
    CreateDockPanes();
    WireSignals();

    RestoreSettings();
    LoadPluginList();

    UpdateMenuState();
    UpdateRecentMenu();
    SyncGroupPane();

    setAcceptDrops(true);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- 拖放
void MainWindow::dragEnterEvent(QDragEnterEvent* ev)
{
    if (ev->mimeData()->hasUrls())
        ev->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* ev)
{
    const QList<QUrl> urls = ev->mimeData()->urls();
    for (const QUrl& u : urls) {
        if (!u.isLocalFile()) continue;
        const QString path = u.toLocalFile();
        if (QFileInfo(path).isFile()) {
            OpenDocumentFile(path);
        } else if (QFileInfo(path).isDir()) {
            QDir dir(path);
            const QStringList files = dir.entryList(QDir::Files);
            for (const QString& f : files)
                OpenDocumentFile(dir.filePath(f));
        }
    }
    ev->acceptProposedAction();
}

// ---------------------------------------------------------------- 菜单
void MainWindow::CreateMenuBar()
{
    QMenuBar* mb = menuBar();
    mb->setNativeMenuBar(false);
    Actions& A = *m_actions;

    // 菜单分组来自 DFM：文件 = 新建 打开 保存 另存为 数据输出 | 打印 关闭 全部关闭 | 最近 | 退出
    QMenu* mFile = mb->addMenu(QStringLiteral("文件(&F)"));
    mFile->addAction(A[CMD_FILE_NEW]);
    mFile->addAction(A[CMD_FILE_OPEN]);
    mFile->addAction(A[CMD_FILE_SAVE]);
    mFile->addAction(A[CMD_FILE_SAVE_AS]);
    mFile->addAction(A[CMD_DATA_OUTPUT]);
    mFile->addSeparator();
    mFile->addAction(A[CMD_FILE_PRINT]);
    mFile->addAction(A[CMD_FILE_CLOSE]);
    mFile->addAction(A[CMD_FILE_CLOSE_ALL]);
    mFile->addSeparator();
    QMenu* mRecent = mFile->addMenu(QStringLiteral("最近打开文件(&R)"));
    mRecent->setObjectName("recentMenu");
    mFile->addSeparator();
    mFile->addAction(A[CMD_APP_EXIT]);

    // 编辑 = 进行编辑 | 剪切 复制 粘贴 撤销 重做 | 四种选取 选取参数 | 独特码查找 帧长查找
    QMenu* mEdit = mb->addMenu(QStringLiteral("编辑(&E)"));
    mEdit->addAction(A[CMD_EDIT_MODE]);
    mEdit->addSeparator();
    mEdit->addAction(A[CMD_EDIT_CUT]);
    mEdit->addAction(A[CMD_EDIT_COPY]);
    mEdit->addAction(A[CMD_EDIT_PASTE]);
    mEdit->addAction(A[CMD_EDIT_UNDO]);
    mEdit->addAction(A[CMD_EDIT_REDO]);
    mEdit->addSeparator();
    mEdit->addAction(A[CMD_SELECT_CONTINUOUS]);
    mEdit->addAction(A[CMD_SELECT_RECT]);
    mEdit->addAction(A[CMD_SELECT_ROW]);
    mEdit->addAction(A[CMD_SELECT_COL]);
    QMenu* mSel = mEdit->addMenu(QStringLiteral("选取参数"));
    mSel->addAction(A[CMD_SELECT_LOAD]);
    mSel->addAction(A[CMD_SELECT_SAVE]);
    mSel->addSeparator();
    mSel->addAction(A[CMD_SELECT_CLEAR_CURRENT]);
    mSel->addAction(A[CMD_SELECT_CLEAR_ALL]);
    mEdit->addSeparator();
    mEdit->addAction(A[CMD_ANALYZE_SEARCH_UNIQUE]);
    mEdit->addAction(A[CMD_ANALYZE_SEARCH_FL]);

    // 查看 = 联动 设为起点 | 标尺 取反 倒序 参数 不等长帧查看 | XTAB YTAB TAB跟随 纵轴显示 | 缩放 | 界面风格
    QMenu* mView = mb->addMenu(QStringLiteral("查看(&V)"));
    mView->addAction(A[CMD_VIEW_LINK]);
    mView->addAction(A[CMD_SET_START]);

    // 原版查看菜单内嵌格式栏的 起点/帧长/步进/显示 四个 TdxBar* 控件
    // （同一 ItemLinks 同时挂在格式栏与菜单里）；Qt 用镜像控件双向同步。
    auto addMenuSpin = [&](const QString& caption, QLineEdit*& ed, int width,
                           const QString& name, void (MainWindow::*slot)()) {
        QWidget* row = new QWidget(mView);
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(10, 2, 10, 2);
        h->setSpacing(8);
        QLabel* lbl = new QLabel(caption, row);
        lbl->setMinimumWidth(30);
        h->addWidget(lbl);
        h->addWidget(CreateSpinRow(row, ed, width, name, slot));
        QWidgetAction* wa = new QWidgetAction(mView);
        wa->setDefaultWidget(row);
        mView->addAction(wa);
    };
    addMenuSpin(QStringLiteral("起点"), m_menuEdStart, 64,
                QStringLiteral("menuEdStart"), &MainWindow::OnMenuStartEdited);
    addMenuSpin(QStringLiteral("帧长"), m_menuEdFrameLen, 72,
                QStringLiteral("menuEdFrameLen"), &MainWindow::OnMenuFrameLenEdited);
    addMenuSpin(QStringLiteral("步进"), m_menuEdStep, 48,
                QStringLiteral("menuEdStep"), &MainWindow::OnMenuStepEdited);
    {
        QWidget* row = new QWidget(mView);
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(10, 2, 10, 2);
        h->setSpacing(8);
        QLabel* lbl = new QLabel(QStringLiteral("显示"), row);
        lbl->setMinimumWidth(30);
        h->addWidget(lbl);
        m_menuCboViewMode = new QComboBox(row);
        m_menuCboViewMode->setObjectName(QStringLiteral("menuCboViewMode"));
        m_menuCboViewMode->setFixedWidth(150);
        FillViewModeCombo(m_menuCboViewMode);
        h->addWidget(m_menuCboViewMode);
        QWidgetAction* wa = new QWidgetAction(mView);
        wa->setDefaultWidget(row);
        mView->addAction(wa);
    }
    mView->addSeparator();
    mView->addAction(A[CMD_VIEW_RULER]);
    mView->addAction(A[CMD_DATA_NOT]);
    mView->addAction(A[CMD_DATA_REVERSE]);
    mView->addAction(A[CMD_ANALYZE_PARAM]);
    mView->addAction(A[CMD_ANALYZE_VARFRAME]);
    mView->addSeparator();
    mView->addAction(A[CMD_VIEW_HTAB]);
    mView->addAction(A[CMD_VIEW_VTAB]);
    mView->addAction(A[CMD_TAB_LOCK]);
    QMenu* mY = mView->addMenu(QStringLiteral("纵轴显示"));
    m_yAxisGroup = new QActionGroup(this);
    m_yAxisGroup->setExclusive(true);
    for (int id : { CMD_VIEW_MODE_YFN, CMD_VIEW_MODE_YFL, CMD_VIEW_MODE_YFB }) {
        QAction* a = A[id];
        a->setCheckable(true);
        m_yAxisGroup->addAction(a);
        mY->addAction(a);
    }
    mView->addSeparator();
    mView->addAction(A[CMD_VIEW_ZOOM_IN]);
    mView->addAction(A[CMD_VIEW_ZOOM_OUT]);
    mView->addAction(A[CMD_VIEW_ZOOM_RESET]);
    mView->addSeparator();
    QMenu* mLook = mView->addMenu(QStringLiteral("界面风格"));
    m_styleGroup = new QActionGroup(this);
    m_styleGroup->setExclusive(true);
    struct LookDef { int id; const char* text; };
    const LookDef looks[] = {
        { CMD_LOOK_TRADITIONAL, "传统风格" },
        { CMD_LOOK_VSNET,       "VS.NET风格" },
        { CMD_LOOK_OFFICE,      "Office2003" },
    };
    for (const LookDef& d : looks) {
        QAction* a = mLook->addAction(QString::fromUtf8(d.text));
        a->setCheckable(true);
        a->setData(d.id);
        m_styleGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, id = d.id]{ DoLookStyle(id); });
    }

    // 原版 处理(&P) 在 DFM 中 ItemLinks 为空；这里只放没有其它入口的命令。
    QMenu* mProc = mb->addMenu(QStringLiteral("处理(&P)"));
    mProc->addAction(A[CMD_PROCESS_UNIQUE]);
    mProc->addSeparator();
    mProc->addAction(A[CMD_ANALYZE_GOTO]);
    mProc->addAction(A[CMD_ANALYZE_QUIT_SEARCH]);

    // 原版 分析(&A) 只挂 DiedaiBtn「迭代」；显示模式与过滤位于格式栏，这里保留菜单入口。
    QMenu* mAna = mb->addMenu(QStringLiteral("分析(&A)"));
    mAna->addAction(A[CMD_ANALYZE_ITERATE]);
    mAna->addSeparator();
    mAna->addAction(A[CMD_VIEW_MODE_BITMAP]);
    mAna->addAction(A[CMD_VIEW_MODE_HEX]);
    mAna->addAction(A[CMD_VIEW_MODE_CHAR]);
    mAna->addAction(A[CMD_VIEW_MODE_PIXEL]);
    mAna->addAction(A[CMD_VIEW_MODE_HEXCHAR]);
    mAna->addSeparator();
    mAna->addAction(A[CMD_ANALYZE_FILTER]);

    m_viewModeGroup = new QActionGroup(this);
    m_viewModeGroup->setExclusive(true);
    for (int id : { CMD_VIEW_MODE_BITMAP, CMD_VIEW_MODE_HEX, CMD_VIEW_MODE_CHAR,
                    CMD_VIEW_MODE_PIXEL, CMD_VIEW_MODE_HEXCHAR }) {
        if (QAction* a = A[id]) {
            a->setCheckable(true);
            m_viewModeGroup->addAction(a);
        }
    }

    // 原版 dxBarSubItem6 识别(&R) 在 DFM 中 ItemLinks 为空；三项用于打开 TProcessForm。
    QMenu* mIds = mb->addMenu(QStringLiteral("识别(&R)"));
    mIds->addAction(A[CMD_IDS_DATA_IN]);
    mIds->addAction(A[CMD_IDS_FORMAT_IN]);
    mIds->addAction(A[CMD_IDS_DATA_OUT]);

    // 原版 CustomComSubItem 只有「管理...」「快速执行...」两项。
    QMenu* mCus = mb->addMenu(QStringLiteral("定制(&C)"));
    mCus->addAction(A[CMD_CUSTOM_COMPONENT]);
    mCus->addAction(A[CMD_CUSTOM_FAST_EXEC]);

    QMenu* mWin = mb->addMenu(QStringLiteral("窗口(&W)"));
    mWin->addAction(A[CMD_VIEW_PAGE]);
    mWin->addAction(A[CMD_VIEW_HORIZ]);
    mWin->addAction(A[CMD_VIEW_VERT]);
    mWin->addAction(A[CMD_VIEW_GROUP_HORIZ]);
    mWin->addAction(A[CMD_VIEW_GROUP_VERT]);
    mWin->addAction(A[CMD_VIEW_TITLE]);
    mWin->addSeparator();
    mWin->addAction(A[CMD_VIEW_GROUP]);
    A[CMD_VIEW_EVENT]->setCheckable(true);
    mWin->addAction(A[CMD_VIEW_EVENT]);

    // 工具栏显隐子菜单（原版窗口菜单的 dxBarToolbarsListItem1：逐条列出工具栏）
    m_toolbarMenu = mWin->addMenu(QStringLiteral("工具栏"));
    for (int i = 0; i < BarCount; ++i) {
        QAction* a = m_toolbarMenu->addAction(ToolBarName(i));
        a->setCheckable(true);
        a->setData(i);
        a->setObjectName(i == BarMain ? QStringLiteral("tb_main")
                        : i == BarFormat ? QStringLiteral("tb_format")
                                         : QStringLiteral("tb_quick"));
        QToolBar* bar = GetToolBar(i);
        a->setChecked(bar && !bar->isHidden());
        connect(a, &QAction::toggled, this, [this, i](bool on){ SetBarVisible(i, on); });
    }

    // 以下为 Qt 多 Tab 结构的补充项，原版 MDI 窗口菜单中没有对应命令。
    mWin->addSeparator();
    mWin->addAction(A[CMD_WINDOW_NEW]);
    QAction* aNext = mWin->addAction(QStringLiteral("下一个(&N)"));
    connect(aNext, &QAction::triggered, this, [this]{
        const int n = m_area->count();
        if (n > 1) m_area->setCurrentIndex((m_area->currentIndex() + 1) % n);
    });
    QAction* aPrev = mWin->addAction(QStringLiteral("上一个(&P)"));
    connect(aPrev, &QAction::triggered, this, [this]{
        const int n = m_area->count();
        if (n > 1) m_area->setCurrentIndex((m_area->currentIndex() - 1 + n) % n);
    });

    QMenu* mHelp = mb->addMenu(QStringLiteral("帮助(&H)"));
    mHelp->addAction(A[CMD_APP_ABOUT]);
}

// ---------------------------------------------------------------- 工具栏
QString MainWindow::ToolBarName(int bar)
{
    switch (bar) {
    case BarMain:   return QStringLiteral("主工具栏");
    case BarFormat: return QStringLiteral("格式栏");
    case BarQuick:  return QStringLiteral("快捷操作");
    }
    return QString();
}

QVector<int> MainWindow::DefaultToolBarItems(int bar)
{
    // 顺序取自 DFM 的 ItemLinks（主工具栏/格式栏/快捷操作），分隔符 = 原版 BeginGroup。
    if (bar == BarMain) {
        return { CMD_FILE_NEW, CMD_FILE_OPEN, CMD_FILE_SAVE, CMD_FILE_PRINT,
                 BarSeparator,
                 CMD_EDIT_CUT, CMD_EDIT_COPY, CMD_EDIT_PASTE, CMD_EDIT_UNDO, CMD_EDIT_REDO,
                 BarSeparator,
                 CMD_ANALYZE_STEP, CMD_VIEW_REFRESH,
                 BarSeparator,
                 CMD_VIEW_PAGE, CMD_VIEW_HORIZ, CMD_VIEW_VERT,
                 CMD_VIEW_GROUP_HORIZ, CMD_VIEW_GROUP_VERT, CMD_VIEW_TITLE };
    }
    if (bar == BarFormat) {
        return { CMD_VIEW_MODE_BITMAP, CMD_VIEW_MODE_HEX, CMD_VIEW_MODE_CHAR,
                 CMD_VIEW_MODE_PIXEL, CMD_ANALYZE_FILTER, CMD_DATA_NOT, CMD_DATA_REVERSE,
                 CMD_ANALYZE_PARAM,
                 BarSeparator,
                 CMD_VIEW_RULER, CMD_VIEW_HTAB, CMD_VIEW_VTAB, CMD_TAB_LOCK,
                 BarWidgetMarker,   // 纵轴显示下拉框（原版 YDispModeSubItem）
                 CMD_VIEW_ZOOM_IN, CMD_VIEW_ZOOM_OUT, CMD_VIEW_ZOOM_RESET };
    }
    if (bar == BarQuick) {
        return { CMD_VIEW_LINK, CMD_SET_START,
                 BarSeparator,
                 CMD_SELECT_CONTINUOUS, CMD_SELECT_RECT, CMD_SELECT_ROW, CMD_SELECT_COL,
                 CMD_SELECT_LOAD,
                 BarSeparator,
                 CMD_EDIT_MODE, CMD_DATA_OUTPUT, CMD_FILE_SAVE_AS,
                 BarSeparator,
                 CMD_ANALYZE_SEARCH_FL, CMD_ANALYZE_SEARCH_UNIQUE, CMD_ANALYZE_QUIT_SEARCH,
                 BarSeparator,
                 CMD_BOOKMARK_TOGGLE, CMD_BOOKMARK_NEXT, CMD_BOOKMARK_PREV, CMD_BOOKMARK_CLEAR,
                 BarSeparator,
                 CMD_ANALYZE_GOTO };
    }
    return {};
}

QToolBar* MainWindow::GetToolBar(int bar) const
{
    switch (bar) {
    case BarMain:   return m_mainBar;
    case BarFormat: return m_formatBar;
    case BarQuick:  return m_quickBar;
    }
    return nullptr;
}

QVector<int> MainWindow::ToolBarItems(int bar) const
{
    if (bar < 0 || bar >= BarCount) return {};
    return m_barItems[bar];
}

void MainWindow::RebuildToolBar(int bar)
{
    QToolBar* tb = GetToolBar(bar);
    if (!tb) return;

    for (QAction* a : m_barTempActions[bar]) {
        tb->removeAction(a);
        delete a;
    }
    m_barTempActions[bar].clear();
    for (QAction* a : m_barManaged[bar])
        tb->removeAction(a);
    m_barManaged[bar].clear();

    // 格式栏：BarWidgetMarker 之后的分组追加到栏尾，之前的分组插到纵轴下拉框前面。
    bool afterMarker = (bar != BarFormat);
    for (int id : m_barItems[bar]) {
        if (bar == BarFormat && id == BarWidgetMarker) {
            afterMarker = true;
            continue;
        }
        if (id == BarSeparator) {
            auto* sep = new QAction(tb);
            sep->setSeparator(true);
            if (bar == BarFormat && !afterMarker && m_formatYDispAction)
                tb->insertAction(m_formatYDispAction, sep);
            else
                tb->addAction(sep);
            m_barTempActions[bar] << sep;
            continue;
        }
        QAction* a = m_actions ? m_actions->Find(id) : nullptr;
        if (!a) continue;
        if (bar == BarFormat && !afterMarker && m_formatYDispAction)
            tb->insertAction(m_formatYDispAction, a);
        else
            tb->addAction(a);
        m_barManaged[bar] << a;
    }

    // 工具栏及其按钮都要拦截右键/双击（子控件上安装过滤不会重复生效）
    tb->installEventFilter(this);
    for (QWidget* w : tb->findChildren<QWidget*>())
        w->installEventFilter(this);
}

void MainWindow::SetToolBarItems(int bar, const QVector<int>& items)
{
    if (bar < 0 || bar >= BarCount) return;
    m_barItems[bar] = items;
    RebuildToolBar(bar);
}

void MainWindow::CreateToolBars()
{
    Actions& A = *m_actions;

    m_mainBar = addToolBar(QStringLiteral("主工具栏"));
    m_mainBar->setObjectName("mainBar");
    m_mainBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_quickBar = addToolBar(QStringLiteral("快捷操作"));
    m_quickBar->setObjectName("quickBar");
    m_quickBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_formatBar = addToolBar(QStringLiteral("格式栏"));
    m_formatBar->setObjectName("formatBar");
    // 原版格式栏为图在上、文字在下（DFM 中按钮同时带 ImageIndex 与 Caption）
    m_formatBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    CreateFormatBar();
    insertToolBarBreak(m_formatBar);

    for (int id : { CMD_VIEW_PAGE, CMD_VIEW_HORIZ, CMD_VIEW_VERT,
                    CMD_VIEW_GROUP_HORIZ, CMD_VIEW_GROUP_VERT, CMD_VIEW_TITLE,
                    CMD_VIEW_RULER, CMD_VIEW_HTAB, CMD_VIEW_VTAB, CMD_TAB_LOCK,
                    CMD_DATA_NOT, CMD_DATA_REVERSE, CMD_VIEW_LINK, CMD_ANALYZE_STEP,
                    CMD_EDIT_MODE })
        if (QAction* a = A[id]) a->setCheckable(true);

    for (int i = 0; i < BarCount; ++i) {
        m_barItems[i] = DefaultToolBarItems(i);
        RebuildToolBar(i);
    }
}

QWidget* MainWindow::CreateSpinRow(QWidget* parent, QLineEdit*& ed, int width,
                                   const QString& name, void (MainWindow::*slot)())
{
    ed = new QLineEdit(parent);
    ed->setFixedWidth(width);
    ed->setObjectName(name);
    QWidget* box = new QWidget(parent);
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(ed);
    QVBoxLayout* arrows = new QVBoxLayout;
    arrows->setContentsMargins(0, 0, 0, 0);
    arrows->setSpacing(0);
    QToolButton* up = new QToolButton(box);
    QToolButton* down = new QToolButton(box);
    up->setObjectName(name + QStringLiteral("Up"));
    down->setObjectName(name + QStringLiteral("Down"));
    up->setArrowType(Qt::UpArrow);
    down->setArrowType(Qt::DownArrow);
    for (QToolButton* b : { up, down }) {
        b->setAutoRaise(true);
        b->setAutoRepeat(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setFixedSize(16, 11);
        arrows->addWidget(b);
    }
    row->addLayout(arrows);

    auto bump = [this, ed, slot](int delta) {
        bool ok = false;
        qulonglong v = ed->text().trimmed().toULongLong(&ok);
        if (!ok) v = 0;
        if (delta < 0) {
            if (v > 0) --v;
        } else {
            ++v;
        }
        ed->setText(QString::number(v));
        (this->*slot)();
    };
    connect(up, &QToolButton::clicked, this, [bump]{ bump(1); });
    connect(down, &QToolButton::clicked, this, [bump]{ bump(-1); });
    return box;
}

void MainWindow::CreateFormatBar()
{
    auto addLbl = [&](const QString& t) {
        m_formatBar->addWidget(new QLabel(t, m_formatBar));
    };

    // 原版为 TdxBarSpinEdit；这里保持 64 位文本输入，另加一对箭头实现 +1/-1
    auto addSpin = [&](QLineEdit*& ed, int width, const QString& name,
                       void (MainWindow::*slot)()) {
        m_formatBar->addWidget(CreateSpinRow(m_formatBar, ed, width, name, slot));
    };

    addLbl(QStringLiteral("起点:"));
    addSpin(m_edStart, 64, QStringLiteral("edStart"), &MainWindow::OnStartEdited);

    addLbl(QStringLiteral("帧长:"));
    addSpin(m_edFrameLen, 72, QStringLiteral("edFrameLen"), &MainWindow::OnFrameLenEdited);
    m_edFrameLen->setToolTip(QStringLiteral("0 = 按屏宽自动（一屏宽为一帧）"));

    addLbl(QStringLiteral("步进:"));
    addSpin(m_edStep, 48, QStringLiteral("edStep"), &MainWindow::OnStepEdited);

    addLbl(QStringLiteral("显示:"));
    m_cboViewMode = new QComboBox(m_formatBar);
    m_cboViewMode->setObjectName(QStringLiteral("cboViewMode"));
    m_cboViewMode->setToolTip(QStringLiteral("显示模式"));
    m_cboViewMode->setFixedWidth(150);
    FillViewModeCombo(m_cboViewMode);
    m_cboViewMode->setCurrentIndex(2);
    m_formatBar->addWidget(m_cboViewMode);

    m_cboYDisp = new QComboBox(m_formatBar);
    m_cboYDisp->setObjectName(QStringLiteral("cboYDisp"));
    m_cboYDisp->setFixedWidth(76);
    m_cboYDisp->addItems({ QStringLiteral("YFN"), QStringLiteral("YFL"), QStringLiteral("YFB") });
    m_formatYDispAction = m_formatBar->addWidget(m_cboYDisp);
}

void MainWindow::CreateStatusBar()
{
    // 原版 StatusBar1（DFM tools/forms.dfm.txt 1187 行起）：三个面板
    // 0=文件 380px，1=光标/选区（Fixed=False，占据剩余宽度），2=插入/改写 30px 居中。
    QStatusBar* sb = statusBar();
    m_statusFile = new QLabel(this);
    m_statusFile->setObjectName("statusFile");
    m_statusFile->setFixedWidth(380);
    m_statusFile->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    sb->addWidget(m_statusFile);

    m_statusPos = new QLabel(this);
    m_statusPos->setObjectName("statusCursor");
    m_statusPos->setMinimumWidth(400);
    m_statusPos->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    sb->addWidget(m_statusPos, 1);

    // 最右侧 30px 面板：插入/改写指示（Insert 切换）
    m_statusIns = new QLabel(QStringLiteral("插入"), this);
    m_statusIns->setObjectName("statusInsert");
    m_statusIns->setFixedWidth(30);
    m_statusIns->setAlignment(Qt::AlignCenter);
    m_statusIns->setToolTip(QStringLiteral("插入/改写（Insert 切换）：粘贴时改写覆盖光标处字节，插入则后移后续字节"));
    sb->addWidget(m_statusIns);
}

void MainWindow::ToggleInsertMode()
{
    m_insertMode = !m_insertMode;
    if (m_statusIns)
        m_statusIns->setText(m_insertMode ? QStringLiteral("插入")
                                          : QStringLiteral("改写"));
}

void MainWindow::CreateDockPanes()
{
    m_groupPane = new GroupPane(this);
    m_groupPane->setObjectName(QStringLiteral("groupPane"));
    connect(m_groupPane, &GroupPane::WindowDroppedOnGroup,
            this, &MainWindow::MoveTabToGroup);
    connect(m_groupPane, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* it, int){
        const int idx = it->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_area->count())
            m_area->setCurrentIndex(idx);
    });
    connect(m_groupPane, &GroupPane::AddGroupRequested,
            this, &MainWindow::OnGroupAddRequested);
    connect(m_groupPane, &GroupPane::DeleteGroupRequested,
            this, &MainWindow::OnGroupDeleteRequested);
    connect(m_groupPane, &GroupPane::CloseGroupRequested,
            this, &MainWindow::OnGroupCloseRequested);

    m_groupDock = new QDockWidget(QStringLiteral("窗口分组"), this);
    m_groupDock->setObjectName("groupDock");
    m_groupDock->setWidget(m_groupPane);
    m_groupDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_groupDock);

    m_eventPane = new EventPane(this);
    m_eventDock = new QDockWidget(QStringLiteral("事件"), this);
    m_eventDock->setObjectName("eventDock");
    m_eventDock->setWidget(m_eventPane);
    addDockWidget(Qt::BottomDockWidgetArea, m_eventDock);
}

void MainWindow::WireSignals()
{
    connect(m_edStart, &QLineEdit::editingFinished, this, &MainWindow::OnStartEdited);
    connect(m_edFrameLen, &QLineEdit::editingFinished, this, &MainWindow::OnFrameLenEdited);
    connect(m_edStep, &QLineEdit::editingFinished, this, &MainWindow::OnStepEdited);
    connect(m_cboViewMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnViewModeCombo);
    connect(m_menuEdStart, &QLineEdit::editingFinished, this, &MainWindow::OnMenuStartEdited);
    connect(m_menuEdFrameLen, &QLineEdit::editingFinished, this, &MainWindow::OnMenuFrameLenEdited);
    connect(m_menuEdStep, &QLineEdit::editingFinished, this, &MainWindow::OnMenuStepEdited);
    connect(m_menuCboViewMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnViewModeCombo);
    connect(m_cboYDisp, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnYDispCombo);
}

// ---------------------------------------------------------------- 设置
void MainWindow::RestoreSettings()
{
    QSettings st;
    restoreGeometry(st.value(QStringLiteral("win/geometry")).toByteArray());
    restoreState(st.value(QStringLiteral("win/state")).toByteArray());

    m_linkBrowse = st.value(QStringLiteral("view/link"), false).toBool();
    m_styleId = st.value(QStringLiteral("view/style"), CMD_LOOK_TRADITIONAL).toInt();
    ApplyStyle(m_styleId);

    // 工具栏内容（原版 dxBarManager1 的 Bars/ItemLinks，运行期可定制）
    static const char* const barKeys[BarCount] = { "view/barMain", "view/barFormat", "view/barQuick" };
    for (int i = 0; i < BarCount; ++i) {
        QVector<int> items;
        const QString text = st.value(QString::fromLatin1(barKeys[i])).toString();
        for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            bool ok = false;
            const int id = part.toInt(&ok);
            if (ok) items << id;
        }
        m_barItems[i] = items.isEmpty() ? DefaultToolBarItems(i) : items;
        RebuildToolBar(i);
    }
    const bool showMain = st.value(QStringLiteral("view/tbMain"), true).toBool();
    const bool showFmt  = st.value(QStringLiteral("view/tbFormat"), true).toBool();
    const bool showQuick= st.value(QStringLiteral("view/tbQuick"), true).toBool();
    SetBarVisible(BarMain, showMain);
    SetBarVisible(BarFormat, showFmt);
    SetBarVisible(BarQuick, showQuick);
    if (m_groupDock)
        m_groupDock->setVisible(st.value(QStringLiteral("view/groupDock"), true).toBool());
    if (m_eventDock)
        m_eventDock->setVisible(st.value(QStringLiteral("view/eventDock"), true).toBool());

    m_host.Opts().cacheMB = st.value(QStringLiteral("plugin/cacheMB"), 16).toInt();
    m_host.Opts().blockKB = st.value(QStringLiteral("plugin/blockKB"), 64).toInt();
    m_host.Opts().tempDir = st.value(QStringLiteral("plugin/tempDir")).toString().toStdWString();
    UpdateMenuState();
}

void MainWindow::SaveSettings()
{
    QSettings st;
    st.setValue(QStringLiteral("win/geometry"), saveGeometry());
    st.setValue(QStringLiteral("win/state"), saveState());
    st.setValue(QStringLiteral("view/link"), m_linkBrowse);
    st.setValue(QStringLiteral("view/style"), m_styleId);
    auto joinItems = [](const QVector<int>& items) {
        QStringList parts;
        for (int id : items) parts << QString::number(id);
        return parts.join(QLatin1Char(','));
    };
    st.setValue(QStringLiteral("view/barMain"), joinItems(m_barItems[BarMain]));
    st.setValue(QStringLiteral("view/barFormat"), joinItems(m_barItems[BarFormat]));
    st.setValue(QStringLiteral("view/barQuick"), joinItems(m_barItems[BarQuick]));
    st.setValue(QStringLiteral("view/tbMain"), !m_mainBar->isHidden());
    st.setValue(QStringLiteral("view/tbFormat"), !m_formatBar->isHidden());
    st.setValue(QStringLiteral("view/tbQuick"), !m_quickBar->isHidden());
    if (m_groupDock)
        st.setValue(QStringLiteral("view/groupDock"), m_groupDock->isVisible());
    if (m_eventDock)
        st.setValue(QStringLiteral("view/eventDock"), m_eventDock->isVisible());
    st.setValue(QStringLiteral("plugin/cacheMB"), m_host.Opts().cacheMB);
    st.setValue(QStringLiteral("plugin/blockKB"), m_host.Opts().blockKB);
    st.setValue(QStringLiteral("plugin/tempDir"), QString::fromStdWString(m_host.Opts().tempDir));
}

void MainWindow::LoadPluginList()
{
    QSettings st;
    const QStringList dlls = st.value(QStringLiteral("plugins/dlls")).toStringList();
    for (const QString& dll : dlls) {
        std::wstring err;
        if (!m_host.RegisterDll(dll.toStdWString(), &err) && m_eventPane) {
            m_eventPane->AddEvent({ QStringLiteral("构件"), QStringLiteral("注册失败"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QFileInfo(dll).fileName() + QStringLiteral(": ") +
                                    QString::fromStdWString(err) });
        }
    }
}

void MainWindow::SavePluginList()
{
    QSettings st;
    QStringList list;
    for (const std::wstring& p : m_host.DllPaths())
        list << QString::fromStdWString(p);
    st.setValue(QStringLiteral("plugins/dlls"), list);
}

void MainWindow::ApplyStyle(int styleId)
{
    m_styleId = styleId;
    switch (styleId) {
    case CMD_LOOK_VSNET:
        setStyleSheet(QStringLiteral(
            "QToolBar { border: 1px solid #7a96df; background: #d6dff7; spacing: 3px; }"
            "QToolBar::handle { background: #b8c8e8; }"
            "QMenuBar { background: #d6dff7; }"
            "QDockWidget::title { background: #c8d6f0; padding: 3px; }"));
        break;
    case CMD_LOOK_OFFICE:
        setStyleSheet(QStringLiteral(
            "QToolBar { border: none; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            " stop:0 #fffcf4, stop:1 #e8c689); spacing: 3px; }"
            "QMenuBar { background: #fffcf4; }"
            "QDockWidget::title { background: #e8c689; padding: 3px; }"));
        break;
    default:
        setStyleSheet(QString());
        break;
    }
    if (m_styleGroup) {
        for (QAction* a : m_styleGroup->actions())
            a->setChecked(a->data().toInt() == styleId);
    }
}

// ---------------------------------------------------------------- 文档
int MainWindow::ActiveIndex() const { return m_area->currentIndex(); }

BesView* MainWindow::ActiveView() const
{
    return dynamic_cast<BesView*>(m_area->currentWidget());
}

BesDoc* MainWindow::ActiveDoc() const
{
    if (auto* v = ActiveView())
        return v->Doc();
    return nullptr;
}

uint64_t MainWindow::EffectiveFrameLen() const
{
    if (auto* v = ActiveView())
        return v->FrameLenBits();
    return 0;
}

BesDoc* MainWindow::OpenDocumentFile(const QString& path)
{
    if (path.isEmpty())
        return nullptr;

    const QFileInfo fi(path);
    for (int i = 0; i < m_area->count(); ++i) {
        auto* v = dynamic_cast<BesView*>(m_area->widget(i));
        BesDoc* d = v ? v->Doc() : nullptr;
        if (d && QFileInfo(d->FilePath()) == fi) {
            m_area->setCurrentIndex(i);
            return d;
        }
    }

    auto* doc = new BesDoc(this);
    QString err;
    if (!doc->OpenFile(path, &err)) {
        QMessageBox::critical(this, QStringLiteral("BES"),
                              err.isEmpty() ? QStringLiteral("打开文件失败！") : err);
        delete doc;
        return nullptr;
    }

    auto* view = new BesView;
    view->SetDoc(doc);
    ApplyLayoutTo(doc);   // 排布是全局的（[form+0x498]），新视图沿用当前排布
    m_area->addTab(view, fi.fileName());
    m_area->setCurrentWidget(view);

    connect(doc, &BesDoc::Changed, this, [this]{ UpdateStatusText(); });
    UpdateMenuState();
    UpdateStatusText();
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("文件"), QStringLiteral("打开"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                fi.absoluteFilePath() });
    AddRecentFile(path);
    SyncGroupPane();
    return doc;
}

void MainWindow::OnTabChanged(int)
{
    ApplyArrangement();
    UpdateMenuState();
    UpdateStatusText();
    SyncGroupPane();
}

// ---------------------------------------------------------------- 窗口排布
// 原版五个按钮先写主窗体全局字段 [form+0x498]，再遍历全部停靠站点逐个重排：
// PageBtnClick@0x408B20 置 0（不改对齐，只恢复单页）、HorizBtnClick@0x408B94
// 置 1 并把每个站点控件重新插回 RootPanel、VertBtnClick@0x408C24 置 2、
// PageHorizBtnClick@0x408CB4 置 3（按组内子项逐个重排）、PageVertBtnClick@
// 0x408E2C 置 4。因此排布是「全部视图」级别的，而不是当前文档的绘制方式。
void MainWindow::ApplyLayoutTo(BesDoc* doc)
{
    if (!doc) return;
    // 排布只重插停靠站点（0x544BC8），不碰 XTAB/YTAB 开关与线宽。
    doc->State().params.layout = m_viewLayout;
}

void MainWindow::SetViewLayout(bes::ViewLayout l)
{
    m_viewLayout = l;
    for (int i = 0; i < m_area->count(); ++i) {
        auto* v = dynamic_cast<BesView*>(m_area->widget(i));
        if (v) ApplyLayoutTo(v->Doc());
    }
    ApplyArrangement();
    UpdateAllViews();
    UpdateMenuState();
}

void MainWindow::ApplyArrangement()
{
    if (!m_area) return;
    QVector<int> shown;
    if (m_viewLayout == bes::ViewLayout::GroupH ||
        m_viewLayout == bes::ViewLayout::GroupV) {
        for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
            if (it.value().contains(m_area->currentIndex())) {
                shown = it.value();
                break;
            }
        }
    }
    m_area->setShownViews(shown);
    m_area->setArrangement(m_viewLayout);
}

int MainWindow::VisibleViewCount() const
{
    return m_area ? m_area->visibleViewCount() : 0;
}

void MainWindow::CloseTab(int idx)
{
    if (idx < 0 || idx >= m_area->count()) return;
    auto* view = dynamic_cast<BesView*>(m_area->widget(idx));
    BesDoc* doc = view ? view->Doc() : nullptr;

    m_area->removeTab(idx);
    delete view;   // 析构时从文档的视图列表移除

    if (doc && doc->Views().isEmpty())
        doc->deleteLater();

    AdjustGroupsAfterClose(idx);
    ApplyArrangement();
    UpdateMenuState();
    UpdateStatusText();
    SyncGroupPane();
}

void MainWindow::AdjustGroupsAfterClose(int idx)
{
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        QVector<int> out;
        for (int v : it.value()) {
            if (v == idx) continue;
            out.push_back(v > idx ? v - 1 : v);
        }
        it.value() = out;
    }
}

void MainWindow::CloseCurrentTab()
{
    CloseTab(m_area->currentIndex());
}

void MainWindow::DoCloseAll()
{
    if (m_area->count() == 0) return;
    if (QMessageBox::question(this, QStringLiteral("BES"),
                              QStringLiteral("关闭该组所有窗口吗?")) != QMessageBox::Yes)
        return;
    while (m_area->count() > 0)
        CloseTab(m_area->count() - 1);
    m_groups.clear();
    SyncGroupPane();
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    SaveSettings();
    SavePluginList();
    while (m_area->count() > 0) {
        auto* view = dynamic_cast<BesView*>(m_area->widget(0));
        BesDoc* doc = view ? view->Doc() : nullptr;
        m_area->removeTab(0);
        delete view;
        if (doc && doc->Views().isEmpty())
            doc->deleteLater();
    }
    QMainWindow::closeEvent(ev);
}

// ---------------------------------------------------------------- 分组
void MainWindow::SyncGroupPane()
{
    if (!m_groupPane) return;
    m_groupPane->clear();

    m_groupPane->AddGroup(QStringLiteral("数据窗口"));
    for (int i = 0; i < m_area->count(); ++i)
        m_groupPane->AddToGroup(QStringLiteral("数据窗口"), m_area->tabText(i), i);

    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        if (it.key() == QStringLiteral("数据窗口")) continue;
        m_groupPane->AddGroup(it.key());
        for (int idx : it.value()) {
            if (idx >= 0 && idx < m_area->count())
                m_groupPane->AddToGroup(it.key(), m_area->tabText(idx), idx);
        }
    }

    const int cur = m_area->currentIndex();
    for (int i = 0; i < m_groupPane->topLevelItemCount(); ++i) {
        QTreeWidgetItem* g = m_groupPane->topLevelItem(i);
        for (int j = 0; j < g->childCount(); ++j) {
            QTreeWidgetItem* c = g->child(j);
            QFont f = m_groupPane->font();
            f.setBold(c->data(0, Qt::UserRole).toInt() == cur);
            c->setFont(0, f);
        }
    }
    m_groupPane->expandAll();
}

bool MainWindow::CreateGroup(const QString& name, const QVector<int>& tabs)
{
    if (name.isEmpty() || name == QStringLiteral("数据窗口")) return false;
    QVector<int> members;
    for (int idx : tabs) {
        if (idx >= 0 && idx < m_area->count() && !members.contains(idx))
            members.push_back(idx);
    }
    std::sort(members.begin(), members.end());
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        if (it.key() == name) continue;
        for (int idx : members) it.value().removeAll(idx);
    }
    m_groups[name] = members;
    SyncGroupPane();
    ApplyArrangement();
    return true;
}

void MainWindow::MoveTabToGroup(int idx, const QString& group)
{
    if (idx < 0 || idx >= m_area->count() || group.isEmpty()) return;
    if (group == QStringLiteral("数据窗口")) {
        // 拖回「数据窗口」：从所有自定义分组移出
        for (auto it = m_groups.begin(); it != m_groups.end(); ++it)
            it.value().removeAll(idx);
    } else {
        if (!m_groups.contains(group)) return;   // 原版一个窗口只属于一个停靠分组
        for (auto it = m_groups.begin(); it != m_groups.end(); ++it)
            if (it.key() != group) it.value().removeAll(idx);
        QVector<int>& members = m_groups[group];
        if (!members.contains(idx)) {
            members.push_back(idx);
            std::sort(members.begin(), members.end());
        }
    }
    SyncGroupPane();
    ApplyArrangement();
}

void MainWindow::OnGroupAddRequested()
{
    QStringList names;
    for (int i = 0; i < m_area->count(); ++i)
        names << m_area->tabText(i);
    const QString def = QStringLiteral("分组%1").arg(m_groups.size() + 1);
    DlgGroupItems dlg(this, names, def);
    if (dlg.exec() == QDialog::Accepted)
        CreateGroup(dlg.GroupName(), dlg.SelectedTabs());
}

void MainWindow::OnGroupDeleteRequested(const QString& group)
{
    if (group == QStringLiteral("数据窗口")) return;
    m_groups.remove(group);
    SyncGroupPane();
    ApplyArrangement();
}

void MainWindow::OnGroupCloseRequested(const QString& group)
{
    if (!m_groups.contains(group)) return;
    QVector<int> idxs = m_groups.value(group);
    std::sort(idxs.begin(), idxs.end(), std::greater<int>());
    for (int idx : idxs)
        CloseTab(idx);
    m_groups.remove(group);
    SyncGroupPane();
}

// ---------------------------------------------------------------- 状态
void MainWindow::UpdateStatusText()
{
    BesDoc* d = ActiveDoc();
    if (!d) {
        m_statusPos->clear();
        m_statusFile->clear();
        return;
    }
    const DocState& st = d->State();
    BesView* v = ActiveView();
    const uint64_t fl = v ? v->FrameLenBits() : d->FrameLenBits(80);
    const int bpc = v ? v->CellBits() : 8;
    const uint64_t totalBits = d->Map().SizeBits();
    const uint64_t cursor = d->CursorBit();
    const uint64_t frameCount = (fl && totalBits > st.frame.startBit)
            ? (totalBits - st.frame.startBit + fl - 1) / fl : 0;
    const uint64_t frameIdx = (fl && cursor >= st.frame.startBit)
            ? (cursor - st.frame.startBit) / fl : 0;
    const uint64_t colBits = (fl && cursor >= st.frame.startBit)
            ? (cursor - st.frame.startBit) % fl : 0;
    const uint64_t distBits = cursor >= st.frame.startBit ? cursor - st.frame.startBit : 0;

    const int unit = UnitOf(st.params.view);
    const bool byteUnit = (unit == 8);

    m_statusFile->setText(QStringLiteral("文件: %1").arg(d->FilePath()));

    // 面板 1 只显示一种文本：有选区时是选取统计，否则是光标标记（原版 CursorInfoLabel）
    if (!st.sel.Empty()) {
        QString text;
        switch (st.sel.Mode()) {
        case SelectMode::Continuous: {
            const uint64_t n = st.sel.SelectedBitsEx(fl, bpc, frameCount);
            text = byteUnit
                ? QStringLiteral("选取了 %1 字节").arg(static_cast<qulonglong>(n / 8))
                : QStringLiteral("选取了 %1 比特").arg(static_cast<qulonglong>(n));
            break;
        }
        case SelectMode::Rect: {
            uint64_t rows = 0, cols = 0;
            for (const RectSel& r : st.sel.Rects()) {
                rows += r.frame1 >= r.frame0 ? r.frame1 - r.frame0 + 1 : 0;
                cols += r.cell1 >= r.cell0 ? r.cell1 - r.cell0 + 1 : 0;
            }
            text = QStringLiteral("选取了 %1行×%2列")
                       .arg(static_cast<qulonglong>(rows))
                       .arg(static_cast<qulonglong>(cols));
            break;
        }
        case SelectMode::Row:
            // 比特视图「选取了 %d 行」、字节视图「选取了 %d行」（原版两套字符串）
            text = byteUnit
                ? QStringLiteral("选取了 %1行").arg(static_cast<qulonglong>(st.sel.Rows().size()))
                : QStringLiteral("选取了 %1 行").arg(static_cast<qulonglong>(st.sel.Rows().size()));
            break;
        case SelectMode::Col:
            text = QStringLiteral("选取了 %1 列").arg(static_cast<qulonglong>(st.sel.Cols().size()));
            break;
        default:
            break;
        }
        if (!text.isEmpty()) {
            m_statusPos->setText(text);
            return;
        }
    }

    const QString unitWord = byteUnit ? QStringLiteral("字节") : QStringLiteral("比特");
    QString marker;
    if (byteUnit) {
        // 字节视图（Hex/字符/位图/十六进制字符）的 CursorInfoLabel 只报行/列：
        // 「标记: 第<行>行 <列>列」（0x47FA88/0x4803CC），可选「 距离[...]」。
        marker = QStringLiteral("标记: 第%1行 %2列")
                     .arg(static_cast<qulonglong>(frameIdx))
                     .arg(static_cast<qulonglong>(colBits / unit));
    } else {
        // 比特视图报绝对起点/光标比特位置 + 行/列（0x442D84，默认文本
        // 「标记: 文件第0比特 第0比特 0行 0列」）。
        marker = QStringLiteral("标记: 文件第%1%2 第%3%4 %5行 %6列")
                     .arg(static_cast<qulonglong>(st.frame.startBit / unit))
                     .arg(unitWord)
                     .arg(static_cast<qulonglong>(cursor / unit))
                     .arg(unitWord)
                     .arg(static_cast<qulonglong>(frameIdx))
                     .arg(static_cast<qulonglong>(colBits / unit));
    }
    if (distBits > 0)
        marker += QStringLiteral(" 距离[%1]").arg(static_cast<qulonglong>(distBits / unit));
    m_statusPos->setText(marker);
}

void MainWindow::UpdateMenuState()
{
    BesDoc* d = ActiveDoc();
    const bool has = d != nullptr;

    if (has) {
        const DocState& st = d->State();
        QSignalBlocker b1(m_edStart), b2(m_edFrameLen), b3(m_edStep),
                       b4(m_cboViewMode), b5(m_cboYDisp),
                       b6(m_menuEdStart), b7(m_menuEdFrameLen), b8(m_menuEdStep),
                       b9(m_menuCboViewMode);
        const int unit = UnitOf(st.params.view);
        const QString startText = QString::number(st.frame.startBit / unit);
        const QString frameText = QString::number(st.frame.frameLen / unit);
        const QString stepText  = QString::number(std::max<uint64_t>(1, st.frame.step / unit));
        const QString unitTip = ByteUnitView(st.params.view)
            ? QStringLiteral("单位：字节（0 = 按屏宽自动）")
            : QStringLiteral("单位：比特（0 = 按屏宽自动）");
        m_edStart->setText(startText);
        m_menuEdStart->setText(startText);
        m_edFrameLen->setText(frameText);
        m_menuEdFrameLen->setText(frameText);
        m_edStep->setText(stepText);
        m_menuEdStep->setText(stepText);
        m_edFrameLen->setToolTip(unitTip);
        m_menuEdFrameLen->setToolTip(unitTip);
        m_cboViewMode->setCurrentIndex(ComboIndexFromView(st.params.view));
        m_menuCboViewMode->setCurrentIndex(ComboIndexFromView(st.params.view));
        m_cboYDisp->setCurrentIndex(static_cast<int>(st.params.yAxis));

        struct Chk { int id; bool on; };
        const Chk chks[] = {
            { CMD_VIEW_PAGE,        m_viewLayout == ViewLayout::Paged },
            { CMD_VIEW_HORIZ,       m_viewLayout == ViewLayout::Horizontal },
            { CMD_VIEW_VERT,        m_viewLayout == ViewLayout::Vertical },
            { CMD_VIEW_GROUP_HORIZ, m_viewLayout == ViewLayout::GroupH },
            { CMD_VIEW_GROUP_VERT,  m_viewLayout == ViewLayout::GroupV },
            { CMD_VIEW_TITLE,       st.params.showTitle },
            { CMD_VIEW_RULER,       st.params.showRuler },
            { CMD_VIEW_HTAB,        st.params.showXTab },
            { CMD_VIEW_VTAB,        st.params.showYTab },
            { CMD_TAB_LOCK,         st.params.tabFollow },
            { CMD_DATA_NOT,         st.params.invert },
            { CMD_DATA_REVERSE,     st.params.highBitFirst },
            { CMD_VIEW_LINK,        m_linkBrowse },
            { CMD_ANALYZE_STEP,     st.singleStep },
            // 选取模式按钮：原版 bsChecked + GroupIndex=2 + AllowAllUp
            { CMD_SELECT_CONTINUOUS, st.sel.Mode() == SelectMode::Continuous },
            { CMD_SELECT_RECT,       st.sel.Mode() == SelectMode::Rect },
            { CMD_SELECT_ROW,        st.sel.Mode() == SelectMode::Row },
            { CMD_SELECT_COL,        st.sel.Mode() == SelectMode::Col },
            { CMD_VIEW_MODE_BITMAP, st.params.view == ViewMode::BitBitmap },
            { CMD_VIEW_MODE_HEX,    st.params.view == ViewMode::Hex },
            { CMD_VIEW_MODE_CHAR,   st.params.view == ViewMode::Char },
            { CMD_VIEW_MODE_PIXEL,  st.params.view == ViewMode::Pixel },
            { CMD_VIEW_MODE_HEXCHAR, st.params.view == ViewMode::HexChar },
            { CMD_VIEW_MODE_YFN,    st.params.yAxis == YAxisMode::YFN },
            { CMD_VIEW_MODE_YFL,    st.params.yAxis == YAxisMode::YFL },
            { CMD_VIEW_MODE_YFB,    st.params.yAxis == YAxisMode::YFB },
            { CMD_EDIT_MODE,        st.editMode },
        };
        for (const Chk& c : chks)
            if (QAction* a = m_actions->Find(c.id))
                a->setChecked(c.on);
    }

    if (QAction* a = m_actions->Find(CMD_VIEW_GROUP))
        a->setChecked(m_groupDock && m_groupDock->isVisible());
    if (QAction* a = m_actions->Find(CMD_VIEW_EVENT))
        a->setChecked(m_eventDock && m_eventDock->isVisible());

    static const int needDoc[] = {
        CMD_FILE_SAVE, CMD_FILE_SAVE_AS, CMD_FILE_PRINT,
        CMD_SELECT_CONTINUOUS, CMD_SELECT_RECT, CMD_SELECT_ROW, CMD_SELECT_COL,
        CMD_SELECT_LOAD, CMD_SELECT_SAVE, CMD_SELECT_CLEAR_CURRENT, CMD_SELECT_CLEAR_ALL,
        CMD_VIEW_MODE_BITMAP, CMD_VIEW_MODE_HEX, CMD_VIEW_MODE_CHAR,
        CMD_VIEW_MODE_PIXEL, CMD_VIEW_MODE_HEXCHAR,
        CMD_ANALYZE_PARAM, CMD_ANALYZE_ITERATE,
        CMD_ANALYZE_STEP,
        CMD_ANALYZE_GOTO, CMD_ANALYZE_SEARCH_UNIQUE, CMD_ANALYZE_SEARCH_FL,
        CMD_ANALYZE_FILTER, CMD_DATA_OUTPUT, CMD_SET_START,
        CMD_BOOKMARK_TOGGLE, CMD_BOOKMARK_NEXT, CMD_BOOKMARK_PREV,
        CMD_BOOKMARK_CLEAR,
        CMD_VIEW_ZOOM_IN, CMD_VIEW_ZOOM_OUT, CMD_VIEW_ZOOM_RESET, CMD_VIEW_RULER,
        CMD_ANALYZE_VARFRAME, CMD_PROCESS_UNIQUE, CMD_ANALYZE_QUIT_SEARCH,
    };
    for (int id : needDoc)
        if (QAction* a = m_actions->Find(id))
            a->setEnabled(has);

    // 剪切/粘贴/撤销/重做需要引擎编辑态（原版 editIface 接口）
    const bool editing = has && d->InEdit();
    auto setEnabled = [this](int id, bool on) {
        if (QAction* a = m_actions->Find(id)) a->setEnabled(on);
    };
    setEnabled(CMD_EDIT_CUT,   editing);
    setEnabled(CMD_EDIT_PASTE, editing);
    setEnabled(CMD_EDIT_COPY,  has);
    setEnabled(CMD_EDIT_UNDO,  editing && d->Editor().CanUndo());
    setEnabled(CMD_EDIT_REDO,  editing && d->Editor().CanRedo());
    setEnabled(CMD_EDIT_MODE,  has);
    // 原版 EditBtnClick 进入编辑时把格式栏「显示」下拉框（ViewModeComboBox）Enabled:=False，
    // 退出编辑的各成功路径再置回 True；该控件设计期即为 False，无文档时同样禁用。
    m_cboViewMode->setEnabled(has && !editing);
    m_menuCboViewMode->setEnabled(has && !editing);
}

void MainWindow::UpdateAllViews()
{
    for (int i = 0; i < m_area->count(); ++i) {
        if (auto* v = dynamic_cast<BesView*>(m_area->widget(i))) {
            v->UpdateScrollRange();
            v->viewport()->update();
        }
    }
    UpdateStatusText();
    UpdateMenuState();
}

void MainWindow::ShowMessage(const QString& text, const QString& caption)
{
    QMessageBox::information(this, caption.isEmpty() ? QStringLiteral("BES") : caption, text);
}

// ---------------------------------------------------------------- 格式栏槽
void MainWindow::OnStartEdited()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    bool ok = false;
    uint64_t v = m_edStart->text().toULongLong(&ok);
    if (ok) {
        v *= static_cast<uint64_t>(UnitOf(d->State().params.view));
        if (d->State().frame.startBit != v) {
            d->State().frame.bVariable = false;
            d->State().varFrames.Clear();
        }
        d->State().frame.startBit = v;
        d->State().filterActive = false;
        d->State().visibleFrames.clear();
        d->UpdateAllViews();
    }
    UpdateAllViews();
}

void MainWindow::OnFrameLenEdited()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    bool ok = false;
    uint64_t v = m_edFrameLen->text().toULongLong(&ok);
    if (ok) {
        v *= static_cast<uint64_t>(UnitOf(d->State().params.view));
        if (d->State().frame.frameLen != v) {
            d->State().frame.bVariable = false;
            d->State().varFrames.Clear();
        }
        d->State().frame.frameLen = v;
        d->State().frame.SanitizeStep();
        d->State().filterActive = false;
        d->State().visibleFrames.clear();
        d->UpdateAllViews();
    }
    UpdateAllViews();
}

void MainWindow::OnStepEdited()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    bool ok = false;
    uint64_t v = m_edStep->text().toULongLong(&ok);
    if (ok && v > 0) {
        v *= static_cast<uint64_t>(UnitOf(d->State().params.view));
        d->State().frame.step = v;
        d->State().frame.SanitizeStep();
        d->UpdateAllViews();
    }
    UpdateAllViews();
}

void MainWindow::OnMenuStartEdited()
{
    if (m_edStart && m_menuEdStart) m_edStart->setText(m_menuEdStart->text());
    OnStartEdited();
}

void MainWindow::OnMenuFrameLenEdited()
{
    if (m_edFrameLen && m_menuEdFrameLen) m_edFrameLen->setText(m_menuEdFrameLen->text());
    OnFrameLenEdited();
}

void MainWindow::OnMenuStepEdited()
{
    if (m_edStep && m_menuEdStep) m_edStep->setText(m_menuEdStep->text());
    OnStepEdited();
}

void MainWindow::OnViewModeCombo(int idx)
{
    if (idx < 0) return;
    BesDoc* d = ActiveDoc();
    if (!d) return;
    d->State().params.view = ViewFromComboIndex(idx);
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::OnYDispCombo(int idx)
{
    if (idx < 0) return;
    BesDoc* d = ActiveDoc();
    if (!d) return;
    d->State().params.yAxis = static_cast<YAxisMode>(idx);
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::OnRecentFile()
{
    if (auto* a = qobject_cast<QAction*>(sender()))
        OpenDocumentFile(a->data().toString());
}

// ---------------------------------------------------------------- 文件动作
void MainWindow::DoOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开"), QString(),
        QStringLiteral("数据文件 (*.dat *.bnt *.bin *.raw *.out);;所有文件 (*.*)"));
    if (!path.isEmpty())
        OpenDocumentFile(path);
}

void MainWindow::DoSave()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    QString err;
    if (!d->SaveFile(&err)) {
        ShowMessage(err.isEmpty() ? QStringLiteral("保存文件失败！") : err);
        return;
    }
    if (d->InEdit()) UpdateMenuState();   // 脏标志已清，撤销/重做可用性不变
    ShowMessage(QStringLiteral("数据已写回文件。"));
}

void MainWindow::DoSaveAs()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为"), d->FilePath(),
        QStringLiteral("数据文件 (*.dat);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    BesView* v = ActiveView();
    OutputOptions opt;
    opt.target = OutputTarget::File;
    opt.filePath = path.toStdWString();
    opt.bitsPerCell = v ? v->CellBits() : 8;
    const DocState& st = d->State();
    if (!st.sel.Empty()) {
        opt.wholeData = false;
        opt.continuousSel = st.sel.HasContinuous();
        opt.rectSel = !st.sel.Rects().empty();
        opt.rowSel = !st.sel.Rows().empty();
        opt.colSel = !st.sel.Cols().empty();
    }

    FrameModel fm = st.frame;
    if (fm.frameLen == 0 && v) fm.frameLen = v->FrameLenBits();
    uint64_t written = 0;
    std::wstring err;
    OutputWriter w;
    if (!w.Write(d->Map(), fm, st.sel, opt, written, &err)) {
        ShowMessage(err.empty() ? QStringLiteral("保存文件失败！")
                                : QString::fromStdWString(err));
        return;
    }
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("文件"), QStringLiteral("另存为"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1 (%2 字节)")
                                    .arg(path).arg(static_cast<qulonglong>(written)) });
    UpdateAllViews();
}

void MainWindow::DoPrint()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;

    DlgPrint dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QPrinter* printer = dlg.Printer();
    if (!printer) return;
    printer->setDocName(QFileInfo(d->FilePath()).fileName());

    QPainter painter;
    if (!painter.begin(printer)) {
        ShowMessage(QStringLiteral("无法开始打印作业！"));
        return;
    }

    const int ml = dlg.Left(), mr = dlg.Right(), mt = dlg.Top(), mb = dlg.Bottom();
    const QRectF page = painter.viewport();
    const int dpiX = printer->logicalDpiX() > 0 ? printer->logicalDpiX() : 96;
    const int dpiY = printer->logicalDpiY() > 0 ? printer->logicalDpiY() : 96;
    // 对话框边距单位为毫米，换算为打印设备像素
    const auto mmToPx = [](int mm, int dpi) {
        return static_cast<int>(std::lround(mm * dpi / 25.4));
    };
    const int left   = static_cast<int>(page.left()) + mmToPx(ml, dpiX);
    const int right  = static_cast<int>(page.right()) - mmToPx(mr, dpiX);
    const int top    = static_cast<int>(page.top()) + mmToPx(mt, dpiY);
    const int bottom = static_cast<int>(page.bottom()) - mmToPx(mb, dpiY);
    if (right <= left || bottom <= top) { painter.end(); return; }

    QFont mono(QStringLiteral("Courier New"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(8);
    painter.setFont(mono);
    const QFontMetrics fm(mono);
    const int lineH = fm.height();
    const int colW  = fm.horizontalAdvance(QLatin1Char('0')) * 2 + 2;
    const int bytesPerLine = std::max(1, (right - left - 120) / colW);
    const int linesPerPage = std::max(1, (bottom - top - lineH * 2) / lineH);
    const uint64_t bytesPerPage = static_cast<uint64_t>(bytesPerLine) *
                                  static_cast<uint64_t>(linesPerPage);

    const FileMap& map = d->Map();
    const DocState& st = d->State();
    const uint64_t totalBits = map.SizeBits();
    const uint64_t startBit  = std::min<uint64_t>(st.frame.startBit, totalBits);
    const uint64_t printLen  = totalBits > startBit ? totalBits - startBit : 0;
    const uint64_t endBit    = startBit + printLen;
    const uint64_t totalBytes = (endBit > startBit) ? (endBit - startBit + 7) / 8 : 0;
    const uint64_t totalPages = std::max<uint64_t>(1, (totalBytes + bytesPerPage - 1) / bytesPerPage);

    int first = std::max(1, dlg.First());
    int last  = dlg.Last() <= 0 ? static_cast<int>(totalPages)
                                : std::min<int>(dlg.Last(), static_cast<int>(totalPages));
    if (first > static_cast<int>(totalPages)) first = static_cast<int>(totalPages);
    if (last < first) last = first;

    uint64_t printed = (static_cast<uint64_t>(first) - 1) * bytesPerPage;
    int pageNo = first - 1;

    while (printed < totalBytes && pageNo < last) {
        ++pageNo;
        int y = top;
        if (pageNo > first) {
            if (!printer->newPage()) break;
        }
        painter.setPen(Qt::black);
        painter.drawText(left, y, QStringLiteral("BES 数据打印  文件: %1%2")
                         .arg(QFileInfo(d->FilePath()).fileName())
                         .arg(pageNo == first ? QString() : QStringLiteral(" (续)")));
        y += lineH;
        painter.drawText(left, y, QStringLiteral("起点: %1 比特   数据长度: %2 比特   共 %3 字节")
                         .arg(static_cast<qulonglong>(startBit))
                         .arg(static_cast<qulonglong>(printLen))
                         .arg(static_cast<qulonglong>(totalBytes)));
        y += lineH * 2;

        for (int line = 0; line < linesPerPage && printed < totalBytes; ++line) {
            const uint64_t lineStartByte = printed;
            painter.setPen(QColor(80, 80, 80));
            painter.drawText(left, y, QStringLiteral("%1")
                             .arg(static_cast<qulonglong>(lineStartByte * 8),
                                  8, 16, QLatin1Char('0')));
            painter.setPen(Qt::black);
            int cx = left + 90;
            for (int b = 0; b < bytesPerLine && printed < totalBytes; ++b, ++printed) {
                const uint64_t bitPos = startBit + lineStartByte * 8 + static_cast<uint64_t>(b) * 8;
                uint8_t val = map.ReadByteAtBit(bitPos);
                if (st.params.invert) val = static_cast<uint8_t>(~val);
                painter.drawText(cx, y, QStringLiteral("%1")
                                 .arg(val, 2, 16, QLatin1Char('0')).toUpper());
                cx += colW;
            }
            y += lineH;
        }

        painter.setPen(Qt::black);
        painter.drawText(left, bottom, QStringLiteral("共 %1 字节   第 %2 / %3 页")
                         .arg(static_cast<qulonglong>(totalBytes))
                         .arg(pageNo).arg(totalPages));
    }

    painter.end();
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("文件"), QStringLiteral("打印"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1 字节 / 第%2-%3页")
                                    .arg(static_cast<qulonglong>(totalBytes))
                                    .arg(first).arg(last) });
}

void MainWindow::DoOutputSet()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    BesView* v = ActiveView();
    DlgOutputSet dlg(this);
    dlg.SetBitsPerCell(v ? v->CellBits() : 8);

    QStringList portLabels;
    QVector<int> portIndices;
    for (const ComponentInfo& ci : m_host.Components()) {
        for (const PortInfo& p : ci.ports) {
            if (p.kind != PortKind::DataOut) continue;
            if (p.index < 1 || p.index > 32) continue;   // 原版端口范围 1..32
            portLabels << QStringLiteral("%1 / 端口%2 [%3]")
                              .arg(QString::fromStdWString(ci.name))
                              .arg(p.index)
                              .arg(QString::fromWCharArray(DataTypeName(p.dataType)));
            portIndices << p.index;
        }
    }
    dlg.SetPortChoices(portLabels, portIndices);

    if (dlg.exec() == QDialog::Accepted) {
        FrameModel fm = d->State().frame;
        if (fm.frameLen == 0 && v) fm.frameLen = v->FrameLenBits();
        uint64_t written = 0;
        std::wstring err;
        OutputWriter w;
        const OutputOptions& opt = dlg.Options();

        QString portPath;
        if (opt.target == OutputTarget::Port) {
            portPath = QDir::temp().filePath(
                QStringLiteral("bes_port_%1.dat").arg(opt.portIndex));
            TempFilePort sink(portPath);
            if (!sink.IsOpen()) {
                ShowMessage(QStringLiteral("无法创建端口输出文件！"));
                return;
            }
            w.BindPort(opt.portIndex, &sink);
            if (!w.Write(d->Map(), fm, d->State().sel, opt, written, &err)) {
                ShowMessage(err.empty() ? QStringLiteral("数据输出失败！")
                                        : QString::fromStdWString(err));
                return;
            }
        } else if (!w.Write(d->Map(), fm, d->State().sel, opt, written, &err)) {
            ShowMessage(err.empty() ? QStringLiteral("数据输出失败！")
                                    : QString::fromStdWString(err));
            return;
        }

        if (opt.target != OutputTarget::None) {
            QString msg = QStringLiteral("已输出 %1 字节").arg(static_cast<qulonglong>(written));
            if (!portPath.isEmpty())
                msg += QStringLiteral("\n端口文件：%1").arg(portPath);
            ShowMessage(msg);
        }
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("文件"), QStringLiteral("数据输出"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("%1 字节%2")
                                        .arg(static_cast<qulonglong>(written))
                                        .arg(portPath.isEmpty()
                                                 ? QString()
                                                 : QStringLiteral(" -> ") + portPath) });
    }
}

// ---------------------------------------------------------------- 处理动作
void MainWindow::DoParamSet()
{
    BesDoc* active = ActiveDoc();
    if (!active) return;
    if (BesView* v = ActiveView()) v->UpdateScrollRange();

    // 原版按窗口列出所有数据窗口；这里每个文档对应一个窗口（克隆标签共享同一文档）
    std::vector<DlgParamSet::Window> windows;
    std::vector<BesDoc*> docs;
    int current = 0;
    for (int i = 0; i < m_area->count(); ++i) {
        auto* v = dynamic_cast<BesView*>(m_area->widget(i));
        BesDoc* d = v ? v->Doc() : nullptr;
        if (!d) continue;
        if (std::find(docs.begin(), docs.end(), d) != docs.end()) continue;
        if (d == active) current = static_cast<int>(windows.size());
        DlgParamSet::Window w;
        w.title  = m_area->tabText(i);
        w.params = d->State().params;
        w.frame  = d->State().frame;
        w.hasStatusPort = false;
        windows.push_back(w);
        docs.push_back(d);
    }
    if (windows.empty()) return;

    DlgParamSet dlg(this, windows, current, active->State().colors, m_host.Opts());
    connect(&dlg, &DlgParamSet::Applied, this, [this, &dlg, &docs] {
        const std::vector<DlgParamSet::Window>& wins = dlg.Windows();
        for (size_t i = 0; i < wins.size() && i < docs.size(); ++i) {
            BesDoc* d = docs[i];
            if (!d) continue;
            DocState& st = d->State();
            const FrameModel oldFrame = st.frame;
            st.params = wins[i].params;
            st.frame  = wins[i].frame;
            st.frame.SanitizeStep();
            st.colors = dlg.Colors();   // 原版颜色为全局设置
            if (oldFrame.startBit != st.frame.startBit ||
                oldFrame.frameLen != st.frame.frameLen) {
                st.frame.bVariable = false;
                st.varFrames.Clear();
                st.filterActive = false;
                st.visibleFrames.clear();
            }
            d->UpdateAllViews();
        }
        m_host.Opts() = dlg.Options();
        UpdateAllViews();
    });
    dlg.exec();
}

void MainWindow::DoFilter()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    DocState& st = d->State();
    FrameModel fm = d->State().frame;
    BesView* v = ActiveView();
    if (fm.frameLen == 0 && v) fm.frameLen = v->FrameLenBits();

    DlgFilter dlg(this, d->Map(), fm,
                  st.filterActive && !st.filter.conditions.empty() ? &st.filter : nullptr,
                  st.frame.bVariable ? &st.varFrames : nullptr);

    bool applied = false;
    auto applyResult = [this, d, &dlg, v, fm, &applied]() {
        DocState& st = d->State();
        if (dlg.Filter().Active()) {
            st.filter = dlg.Filter();
            st.visibleFrames = dlg.Matched();
            st.filterActive = dlg.ApplyToView();
        } else {
            st.filter = FrameFilter();
            st.visibleFrames.clear();
            st.filterActive = false;
        }

        if (!dlg.Matched().empty() && v) {
            const uint64_t first = dlg.Matched().front();
            const uint64_t bit = st.frame.bVariable && first < st.varFrames.Size()
                ? st.varFrames.Frames()[static_cast<size_t>(first)].startBit
                : st.frame.startBit + first * fm.frameLen;
            d->SetCursorBit(bit);
            v->SyncToBit(bit);
            if (m_eventPane)
                m_eventPane->AddEvent({ QStringLiteral("分析"), QStringLiteral("帧过滤"),
                                        QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                        QStringLiteral("匹配 %1 帧，已跳至第 %2 帧")
                                            .arg(static_cast<qulonglong>(dlg.Matched().size()))
                                            .arg(static_cast<qulonglong>(first)) });
        }
        applied = true;
        d->UpdateAllViews();
        UpdateAllViews();
    };
    // 原版“应用”不关闭对话框，立即作用于主视图
    connect(&dlg, &DlgFilter::FilterApplied, this, applyResult);

    const bool accepted = (dlg.exec() == QDialog::Accepted);
    if (!accepted && !applied)
        return;
    if (!applied || !dlg.Filter().Active())
        applyResult();
}

void MainWindow::DoSearchUnique()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    DlgSearchUnique dlg(this, d->Map());
    if (dlg.exec() == QDialog::Accepted && !dlg.Hits().empty()) {
        const uint64_t bit = dlg.Hits().front().bitPos;
        d->SetCursorBit(bit);
        if (BesView* v = ActiveView()) v->SyncToBit(bit);
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("查找"), QStringLiteral("独特码"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("命中 %1 处，已跳至第 %2 比特")
                                        .arg(static_cast<qulonglong>(dlg.Hits().size()))
                                        .arg(static_cast<qulonglong>(bit)) });
        UpdateAllViews();
    }
}

void MainWindow::DoSearchFrameLength()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    BesView* v = ActiveView();
    const uint64_t fl = v ? v->FrameLenBits() : d->FrameLenBits(80);
    const uint64_t start = d->State().frame.startBit;
    DlgSearchFl dlg(this, d->Map(), start, fl);
    connect(&dlg, &DlgSearchFl::FrameLengthFound, this,
            [this, d](quint64 len, quint64 cols) {
        DocState& st = d->State();
        if (st.frame.frameLen != len) {
            st.frame.bVariable = false;
            st.varFrames.Clear();
        }
        st.frame.frameLen = len;
        st.frame.SanitizeStep();
        st.filterActive = false;
        st.visibleFrames.clear();
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("查找"), QStringLiteral("帧长"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("帧长%1比特，全0/1列%2个，已应用")
                                        .arg(static_cast<qulonglong>(len))
                                        .arg(static_cast<qulonglong>(cols)) });
        d->UpdateAllViews();
        UpdateAllViews();
    });
    dlg.exec();
}

void MainWindow::DoVarFrame(bool process)
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    DocState& st = d->State();
    DlgVarFrame dlg(this, d->Map(), st.frame, process,
                    st.frame.bVariable ? &st.varFrames : nullptr);

    // 扫描/编辑时实时预览；取消则恢复打开对话框前的状态
    const bes::VariableFrameSet prevFrames = st.varFrames;
    const bes::FrameModel       prevFrame  = st.frame;
    const bool                  prevFilter = st.filterActive;
    const std::vector<uint64_t> prevVisible = st.visibleFrames;
    bool previewed = false;

    auto applyFrames = [&](const bes::VariableFrameSet& fs) {
        st.varFrames = fs;
        st.frame.bVariable = (fs.Size() > 0);
        st.frame.startBit = dlg.Frame().startBit;
        st.filterActive = false;
        st.visibleFrames.clear();
        d->UpdateAllViews();
        UpdateAllViews();
    };

    if (!process) {
        connect(&dlg, &DlgVarFrame::FramesChanged, this, [&]{
            previewed = true;
            applyFrames(dlg.Frames());
        });
    }

    if (dlg.exec() == QDialog::Accepted) {
        applyFrames(dlg.Frames());
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("处理"), QStringLiteral("不等长帧"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("%1 帧").arg(static_cast<qulonglong>(st.varFrames.Size())) });
    } else if (previewed) {
        st.varFrames = prevFrames;
        st.frame = prevFrame;
        st.filterActive = prevFilter;
        st.visibleFrames = prevVisible;
        d->UpdateAllViews();
        UpdateAllViews();
    }
}

void MainWindow::DoQuitSearch()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    d->State().filterActive = false;
    d->State().visibleFrames.clear();
    d->State().filter = FrameFilter();
    UpdateAllViews();
}

void MainWindow::DoSetStart()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    d->State().frame.startBit = d->CursorBit();
    UpdateAllViews();
}

void MainWindow::DoRefresh()
{
    UpdateAllViews();
}

// ---------------------------------------------------------------- 编辑
namespace {

// 原版剪贴板里放的是数据字节，这里用自有格式承载，同时给出文本便于外部程序查看
const char kBesBytesMime[] = "application/x-bes-bytes";

QByteArray ClipboardBytes(const QMimeData* md)
{
    if (!md) return QByteArray();
    if (md->hasFormat(QString::fromLatin1(kBesBytesMime)))
        return md->data(QString::fromLatin1(kBesBytesMime));
    if (md->hasText())
        return md->text().toLocal8Bit();   // 中文环境即 GBK
    return QByteArray();
}

// 连续选取覆盖的字节区间；原版按字节编辑，端点不足整字节的并入整字节
bool SelectedByteRange(const BesDoc& d, uint64_t& off, uint64_t& len)
{
    const SelectionSet& sel = d.State().sel;
    if (!sel.HasContinuous()) return false;
    const uint64_t size = d.Map().SizeBytes();
    if (size == 0) return false;
    const uint64_t lo = sel.ContinuousStart() / 8ull;
    const uint64_t hi = sel.ContinuousEnd() / 8ull;
    if (lo >= size) return false;
    off = lo;
    len = std::min(hi, size - 1) - lo + 1;
    return len > 0;
}

} // namespace

void MainWindow::ToggleEditMode()
{
    BesDoc* d = ActiveDoc();
    if (!d) { UpdateMenuState(); return; }

    if (!d->InEdit()) {
        // 原版进入编辑不做任何前置校验
        QString err;
        if (!d->EnterEdit(&err)) {
            ShowMessage(err.isEmpty() ? QStringLiteral("无法进入编辑状态！") : err);
            UpdateMenuState();
            return;
        }
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("进行编辑"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("进入编辑状态") });
        UpdateAllViews();
        return;
    }

    int choice = 2;   // 不脏：直接退出
    if (d->Editor().Dirty()) {
        const int r = QMessageBox::question(
            this, QStringLiteral("BES"),
            QStringLiteral("文件内容已改变，保存吗？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        choice = (r == QMessageBox::Yes) ? 1
               : (r == QMessageBox::No) ? 2 : 0;
    }
    LeaveEditMode(d, choice);
}

bool MainWindow::LeaveEditMode(BesDoc* d, int choice)
{
    if (!d || !d->InEdit()) return false;
    if (choice == 0) { UpdateMenuState(); return false; }   // 取消：按钮弹回，保持编辑态
    if (choice == 1) {
        QString err;
        if (!d->SaveEdits(&err)) {
            ShowMessage(err.isEmpty() ? QStringLiteral("文件不可写！") : err);
            UpdateMenuState();
            return false;
        }
    }
    d->LeaveEdit();
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("进行编辑"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                choice == 1 ? QStringLiteral("保存并退出编辑状态")
                                            : QStringLiteral("退出编辑状态") });
    UpdateAllViews();
    return true;
}

void MainWindow::DoUndo()
{
    BesDoc* d = ActiveDoc();
    if (!d || !d->InEdit()) return;
    if (!d->Editor().Undo()) return;   // 无撤销记录时静默
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::DoRedo()
{
    BesDoc* d = ActiveDoc();
    if (!d || !d->InEdit()) return;
    if (!d->Editor().Redo()) return;
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::DoCopy()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    uint64_t off = 0, len = 0;
    if (!SelectedByteRange(*d, off, len)) {
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("复制"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("需要连续选取") });
        return;
    }
    QByteArray bytes(static_cast<int>(len), Qt::Uninitialized);
    for (uint64_t i = 0; i < len; ++i) {
        uint8_t b = 0;
        d->Map().ReadByte(off + i, b);
        bytes[static_cast<int>(i)] = static_cast<char>(b);
    }
    QMimeData* md = new QMimeData;
    md->setData(QString::fromLatin1(kBesBytesMime), bytes);
    md->setText(QString::fromLatin1(bytes.constData(), bytes.size()));
    QApplication::clipboard()->setMimeData(md);
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("复制"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1 字节（起点 %2）")
                                    .arg(static_cast<qulonglong>(len))
                                    .arg(static_cast<qulonglong>(off)) });
}

void MainWindow::DoCut()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    if (!d->InEdit()) { DoCopy(); return; }   // 非编辑态只能复制（删除需要可增长缓冲）
    uint64_t off = 0, len = 0;
    if (!SelectedByteRange(*d, off, len)) { DoCopy(); return; }
    DoCopy();
    if (!d->Editor().Remove(off, len)) return;
    d->State().sel.ClearContinuous();
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("剪切"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("删除 %1 字节（起点 %2）")
                                    .arg(static_cast<qulonglong>(len))
                                    .arg(static_cast<qulonglong>(off)) });
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::DoPaste()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    if (!d->InEdit()) {
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("编辑"), QStringLiteral("粘贴"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("请先进入编辑状态") });
        return;
    }
    const QByteArray bytes = ClipboardBytes(QApplication::clipboard()->mimeData());
    if (bytes.isEmpty()) return;
    const uint64_t off = d->CursorBit() / 8ull;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(bytes.constData());
    BinaryEditor& ed = d->Editor();
    // 插入/改写由状态栏指示（原版 [engine+0x1c]）
    const bool ok = m_insertMode ? ed.Insert(off, p, static_cast<size_t>(bytes.size()))
                                 : ed.Overwrite(off, p, static_cast<size_t>(bytes.size()));
    if (!ok) return;
    if (m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("编辑"),
                                m_insertMode ? QStringLiteral("粘贴(插入)")
                                             : QStringLiteral("粘贴(改写)"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1 字节（起点 %2）")
                                    .arg(static_cast<qulonglong>(bytes.size()))
                                    .arg(static_cast<qulonglong>(off)) });
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::DoSelectMode(SelectMode m)
{
    BesDoc* d = ActiveDoc();
    if (!d) { UpdateMenuState(); return; }
    d->SetSelectMode(m);
    UpdateAllViews();
    UpdateMenuState();   // 四个选取按钮的勾选态跟随当前模式（同模式再点会经 ClearAll 退出）
}

void MainWindow::DoClearCurrentSelection()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    SelectionSet& sel = d->State().sel;
    sel.ClearMode(sel.Mode());
    UpdateAllViews();
}

void MainWindow::DoClearAllSelection()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    SelectionSet& sel = d->State().sel;
    const SelectMode mode = sel.Mode();
    sel.ClearAll();
    sel.SetMode(mode);   // 清列表不动模式（原版 0x4404CC dl=1）
    UpdateAllViews();
}

void MainWindow::DoSaveSelection()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("选择参数文件"), QString(),
        QStringLiteral("选取参数文件(*.slp);;所有文件(*.*)"));
    if (path.isEmpty()) return;
    QString actual = path;
    if (QFileInfo(actual).suffix().isEmpty())
        actual += QStringLiteral(".slp");
    QFile f(actual);
    if (!f.open(QIODevice::WriteOnly)) {
        ShowMessage(QStringLiteral("保存文件失败！"));
        return;
    }
    const std::vector<uint8_t> blob = d->State().sel.SaveSlp();
    f.write(reinterpret_cast<const char*>(blob.data()),
            static_cast<qint64>(blob.size()));
    f.close();
}

void MainWindow::DoLoadSelection()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择参数文件"), QString(),
        QStringLiteral("选取参数文件(*.slp);;所有文件(*.*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        ShowMessage(QStringLiteral("无法打开文件"));
        return;
    }
    const QByteArray raw = f.readAll();
    f.close();
    const std::vector<uint8_t> blob(
        reinterpret_cast<const uint8_t*>(raw.constData()),
        reinterpret_cast<const uint8_t*>(raw.constData()) + raw.size());
    std::wstring err;
    if (!d->State().sel.LoadSlp(blob, &err))
        ShowMessage(QString::fromStdWString(err));
    UpdateAllViews();
}

void MainWindow::DoToggleBookmark()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    const uint64_t bit = d->CursorBit();
    if (d->State().marks.Has(bit))
        d->State().marks.RemoveAt(bit);
    else
        d->State().marks.Set(bit, L"");
    UpdateAllViews();
}

void MainWindow::DoNextBookmark()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    uint64_t out = 0;
    if (d->State().marks.Next(d->CursorBit(), out)) {
        d->SetCursorBit(out);
        if (BesView* v = ActiveView()) v->SyncToBit(out);
        UpdateAllViews();
    }
}

void MainWindow::DoPrevBookmark()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    uint64_t out = 0;
    if (d->State().marks.Prev(d->CursorBit(), out)) {
        d->SetCursorBit(out);
        if (BesView* v = ActiveView()) v->SyncToBit(out);
        UpdateAllViews();
    }
}

void MainWindow::DoClearBookmarks()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    // 原版 ClearBookMarkBtn 走 BookMarkBtnClick 的 else 分支（书签接口 vtbl+0x48）
    d->State().marks.Clear();
    UpdateAllViews();
}

void MainWindow::DoGoto()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    BesView* v = ActiveView();
    FrameModel fm = d->State().frame;
    if (fm.frameLen == 0 && v) fm.frameLen = v->FrameLenBits();
    DlgPosition dlg(this, fm, d->State().marks,
                    v ? v->CellBits() : 8,
                    ByteUnitView(d->State().params.view));
    connect(&dlg, &DlgPosition::Applied, this, [this, d](uint64_t bit){
        d->State().frame.startBit = bit;
        d->SetCursorBit(bit);
        if (BesView* view = ActiveView()) view->SyncToBit(bit);
        UpdateAllViews();
    });
    if (dlg.exec() == QDialog::Accepted) {
        d->State().frame.startBit = dlg.Frame().startBit;
        d->SetCursorBit(dlg.ResultBit());
        if (v) v->SyncToBit(dlg.ResultBit());
        UpdateAllViews();
    }
}

void MainWindow::DoComponentManage()
{
    DlgComponent dlg(this, m_host);
    // 原版 AddBtnClick 往事件表写一条文本为 'Com' 的记录
    connect(&dlg, &DlgComponent::ComponentRegistered, this, [this](const QString& name) {
        if (m_eventPane)
            m_eventPane->AddEvent({ QStringLiteral("构件"), QStringLiteral("注册"),
                                    QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                    name });
    });
    dlg.exec();
    SavePluginList();
    SyncGroupPane();
}

// 原版「识别 → 数据流输入 / 数据格式输入 / 数据流输出」三项共用一个窗口：
// TProcessForm 的行分类正是 数据输入(SelectMode/反选/选取参数)、数据格式(数据类型)、
// 数据输出(输出标志/输出文件)（DFM 0x22B9F8），故三个命令都走这里。
void MainWindow::DoProcessFlow()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;

    QString comName;
    int inPortIndex = 0;
    bool hasOut = false;
    if (m_host.Count() > 0) {
        DlgComSelect dlg(this, m_host);
        if (dlg.exec() != QDialog::Accepted || dlg.SelectedName().empty()) return;
        comName = QString::fromStdWString(dlg.SelectedName());
        // 原版 TComSelectForm.OKBtnClick 把实例信息写回 +0x2E4/+0x2F4，
        // TProcessForm 直接取用：唯一的数据流输入端口 ID 与是否存在输出端口。
        inPortIndex = dlg.DataInPortId();
        hasOut = dlg.HasOutputPort();
    } else {
        ShowMessage(QStringLiteral("没有可用构件！\n请先通过「定制 → 定制构件管理」注册构件 DLL。"),
                    QStringLiteral("构件数据流处理"));
        return;
    }

    DlgProcess dlg(this, comName, d->State().sel);
    BesView* v = ActiveView();
    FrameModel fm = d->State().frame;
    if (fm.frameLen == 0 && v) fm.frameLen = v->FrameLenBits();

    dlg.setOutputPort(hasOut);
    // 原版 SelectModeRowChange 的「列选取也可反选」条件取 [[ctx+0x74]+4]==7，
    // 那是另一个上下文对象，与本 ABI 的 1 基构件类型（1..4）无关，故不启用。

    // 数据流输入管线：选区字节 → 构件输入端口（临时文件承载，见 FlowInputFile）
    QString portPath;
    if (inPortIndex)
        portPath = QDir::temp().filePath(
            QStringLiteral("bes_flow_in_%1.dat").arg(inPortIndex));
    std::unique_ptr<FlowInputFile> port;
    if (!portPath.isEmpty()) {
        port.reset(new FlowInputFile(portPath));
        if (port->IsOpen()) dlg.bindFlowPort(port.get());
        else port.reset();
    }
    dlg.setSource(&d->Map(), d->State().sel, fm, v ? v->CellBits() : 8);

    dlg.exec();
    if (m_eventPane && dlg.engine().bytesWritten())
        m_eventPane->AddEvent({ QStringLiteral("处理"), QStringLiteral("数据流"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1：已输入 %2 字节%3")
                                    .arg(comName)
                                    .arg(static_cast<qulonglong>(dlg.engine().bytesWritten()))
                                    .arg(port ? QStringLiteral("（端口文件 %1）").arg(portPath)
                                              : QString()) });
    if (dlg.exitRequested() && m_eventPane)
        m_eventPane->AddEvent({ QStringLiteral("处理"), QStringLiteral("数据流"),
                                QTime::currentTime().toString(QStringLiteral("hh:mm:ss")),
                                QStringLiteral("%1：已退出").arg(comName) });
}

// 快速执行的加载段：原版 FastExecuteBtnClick 用 LoadLibrary + GetProcAddress
// 校验 RegComNum/RegComInfo/CreateInstance，任一失败都只报「不是局级系统构件！」，
// 因此这里不区分「打不开」与「缺导出」。
bool MainWindow::LoadFastExecuteDll(const QString& dllPath, bes::ComponentHost* host,
                                   std::wstring* err)
{
    std::wstring regErr;
    if (!host->RegisterDll(dllPath.toStdWString(), &regErr) ||
        !host->HasComponentAbi(dllPath.toStdWString())) {
        if (err) *err = L"不是局级系统构件！";
        return false;
    }
    return true;
}

void MainWindow::DoFastExecute()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择构件"), QString(),
        QStringLiteral("局级系统构件(*.dll)"));
    if (path.isEmpty()) return;

    // 原版只用临时列表，不写入持久构件注册表
    bes::ComponentHost host;
    std::wstring err;
    if (!LoadFastExecuteDll(path, &host, &err)) {
        QMessageBox::critical(this, QStringLiteral("错误"), QString::fromStdWString(err));
        return;
    }
    if (host.Count() == 0) {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QStringLiteral("没有可用构件！"));
        return;
    }

    DlgComSelect dlg(this, host);
    if (dlg.exec() != QDialog::Accepted || dlg.SelectedName().empty()) return;

    std::wstring modalErr;
    BesDoc* d = ActiveDoc();
    const std::wstring file = d ? d->FilePath().toStdWString() : std::wstring();
    if (host.ShowModal(dlg.SelectedName(), file, &modalErr)) {
        ShowMessage(QStringLiteral("已打开构件：%1")
                        .arg(QString::fromStdWString(dlg.SelectedName())),
                    QStringLiteral("选择构件"));
    } else {
        QMessageBox::critical(this, QStringLiteral("错误"),
                              QString::fromStdWString(modalErr));
    }
}

void MainWindow::DoSetViewMode(ViewMode m)
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    d->State().params.view = m;
    m_cboViewMode->setCurrentIndex(ComboIndexFromView(m));
    d->UpdateAllViews();
    UpdateAllViews();
}

void MainWindow::DoLookStyle(int cmdId)
{
    ApplyStyle(cmdId);
    QSettings().setValue(QStringLiteral("view/style"), cmdId);
}

void MainWindow::DoViewGroup()
{
    if (!m_groupDock) return;
    m_groupDock->setVisible(!m_groupDock->isVisible());
    UpdateMenuState();
}

void MainWindow::DoToggleEventPane()
{
    if (!m_eventDock) return;
    m_eventDock->setVisible(!m_eventDock->isVisible());
    UpdateMenuState();
}

void MainWindow::ToggleLinkBrowse()
{
    m_linkBrowse = !m_linkBrowse;
    QSettings().setValue(QStringLiteral("view/link"), m_linkBrowse);
    UpdateMenuState();
}

void MainWindow::SetBarVisible(int bar, bool visible)
{
    QToolBar* tb = GetToolBar(bar);
    if (!tb) return;
    tb->setVisible(visible);
    SyncToolBarMenu();
}

void MainWindow::SyncToolBarMenu()
{
    if (!m_toolbarMenu) return;
    for (QAction* a : m_toolbarMenu->actions()) {
        bool ok = false;
        const int i = a->data().toInt(&ok);
        if (!ok || i < 0 || i >= BarCount) continue;
        const QToolBar* tb = GetToolBar(i);
        QSignalBlocker block(a);
        a->setChecked(tb && !tb->isHidden());
    }
}

void MainWindow::ResetToolBars()
{
    for (int i = 0; i < BarCount; ++i) {
        m_barItems[i] = DefaultToolBarItems(i);
        RebuildToolBar(i);
        SetBarVisible(i, true);
    }
}

QMenu* MainWindow::BuildToolBarContextMenu()
{
    // 原版 TdxBar 的右键菜单：逐条工具栏勾选 + 打开 TdxBarCustomizingForm。
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("barContextMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    for (int i = 0; i < BarCount; ++i) {
        QAction* a = menu->addAction(ToolBarName(i));
        a->setCheckable(true);
        a->setData(i);
        a->setObjectName(QStringLiteral("barCtx_") + QString::number(i));
        const QToolBar* tb = GetToolBar(i);
        a->setChecked(tb && !tb->isHidden());
        connect(a, &QAction::toggled, this, [this, i](bool on){ SetBarVisible(i, on); });
    }
    menu->addSeparator();
    QAction* cust = menu->addAction(QStringLiteral("自定义..."));
    cust->setObjectName(QStringLiteral("barCtxCustomize"));
    connect(cust, &QAction::triggered, this, &MainWindow::OpenCustomize);
    return menu;
}

void MainWindow::OpenCustomize()
{
    DlgCustomize dlg(this);
    dlg.exec();
    SyncToolBarMenu();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    auto* w = qobject_cast<QWidget*>(obj);
    QToolBar* owner = nullptr;
    if (w) {
        for (QToolBar* bar : { m_mainBar, m_formatBar, m_quickBar }) {
            if (bar && (w == bar || bar->isAncestorOf(w))) {
                owner = bar;
                break;
            }
        }
    }
    if (owner) {
        if (ev->type() == QEvent::ContextMenu) {
            auto* ce = static_cast<QContextMenuEvent*>(ev);
            BuildToolBarContextMenu()->popup(ce->globalPos());
            return true;
        }
        // 只认工具栏空白处（停靠站点）的双击；按钮上的双击仍按原命令执行
        if (ev->type() == QEvent::MouseButtonDblClick && w == owner) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                OpenCustomize();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::DuplicateCurrentTab()
{
    BesDoc* d = ActiveDoc();
    if (!d) return;
    auto* view = new BesView;
    view->SetDoc(d);
    m_area->addTab(view, QFileInfo(d->FilePath()).fileName() + QStringLiteral("*"));
    m_area->setCurrentWidget(view);
    UpdateMenuState();
    SyncGroupPane();
}

void MainWindow::DoAbout()
{
    DlgAbout dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------- 最近文件
void MainWindow::AddRecentFile(const QString& path)
{
    QSettings st;
    QStringList files = st.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > 8) files.removeLast();
    st.setValue(QStringLiteral("recentFiles"), files);
    UpdateRecentMenu();
}

void MainWindow::UpdateRecentMenu()
{
    QMenu* mRecent = findChild<QMenu*>(QStringLiteral("recentMenu"));
    if (!mRecent) return;
    mRecent->clear();
    QSettings st;
    const QStringList files = st.value(QStringLiteral("recentFiles")).toStringList();
    if (files.isEmpty()) {
        QAction* empty = mRecent->addAction(QStringLiteral("（无）"));
        empty->setEnabled(false);
        return;
    }
    int idx = 1;
    for (const QString& f : files) {
        if (!QFile::exists(f)) continue;
        QAction* a = mRecent->addAction(QStringLiteral("&%1 %2").arg(idx++).arg(f));
        connect(a, &QAction::triggered, this, [this, f]{ OpenDocumentFile(f); });
    }
}
