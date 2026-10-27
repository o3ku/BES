// BesView.h —— 数据视图（绘制 + 滚动 + 鼠标选取）
#pragma once

#include <QAbstractScrollArea>
#include <QFont>
#include <QPoint>

#include "core/Renderer.h"

class BesDoc;
class QMenu;
class QContextMenuEvent;
class QTimer;

class BesView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit BesView(QWidget* parent = nullptr);
    ~BesView() override;

    void SetDoc(BesDoc* doc);
    BesDoc* Doc() const { return m_doc; }

    void SetScrollBit(uint64_t bit);
    void SetScrollRowQuiet(uint64_t row);            // 联动同步（不触发再同步）
    void SetHOffsetQuiet(uint64_t off);
    uint64_t ScrollBit() const { return m_scrollBit; }
    uint64_t ScrollRow() const { return m_scrollRow; }
    uint64_t HOffset() const { return m_hOffset; }

    void UpdateScrollRange();
    void SyncToBit(uint64_t bit);

    // 联动浏览：把「同样的移动量」加到同步范围内每个视图自己的位置上
    // （原版 0x407B4C 收集同步对象：同一停靠容器内的其它视图，排除触发者本身）。
    void LinkShift(int64_t dRow, int64_t dCol);

    // 当前有效帧长（bit）/ 每格比特数 / 可见列数
    uint64_t FrameLenBits() const;
    int      CellBits() const;
    int      VisibleCols() const;
    int      YAxisWidth() const { return m_yaxisW; }

    // 原版 TMDIChild.FormMouseUp（0x405AE0）：右键弹出「快捷操作」栏的 ItemLinks。
    // 菜单项与命令共用 Actions，调用者负责 delete。
    QMenu* CreateContextMenu();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    void   EnsureFont();
    bool   HitBit(const QPoint& pt, uint64_t& bit) const;
    void   OnVScroll(int val);
    void   OnHScroll(int val);
    void   CommitSelection(uint64_t bit);
    void   BuildPreview(bes::SelectionSet& out) const;
    uint64_t RowForBit(uint64_t bit) const;
    void   PrepareRenderer(bes::Renderer& r) const;

    // 键盘扩展（原版 KeyDown@0x445B0C / KeyUp@0x4477DC）
    void   BeginKeyboardSelection();
    void   MoveKeyboardEndpoint(int dRow, int dCol, int absCol);
    void   EndKeyboardSelection(bool commit);
    void   MoveCaret(int dRow, int dCol, int absCol);
    void   EnsureBitVisible(uint64_t bit);

    // 拖出视口自动滚动（原版 +0x52 与 0x443F5C 的 PtInRect 判定）
    void   UpdateAutoScroll(const QPoint& pos);
    void   OnAutoScrollTick();

    BesDoc*   m_doc = nullptr;
    QFont     m_font;
    QFont     m_drawFont;
    int       m_charW = 8;
    int       m_zoomW = 8;    // 缩放后字符宽（= m_charW * zoom）
    int       m_zoomH = 12;   // 缩放后字符高
    int       m_charH = 16;
    int       m_yaxisW = 0;   // 纵轴区宽（0=不显示）
    uint64_t  m_scrollRow = 0;
    uint64_t  m_scrollBit = 0;
    uint64_t  m_hOffset = 0;  // 水平滚动格偏移
    bool      m_dragging = false;
    uint64_t  m_anchorBit = 0;
    uint64_t  m_dragBit = 0;     // 拖动中的当前端点（原版 obj+0x60/+0x64/+0x68）
    bool      m_hasAnchor = false;
    bool      m_keySelecting = false; // Shift 按住中（原版 +0x51）
    bool      m_keyMoved = false;     // 已用方向键移动过端点
    QTimer*   m_autoScrollTimer = nullptr;
    int       m_autoDx = 0;
    int       m_autoDy = 0;
    QPoint    m_lastMouse;
};
