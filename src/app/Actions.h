// Actions.h —— 全部命令动作（ID 与原程序 verres.h 一致）
#pragma once

#include <QObject>
#include <QAction>
#include <QVector>

class MainWindow;
class BesDoc;

// 命令 ID（沿用原程序 verres.h 的数值）
enum CmdId {
    CMD_FILE_NEW              = 40001,
    CMD_VIEW_REFRESH          = 40002,
    CMD_VIEW_ZOOM_IN          = 40003,
    CMD_VIEW_ZOOM_OUT         = 40004,
    CMD_VIEW_ZOOM_RESET       = 40005,
    CMD_VIEW_RULER            = 40006,
    CMD_VIEW_GROUP_HORIZ      = 40009,
    CMD_VIEW_GROUP_VERT       = 40010,
    CMD_VIEW_GROUP            = 40011,
    CMD_VIEW_LINK             = 40013,
    CMD_VIEW_TITLE            = 40014,
    CMD_VIEW_PAGE             = 40015,
    CMD_VIEW_HORIZ            = 40016,
    CMD_VIEW_VERT             = 40017,
    CMD_VIEW_MODE_YFN         = 40018,
    CMD_VIEW_MODE_YFL         = 40019,
    CMD_VIEW_MODE_YFB         = 40020,
    CMD_SELECT_CONTINUOUS     = 40021,
    CMD_SELECT_RECT           = 40022,
    CMD_SELECT_ROW            = 40023,
    CMD_SELECT_COL            = 40024,
    CMD_SELECT_LOAD           = 40025,
    CMD_SELECT_SAVE           = 40026,
    CMD_SELECT_CLEAR_CURRENT  = 40027,
    CMD_SELECT_CLEAR_ALL      = 40028,
    CMD_ANALYZE_PARAM         = 40030,
    CMD_ANALYZE_ITERATE       = 40031,
    CMD_ANALYZE_VARFRAME      = 40032,
    CMD_ANALYZE_GOTO          = 40033,
    CMD_ANALYZE_SEARCH_UNIQUE = 40034,
    CMD_ANALYZE_SEARCH_FL     = 40035,
    CMD_ANALYZE_QUIT_SEARCH   = 40036,
    CMD_ANALYZE_FILTER        = 40037,
    CMD_VIEW_MODE_BITMAP      = 40038,
    CMD_VIEW_MODE_HEX         = 40039,
    CMD_VIEW_MODE_CHAR        = 40040,
    CMD_VIEW_MODE_PIXEL       = 40041,
    CMD_IDS_DATA_IN           = 40042,
    CMD_IDS_FORMAT_IN         = 40043,
    CMD_IDS_DATA_OUT          = 40044,
    CMD_CUSTOM_COMPONENT      = 40045,
    CMD_CUSTOM_FAST_EXEC      = 40048,
    CMD_WINDOW_NEW            = 40049,
    CMD_LOOK_TRADITIONAL      = 40050,
    CMD_LOOK_VSNET            = 40051,
    CMD_LOOK_OFFICE           = 40052,
    CMD_PROCESS_UNIQUE        = 40053,
    CMD_DATA_OUTPUT           = 40054,
    CMD_FILE_CLOSE_ALL        = 40055,
    CMD_BOOKMARK_TOGGLE       = 40056,
    CMD_BOOKMARK_NEXT         = 40057,
    CMD_BOOKMARK_PREV         = 40058,
    CMD_BOOKMARK_CLEAR        = 40059,
    CMD_SET_START             = 40060,
    CMD_DATA_NOT              = 40061,
    CMD_DATA_REVERSE          = 40062,
    CMD_VIEW_HTAB             = 40063,
    CMD_VIEW_VTAB             = 40064,
    CMD_TAB_LOCK              = 40065,
    // 文件/编辑等标准命令
    CMD_FILE_OPEN             = 40090,
    CMD_FILE_SAVE             = 40091,
    CMD_FILE_SAVE_AS          = 40092,
    CMD_FILE_PRINT            = 40093,
    CMD_FILE_CLOSE            = 40094,
    CMD_APP_EXIT              = 40095,
    CMD_APP_ABOUT             = 40096,
    CMD_EDIT_CUT              = 40097,
    CMD_EDIT_COPY             = 40098,
    CMD_EDIT_PASTE            = 40099,
    CMD_EDIT_UNDO             = 40100,
    CMD_EDIT_REDO             = 40101,
    CMD_VIEW_MODE_HEXCHAR     = 40102,
    CMD_VIEW_EVENT            = 40103,
    CMD_ANALYZE_STEP          = 40104,
    CMD_EDIT_MODE             = 40105,   // 工具栏「进行编辑」（原版 EditBtn，bsChecked）
};

class Actions : public QObject {
    Q_OBJECT
public:
    explicit Actions(MainWindow* mw, QObject* parent = nullptr);

    // 按 ID 查找
    QAction* operator[](int id) const;
    QAction* Find(int id) const;
    int IdOf(const QAction* action) const;
    // 全部命令（按 ID 升序），供工具栏定制对话框使用
    QList<QAction*> All() const;

    // 加入最近文件
    void AddRecentFile(const QString& path);

private:
    void Build();

    MainWindow* m_mw;
    QMap<int, QAction*> m_actions;
};
