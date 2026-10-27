// MainWindow.h —— 主窗口（菜单 + 4 工具栏 + MDI/Tab + 停靠面板 + 状态栏）
#pragma once

#include <QMainWindow>
#include <QMap>
#include <QVector>
#include <QStringList>

#include "core/DataTypes.h"
#include "core/SearchEngine.h"
#include "core/OutputWriter.h"
#include "plugin/ComponentHost.h"
#include "app/DocState.h"

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QToolBar;
class QLineEdit;
class QComboBox;
class QLabel;
class QMenu;
class QAction;
class QActionGroup;
class QDockWidget;
class BesDoc;
class BesView;
class ViewArea;
class EventPane;
class GroupPane;
class Actions;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // ---- 文档 ----
    BesDoc* OpenDocumentFile(const QString& path);
    BesDoc* ActiveDoc() const;
    BesView* ActiveView() const;
    int     ActiveIndex() const;

    // ---- 状态 ----
    void UpdateStatusText();
    void UpdateMenuState();
    void UpdateAllViews();
    void ToggleInsertMode();
    bool    InsertMode() const { return m_insertMode; }
    void ShowMessage(const QString& text, const QString& caption = QString());

    // ---- 动作（供 Actions 调用） ----
    void DoOpen();
    void DoSave();
    void DoSaveAs();
    void DoPrint();
    void DoCloseAll();
    void DoAbout();
    void DoSearchUnique();
    void DoSearchFrameLength();
    void DoFilter();
    void DoOutputSet();
    void DoParamSet();
    void DoPosition();
    void DoComponentManage();
    // 构件数据流处理（原版 TProcessForm：一个窗口同时覆盖数据流输入/格式输入/输出）
    void DoProcessFlow();
    void DoFastExecute();
    // 快速执行的加载段（不含模态框，便于测试）：把 DLL 载入临时宿主并校验 ABI
    static bool LoadFastExecuteDll(const QString& dllPath, bes::ComponentHost* host,
                                   std::wstring* err);
    void DoVarFrame(bool processMode);
    void DoSetStart();
    void DoRefresh();
    // ---- 编辑（原版 EditBtn / editIface 接口）----
    // 切换「进行编辑」：进入时把文件换成可增长内存副本；退出时脏则询问是否保存
    void ToggleEditMode();
    // choice: 0=取消（保持编辑态）1=保存后退出 2=不保存直接退出；返回 false 表示未退出
    bool LeaveEditMode(BesDoc* d, int choice);
    void DoCut();
    void DoCopy();
    void DoPaste();
    void DoUndo();
    void DoRedo();
    void DoSelectMode(bes::SelectMode m);
    void DoClearCurrentSelection();
    void DoClearAllSelection();
    void DoSaveSelection();
    void DoLoadSelection();
    void DoToggleBookmark();
    void DoNextBookmark();
    void DoPrevBookmark();
    void DoClearBookmarks();
    void DoGoto();
    void DoSetViewMode(bes::ViewMode m);
    void DoLookStyle(int cmdId);
    void DoViewGroup();
    void DoToggleEventPane();
    void ToggleLinkBrowse();
    void CloseTab(int idx);
    void CloseCurrentTab();
    void DuplicateCurrentTab();
    bool LinkBrowse() const { return m_linkBrowse; }
    void DoQuitSearch();

    // ---- 窗口排布（原版 [form+0x498]：分页/横排/竖排/分组横排/分组竖排）----
    // 全局状态：一次点击对全部视图生效（原版遍历所有停靠站点逐个重排）。
    void SetViewLayout(bes::ViewLayout l);
    bes::ViewLayout GetViewLayout() const { return m_viewLayout; }
    int  VisibleViewCount() const;

    // ---- 窗口分组（分组树内拖动窗口归组；「数据窗口」= 全部窗口）----
    bool CreateGroup(const QString& name, const QVector<int>& tabs);
    void MoveTabToGroup(int idx, const QString& group);

    bes::ComponentHost& Components() { return m_host; }
    Actions* Cmds() const { return m_actions; }

    // ---- 工具栏定制（原版 dxBarManager1 + TdxBarCustomizingForm）----
    // 每条工具栏的内容是命令 ID 的有序列表；0 = 分隔符，-1 = 格式栏纵轴下拉框占位。
    enum ToolBarKind { BarMain = 0, BarFormat = 1, BarQuick = 2, BarCount = 3 };
    static const int BarSeparator    = 0;
    static const int BarWidgetMarker = -1;
    static QVector<int> DefaultToolBarItems(int bar);
    static QString ToolBarName(int bar);
    QVector<int> ToolBarItems(int bar) const;
    void SetToolBarItems(int bar, const QVector<int>& items);
    QToolBar* GetToolBar(int bar) const;
    void SetBarVisible(int bar, bool visible);
    void ResetToolBars();
    void OpenCustomize();
    QMenu* BuildToolBarContextMenu();

protected:
    void closeEvent(QCloseEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    void OnStartEdited();
    void OnFrameLenEdited();
    void OnStepEdited();
    // 查看菜单镜像控件：先回写格式栏，再走同一处理（原版两处是同一个控件）
    void OnMenuStartEdited();
    void OnMenuFrameLenEdited();
    void OnMenuStepEdited();
    void OnViewModeCombo(int idx);
    void OnYDispCombo(int idx);
    void OnTabChanged(int idx);
    void SyncGroupPane();
    void AddRecentFile(const QString& path);
    void UpdateRecentMenu();
    void OnRecentFile();
    void OnGroupAddRequested();
    void OnGroupDeleteRequested(const QString& group);
    void OnGroupCloseRequested(const QString& group);

private:
    void CreateActions();
    void CreateMenuBar();
    void CreateToolBars();
    void CreateFormatBar();
    void RebuildToolBar(int bar);
    void SyncToolBarMenu();
    // 格式栏/查看菜单共用的「文本框 + 上下箭头」行（原版 TdxBarSpinEdit）
    QWidget* CreateSpinRow(QWidget* parent, QLineEdit*& ed, int width,
                           const QString& name, void (MainWindow::*slot)());
    void CreateStatusBar();
    void CreateDockPanes();
    void WireSignals();
    void ApplyStyle(int styleId);
    void RestoreSettings();
    void SaveSettings();
    void LoadPluginList();
    void SavePluginList();
    uint64_t EffectiveFrameLen() const;
    void AdjustGroupsAfterClose(int idx);
    void ApplyArrangement();
    void ApplyLayoutTo(BesDoc* doc);

    ViewArea*     m_area = nullptr;
    QToolBar*     m_mainBar = nullptr;
    QToolBar*     m_formatBar = nullptr;
    QToolBar*     m_quickBar = nullptr;
    QVector<int>  m_barItems[BarCount];
    QVector<QAction*> m_barManaged[BarCount];     // 由布局添加的命令动作（不拥有）
    QVector<QAction*> m_barTempActions[BarCount]; // 重建时创建的分隔符（拥有）
    QAction*      m_formatYDispAction = nullptr;
    QDockWidget*  m_groupDock = nullptr;
    QDockWidget*  m_eventDock = nullptr;
    QMenu*        m_toolbarMenu = nullptr;
    QLabel*       m_statusPos = nullptr;
    QLabel*       m_statusFile = nullptr;
    QLabel*       m_statusIns = nullptr;
    bool          m_insertMode = true;

    QLineEdit*    m_edStart = nullptr;
    QLineEdit*    m_edFrameLen = nullptr;
    QLineEdit*    m_edStep = nullptr;
    QComboBox*    m_cboViewMode = nullptr;
    QComboBox*    m_cboYDisp = nullptr;
    // 查看菜单内嵌的镜像控件（原版同一 TdxBar* 的 ItemLinks 同时挂在格式栏与菜单）
    QLineEdit*    m_menuEdStart = nullptr;
    QLineEdit*    m_menuEdFrameLen = nullptr;
    QLineEdit*    m_menuEdStep = nullptr;
    QComboBox*    m_menuCboViewMode = nullptr;

    EventPane*    m_eventPane = nullptr;
    GroupPane*    m_groupPane = nullptr;
    Actions*      m_actions = nullptr;
    QActionGroup* m_styleGroup = nullptr;
    QActionGroup* m_viewModeGroup = nullptr;
    QActionGroup* m_yAxisGroup = nullptr;
    bool          m_linkBrowse = false;
    int           m_styleId = 40050;   // 传统风格
    // 窗口排布（原版 [form+0x498]，全局而非按文档）
    bes::ViewLayout m_viewLayout = bes::ViewLayout::Paged;
    QMap<QString, QVector<int>> m_groups;
    bes::ComponentHost m_host;
};
