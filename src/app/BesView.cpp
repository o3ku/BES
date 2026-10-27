// BesView.cpp
#include "app/BesView.h"
#include "app/BesDoc.h"
#include "app/MainWindow.h"
#include "app/Actions.h"
#include "app/ViewArea.h"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTimer>

#include <algorithm>
#include <climits>

using namespace bes;

namespace {
inline QColor FromColorRef(uint32_t c) {
    return QColor(static_cast<int>(c & 0xFF),
                  static_cast<int>((c >> 8) & 0xFF),
                  static_cast<int>((c >> 16) & 0xFF));
}
} // namespace

BesView::BesView(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    setFont(QFont(QStringLiteral("宋体"), 9));
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &BesView::OnVScroll);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &BesView::OnHScroll);
    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(60);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &BesView::OnAutoScrollTick);
    EnsureFont();
}

BesView::~BesView()
{
    if (m_doc)
        m_doc->RemoveView(this);
}

void BesView::SetDoc(BesDoc* doc)
{
    if (m_doc && m_doc != doc)
        m_doc->RemoveView(this);
    m_doc = doc;
    if (m_doc)
        m_doc->AddView(this);
    EnsureFont();
    UpdateScrollRange();
    viewport()->update();
}

void BesView::EnsureFont()
{
    QFontMetrics fm(font());
    m_charW = std::max(4, fm.horizontalAdvance(QLatin1Char('W')));
    m_charH = std::max(4, fm.height());
    const int zoom = (m_doc && m_doc->State().params.zoom > 0)
                     ? m_doc->State().params.zoom : 1;
    m_zoomW = m_charW * zoom;
    m_zoomH = m_charH * zoom;

    // 缩放时同时放大绘制字体（不仅是格间距）
    m_drawFont = font();
    if (font().pointSizeF() > 0)
        m_drawFont.setPointSizeF(font().pointSizeF() * zoom);
}

int BesView::CellBits() const
{
    return m_doc ? Renderer::BitsPerCell(m_doc->State().params.view) : 8;
}

int BesView::VisibleCols() const
{
    if (!m_doc) return 80;
    const int span = Renderer::CharsPerCell(m_doc->State().params.view);
    const int z = std::max(1, m_doc->State().params.zoom);
    const int cw = std::max(1, m_charW * z * span);
    const int w = viewport()->width() - m_yaxisW;
    return std::max(1, w / cw);
}

uint64_t BesView::FrameLenBits() const
{
    if (!m_doc) return 0;
    Renderer r;
    PrepareRenderer(r);
    return r.EffectiveFrameLen(VisibleCols());
}

void BesView::PrepareRenderer(Renderer& r) const
{
    if (!m_doc) return;
    const DocState& st = m_doc->State();
    r.SetData(&m_doc->Map());
    r.SetFrame(&st.frame);
    r.SetSelection(&st.sel);
    r.SetParams(st.params);
    r.SetColors(st.colors);
    r.SetVariableFrames(&st.varFrames);
    r.SetVisibleFrames(&st.visibleFrames, st.filterActive);
    r.SetColsPerRow(VisibleCols());
}

void BesView::UpdateScrollRange()
{
    EnsureFont();
    if (!m_doc || !m_doc->Map().IsOpen()) {
        verticalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setRange(0, 0);
        m_yaxisW = 0;
        return;
    }

    const DocState& st = m_doc->State();
    const bool frameRows = (st.params.layout != ViewLayout::Horizontal &&
                            st.params.layout != ViewLayout::GroupH);

    // 纵轴宽度：按纵轴模式的最大数值位数
    if (frameRows) {
        const uint64_t totalBits = m_doc->Map().SizeBits();
        uint64_t maxVal = 0;
        if (st.frame.bVariable && st.varFrames.Size() > 0) {
            switch (st.params.yAxis) {
            case YAxisMode::YFN: maxVal = st.varFrames.Size(); break;
            case YAxisMode::YFL:
                for (const VariableFrame& f : st.varFrames.Frames())
                    maxVal = std::max(maxVal, f.length);
                break;
            case YAxisMode::YFB:
                for (const VariableFrame& f : st.varFrames.Frames())
                    maxVal = std::max(maxVal, f.startBit);
                break;
            }
        } else {
            const uint64_t fl = m_doc->FrameLenBits(VisibleCols());
            const uint64_t frames = (fl && totalBits > st.frame.startBit)
                    ? ((totalBits - st.frame.startBit + fl - 1) / fl) : 0;
            switch (st.params.yAxis) {
            case YAxisMode::YFN: maxVal = frames; break;
            case YAxisMode::YFL: maxVal = fl;     break;
            case YAxisMode::YFB: maxVal = totalBits; break;
            }
        }
        int digits = 1;
        while (maxVal >= 10) { maxVal /= 10; ++digits; }
        m_yaxisW = std::min(std::max(m_zoomW * digits + 12, 48), 180);
    } else {
        // 横排同样保留左侧行号标尺：按数据行号/行宽/行首比特取最大位数
        const uint64_t bpc = static_cast<uint64_t>(
            Renderer::BitsPerCell(st.params.view));
        const uint64_t rowBits = static_cast<uint64_t>(VisibleCols()) * bpc;
        const uint64_t totalBits = m_doc->Map().SizeBits();
        const uint64_t base = std::min<uint64_t>(st.frame.startBit, totalBits);
        const uint64_t rows = (totalBits > base && rowBits > 0)
                ? (totalBits - base + rowBits - 1) / rowBits : 0;
        uint64_t maxVal = 0;
        switch (st.params.yAxis) {
        case YAxisMode::YFN: maxVal = rows;      break;
        case YAxisMode::YFL: maxVal = rowBits;   break;
        case YAxisMode::YFB: maxVal = totalBits; break;
        }
        int digits = 1;
        while (maxVal >= 10) { maxVal /= 10; ++digits; }
        m_yaxisW = std::min(std::max(m_zoomW * digits + 12, 48), 180);
    }

    const int visCols = VisibleCols();
    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(visCols);
    const Renderer::Geometry g = r.Compute(visCols);

    const int rulerH = st.params.showRuler ? m_zoomH : 0;
    const int titleH = st.params.showTitle ? m_zoomH : 0;
    // 上、下两条标尺
    const int visibleRows = std::max(1, (viewport()->height() - 2 * rulerH - titleH) /
                                        std::max(1, m_zoomH));

    const uint64_t maxRow = g.rows > static_cast<uint64_t>(visibleRows)
            ? g.rows - static_cast<uint64_t>(visibleRows) : 0;
    verticalScrollBar()->setRange(0, static_cast<int>(std::min<uint64_t>(maxRow, INT_MAX)));
    verticalScrollBar()->setPageStep(visibleRows);
    verticalScrollBar()->setSingleStep(1);
    if (m_scrollRow > maxRow)
        m_scrollRow = maxRow;
    {
        QSignalBlocker b(verticalScrollBar());
        verticalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(m_scrollRow, INT_MAX)));
    }

    // 水平滚动：长帧超过一屏时可平移；自动帧长时一屏即一帧（范围 0）
    uint64_t maxCells = g.cellsPerFrame;
    if (!frameRows)
        maxCells = static_cast<uint64_t>(visCols);
    const uint64_t hMax = maxCells > static_cast<uint64_t>(visCols)
            ? maxCells - static_cast<uint64_t>(visCols) : 0;
    horizontalScrollBar()->setRange(0, static_cast<int>(std::min<uint64_t>(hMax, INT_MAX)));
    horizontalScrollBar()->setPageStep(std::max(1, visCols));
    horizontalScrollBar()->setSingleStep(1);
    if (m_hOffset > hMax) m_hOffset = hMax;
    {
        QSignalBlocker b(horizontalScrollBar());
        horizontalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(m_hOffset, INT_MAX)));
    }

    m_scrollBit = r.RowStartBit(m_scrollRow);
}

uint64_t BesView::RowForBit(uint64_t bit) const
{
    if (!m_doc) return 0;
    const DocState& st = m_doc->State();
    const int visCols = VisibleCols();
    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(visCols);

    if (st.params.layout == ViewLayout::Horizontal ||
        st.params.layout == ViewLayout::GroupH) {
        const int bpc = Renderer::BitsPerCell(st.params.view);
        return (bit / static_cast<uint64_t>(bpc)) / static_cast<uint64_t>(std::max(1, visCols));
    }

    const uint64_t frameIdx = r.FrameIndexOfBit(bit);
    if (st.frame.bVariable && st.varFrames.Size() > 0)
        return std::min<uint64_t>(frameIdx, st.varFrames.Size() - 1);

    if (st.filterActive) {
        for (size_t i = 0; i < st.visibleFrames.size(); ++i)
            if (st.visibleFrames[i] == frameIdx) return static_cast<uint64_t>(i);
        return 0;
    }
    return frameIdx;
}

void BesView::SetScrollRowQuiet(uint64_t row)
{
    if (QScrollBar* sb = verticalScrollBar())
        row = std::min<uint64_t>(row, static_cast<uint64_t>(std::max(0, sb->maximum())));
    m_scrollRow = row;
    if (QScrollBar* sb = verticalScrollBar()) {
        QSignalBlocker b(sb);
        sb->setValue(static_cast<int>(std::min<uint64_t>(row, INT_MAX)));
    }
    if (m_doc) {
        Renderer r;
        PrepareRenderer(r);
        r.SetColsPerRow(VisibleCols());
        m_scrollBit = r.RowStartBit(row);
    }
    viewport()->update();
}

void BesView::SetHOffsetQuiet(uint64_t off)
{
    if (QScrollBar* sb = horizontalScrollBar())
        off = std::min<uint64_t>(off, static_cast<uint64_t>(std::max(0, sb->maximum())));
    m_hOffset = off;
    if (QScrollBar* sb = horizontalScrollBar()) {
        QSignalBlocker b(sb);
        sb->setValue(static_cast<int>(std::min<uint64_t>(off, INT_MAX)));
    }
    viewport()->update();
}

void BesView::SetScrollBit(uint64_t bit)
{
    if (!m_doc) return;
    m_scrollBit = bit;
    m_scrollRow = RowForBit(bit);
    UpdateScrollRange();
    {
        QSignalBlocker b(verticalScrollBar());
        verticalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(m_scrollRow, INT_MAX)));
    }
    viewport()->update();
}

void BesView::SyncToBit(uint64_t bit)
{
    if (!m_doc) return;
    m_scrollBit = bit;
    m_scrollRow = RowForBit(bit);
    {
        QSignalBlocker b(verticalScrollBar());
        verticalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(m_scrollRow, INT_MAX)));
    }
    viewport()->update();
}

void BesView::OnVScroll(int)
{
    if (!m_doc) return;
    const uint64_t prevRow = m_scrollRow;
    uint64_t row = static_cast<uint64_t>(verticalScrollBar()->value());
    const DocState& st = m_doc->State();
    if (st.params.tabFollow && st.params.yTab > 0) {
        row = (row / static_cast<uint64_t>(st.params.yTab)) *
              static_cast<uint64_t>(st.params.yTab);
        QSignalBlocker b(verticalScrollBar());
        verticalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(row, INT_MAX)));
    }
    m_scrollRow = row;

    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());
    m_scrollBit = r.RowStartBit(row);

    viewport()->update();

    LinkShift(static_cast<int64_t>(row) - static_cast<int64_t>(prevRow), 0);
}

void BesView::OnHScroll(int)
{
    if (!m_doc) return;
    const uint64_t prevOff = m_hOffset;
    uint64_t off = static_cast<uint64_t>(horizontalScrollBar()->value());
    const DocState& st = m_doc->State();
    if (st.params.tabFollow && st.params.xTab > 0) {
        off = (off / static_cast<uint64_t>(st.params.xTab)) *
              static_cast<uint64_t>(st.params.xTab);
        QSignalBlocker b(horizontalScrollBar());
        horizontalScrollBar()->setValue(static_cast<int>(std::min<uint64_t>(off, INT_MAX)));
    }
    m_hOffset = off;
    viewport()->update();

    LinkShift(0, static_cast<int64_t>(off) - static_cast<int64_t>(prevOff));
}

// 联动浏览（原版 LockScrollBtn，[form+0x3C8]，按下态 [+0x83]）：
// 原版 0x407B4C(当前视图, 输出列表) 先 Clear 列表，仅在按钮按下时收集同步对象 ——
// 对象 = 当前视图所在停靠容器（须为 TdxSideContainerDockSite）里的其它视图，
// 且排除触发滚动的视图本身；三个调用点（滚轮 0x405C34、滚动条通知 0x4061C6/0x4064A9）
// 都是把「本次移动量」加到每个成员自己的位置上，而不是把它们拉到同一个绝对位置。
// 现版所有视图同在一个视图区（ViewArea，等价于同一个停靠容器），故同步集合 = 其它视图。
void BesView::LinkShift(int64_t dRow, int64_t dCol)
{
    if (dRow == 0 && dCol == 0) return;
    auto* mw = qobject_cast<MainWindow*>(window());
    if (!mw || !mw->LinkBrowse()) return;
    auto* area = qobject_cast<ViewArea*>(mw->centralWidget());
    if (!area) return;
    const int cur = area->indexOf(this);
    for (int i = 0; i < area->count(); ++i) {
        if (i == cur) continue;
        auto* v = dynamic_cast<BesView*>(area->widget(i));
        if (!v) continue;
        if (dRow != 0) {
            const int64_t row = static_cast<int64_t>(v->ScrollRow()) + dRow;
            v->SetScrollRowQuiet(static_cast<uint64_t>(row > 0 ? row : 0));
        }
        if (dCol != 0) {
            const int64_t col = static_cast<int64_t>(v->HOffset()) + dCol;
            v->SetHOffsetQuiet(static_cast<uint64_t>(col > 0 ? col : 0));
        }
    }
}

void BesView::resizeEvent(QResizeEvent* ev)
{
    QAbstractScrollArea::resizeEvent(ev);
    EnsureFont();
    UpdateScrollRange();
}

void BesView::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    if (!m_doc || !m_doc->Map().IsOpen()) {
        p.fillRect(viewport()->rect(), Qt::white);
        return;
    }

    EnsureFont();
    const DocState& st = m_doc->State();
    Renderer r;
    PrepareRenderer(r);
    // 拖动中显示待提交选区（原版移动时只刷新，记录在松开时才写入列表）
    SelectionSet preview;
    if (m_dragging && m_hasAnchor) {
        BuildPreview(preview);
        r.SetSelection(&preview);
    }
    const int visCols = VisibleCols();

    const QRect rc = viewport()->rect();
    p.fillRect(rc, FromColorRef(st.colors.background));

    const int rulerH = st.params.showRuler ? m_zoomH : 0;
    const int titleH = st.params.showTitle ? m_zoomH : 0;
    const int yaxisW = m_yaxisW;

    std::vector<Cell> cells;
    uint64_t totalRows = 0, totalCols = 0;
    r.Layout(rc.width() - yaxisW, rc.height() - 2 * rulerH - titleH,
             m_charW, m_charH, m_scrollRow, m_hOffset, cells, totalRows, totalCols);

    for (Cell& c : cells) {
        c.x += yaxisW;
        c.y += rulerH + titleH;
    }

    p.setFont(m_drawFont);

    // ---- 内容 ----
    QColor body = FromColorRef(st.colors.body);
    for (Cell& c : cells) {
        r.FillCell(c);
        switch (st.params.view) {
        case ViewMode::Pixel: {
            p.fillRect(c.x, c.y, c.w, c.h, QColor(c.byteVal, c.byteVal, c.byteVal));
            break;
        }
        case ViewMode::BitBitmap:
            if (c.isSet)
                p.fillRect(c.x, c.y, c.w, c.h, body);
            break;
        case ViewMode::Hex: {
            QString t;
            t += QChar(c.ch);
            t += QChar(c.ch2);
            p.setPen(body);
            p.drawText(QRect(c.x, c.y, c.w, c.h), Qt::AlignLeft | Qt::AlignVCenter, t);
            break;
        }
        case ViewMode::HexChar: {
            QString t;
            t += QChar(c.ch);
            t += QChar(c.ch2);
            t += QLatin1Char(' ');
            t += QChar(c.ch3);
            p.setPen(body);
            p.drawText(QRect(c.x, c.y, c.w, c.h), Qt::AlignLeft | Qt::AlignVCenter, t);
            break;
        }
        default:
            if (c.ch) {
                p.setPen(body);
                p.drawText(QRect(c.x, c.y, c.w, c.h), Qt::AlignCenter, QString(QChar(c.ch)));
            }
            break;
        }
    }

    // ---- 选区（半透明覆盖，不遮挡内容）----
    QColor selColor = FromColorRef(st.colors.selection);
    selColor.setAlpha(90);
    for (const Cell& c : cells) {
        if (r.IsSelected(c.bitPos))
            p.fillRect(c.x, c.y, c.w, c.h, selColor);
    }

    // ---- XTAB / YTAB 分割线（与数据坐标对齐）----
    // 只由用户的 XTAB/YTAB 开关决定：原版分组排布仅重插停靠站点，不改线与开关。
    const bool drawX = st.params.showXTab;
    const bool drawY = st.params.showYTab;
    const int colsPerRow = r.ColsPerRow();
    const int gridTop = rulerH + titleH;
    const int gridBottom = rc.height() - rulerH;   // 下标尺占底部
    if (drawX && st.params.xTab > 0 && !cells.empty()) {
        QPen pen(FromColorRef(st.colors.tab));
        p.setPen(pen);
        for (size_t i = 0; i < cells.size(); ++i) {
            const int col = static_cast<int>(i % static_cast<size_t>(std::max(1, colsPerRow)));
            const int xStep = st.params.xStep > 0 ? st.params.xStep : 1;
            const uint64_t absCol = m_hOffset +
                static_cast<uint64_t>(col) * static_cast<uint64_t>(xStep);
            if (absCol % static_cast<uint64_t>(st.params.xTab) == 0)
                p.drawLine(cells[i].x, gridTop, cells[i].x, gridBottom);
        }
    }
    if (drawY && st.params.yTab > 0) {
        QPen pen(FromColorRef(st.colors.tab));
        p.setPen(pen);
        const int rowsVisible = (gridBottom - gridTop) / std::max(1, m_zoomH) + 1;
        const int yStep = st.params.yStep > 0 ? st.params.yStep : 1;
        for (int i = 0; i < rowsVisible; ++i) {
            const uint64_t absRow = m_scrollRow + static_cast<uint64_t>(i * yStep);
            if (absRow % static_cast<uint64_t>(st.params.yTab) == 0) {
                const int y = gridTop + i * m_zoomH;
                p.drawLine(yaxisW, y, rc.width(), y);
            }
        }
    }

    // ---- 左侧行号标尺（YFN/YFL/YFB 行标注；横排为数据行号）----
    if (yaxisW > 0) {
        p.fillRect(0, gridTop, yaxisW - 4, gridBottom - gridTop,
                   FromColorRef(st.colors.ruler));
        p.setPen(Qt::white);
        const int rowsVisible = (gridBottom - gridTop) / std::max(1, m_zoomH) + 1;
        wchar_t buf[40];
        const int yStep = st.params.yStep > 0 ? st.params.yStep : 1;
        for (int i = 0; i < rowsVisible; ++i) {
            const uint64_t rowIdx = m_scrollRow + static_cast<uint64_t>(i * yStep);
            if (rowIdx >= totalRows) break;
            if (!r.YAxisText(rowIdx, buf, 40)) break;
            p.drawText(QRect(2, gridTop + i * m_zoomH, yaxisW - 8, m_zoomH),
                       Qt::AlignVCenter | Qt::AlignRight,
                       QString::fromWCharArray(buf));
        }
        if (rulerH > 0)
            p.drawText(QRect(2, 0, yaxisW - 8, rulerH), Qt::AlignVCenter,
                       QStringLiteral("行/列"));
    }

    // ---- 标尺（顶部/底部：列坐标）----
    if (st.params.showRuler) {
        const int span = Renderer::CharsPerCell(st.params.view);
        const int cellPx = m_zoomW * span;
        const int xStep = st.params.xStep > 0 ? st.params.xStep : 1;
        const int bpc = Renderer::BitsPerCell(st.params.view);
        const uint64_t cpf = r.CellsPerFrame(r.ColsPerRow());
        const uint64_t colsInRow = (cpf > m_hOffset) ? (cpf - m_hOffset) : 0;
        const uint64_t maxCol = std::min<uint64_t>(static_cast<uint64_t>(visCols), colsInRow);
        auto drawRuler = [&](int y, bool bottom) {
            p.fillRect(0, y, rc.width(), rulerH, FromColorRef(st.colors.ruler));
            p.setPen(Qt::white);
            const QFontMetrics fm = p.fontMetrics();
            int lastLabelRight = -1;   // 上一个数字标签的右边界（含间距）
            for (uint64_t c = 0; c < maxCol; ++c) {
                const uint64_t absCol = m_hOffset + c * static_cast<uint64_t>(xStep);
                if (absCol >= cpf) break;
                const int x = yaxisW + static_cast<int>(c) * cellPx;
                if (x >= rc.width()) break;
                const uint64_t bitPos = absCol * static_cast<uint64_t>(bpc);
                if (bitPos % 8 != 0) continue;
                // 8 比特刻度：比特位图 8 格一刻，字节模式每格一刻（随模式/缩放变化）
                const int ty = bottom ? y : y + rulerH - 4;
                p.drawLine(x, ty, x, ty + 3);
                // 数字按实际字宽避让：字节模式每格都是 8 比特边界，
                // 不能每格都写标签，否则数字互相重叠
                const QString label = QString::number(bitPos);
                if (x < lastLabelRight) continue;
                const int tw = fm.horizontalAdvance(label);
                p.drawText(QRect(x, y, tw + 2, rulerH), Qt::AlignVCenter | Qt::AlignLeft,
                           label);
                lastLabelRight = x + tw + 6;
            }
        };
        drawRuler(0, false);
        drawRuler(rc.height() - rulerH, true);
    }

    // ---- 标题（布局=标题）----
    if (titleH > 0) {
        const uint64_t start = r.RowStartBit(m_scrollRow);
        const uint64_t len = r.FrameLenOfRow(m_scrollRow);
        const uint64_t idx = r.FrameIndexOfBit(start);
        p.fillRect(0, rulerH, rc.width(), titleH, FromColorRef(st.colors.background));
        p.setPen(body);
        p.drawText(QRect(yaxisW, rulerH, rc.width() - yaxisW, titleH),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("帧 %1    帧长 %2 比特%3    起点 %4")
                       .arg(idx).arg(len)
                       .arg(st.frame.frameLen == 0 ? QStringLiteral("（自动）") : QString())
                       .arg(start));
        p.setPen(FromColorRef(st.colors.tab));
        p.drawLine(0, rulerH + titleH - 1, rc.width(), rulerH + titleH - 1);
    }

    // ---- 书签 / 标记 / 光标 ----
    const auto& marks = st.marks.All();
    auto markInCell = [&marks](const Cell& c) -> bool {
        auto it = marks.lower_bound(c.bitPos);
        return it != marks.end() &&
               it->first < c.bitPos + static_cast<uint64_t>(c.bitLen);
    };
    const uint64_t cursor = m_doc->CursorBit();
    const uint64_t startBit = st.frame.startBit;
    for (const Cell& c : cells) {
        const uint64_t cellEnd = c.bitPos + static_cast<uint64_t>(c.bitLen);
        if (markInCell(c)) {
            QPen pen(FromColorRef(st.colors.bookmark));
            pen.setWidth(2);
            p.setPen(pen);
            p.drawRect(c.x + 1, c.y + 1, c.w - 2, c.h - 2);
        }
        if (cursor >= c.bitPos && cursor < cellEnd) {
            QPen pen(FromColorRef(st.colors.marker));
            pen.setWidth(2);
            p.setPen(pen);
            p.drawRect(c.x + 1, c.y + 1, c.w - 3, c.h - 3);
        }
        if (startBit >= c.bitPos && startBit < cellEnd) {
            QPen pen(FromColorRef(st.colors.marker));
            pen.setWidth(2);
            p.setPen(pen);
            p.drawLine(c.x + 1, c.y + c.h - 2, c.x + c.w - 2, c.y + c.h - 2);
        }
    }
}

bool BesView::HitBit(const QPoint& pt, uint64_t& bit) const
{
    if (!m_doc) return false;
    const QRect rc = viewport()->rect();
    const DocState& st = m_doc->State();
    const int rulerH = st.params.showRuler ? m_zoomH : 0;
    const int titleH = st.params.showTitle ? m_zoomH : 0;
    const int contentW = rc.width() - m_yaxisW;
    const int contentH = rc.height() - 2 * rulerH - titleH;
    if (contentW <= 0 || contentH <= 0) return false;

    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());
    return r.HitTest(pt.x() - m_yaxisW, pt.y() - rulerH - titleH,
                     contentW, contentH, m_charW, m_charH,
                     m_scrollRow, m_hOffset, bit);
}

// 原版交互（0x443840 按下 / 0x443DE0 移动 / 0x444924 松开）：
//  · 按下只记锚点与当前端点（obj+0x54..+0x68），拖动只更新当前端点；
//  · 松开时把这一段作为一条记录追加进当前模式的 TList（0x444924），
//    因此默认就是「累积多次拖动」，与 Ctrl 无关（鼠标处理里没有 GetAsyncKeyState）；
//  · 连续模式按起点排序插入（Sort/comparer 0x4033AC），行/列合并相邻区间。
void BesView::CommitSelection(uint64_t bit)
{
    if (!m_doc) return;
    DocState& st = m_doc->State();
    const SelectMode mode = st.sel.Mode();
    if (mode == SelectMode::None) {
        m_doc->SetCursorBit(bit);
        m_doc->UpdateAllViews();
        return;
    }

    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());

    switch (mode) {
    case SelectMode::Continuous:
        st.sel.AddContinuous(std::min(m_anchorBit, bit), std::max(m_anchorBit, bit));
        break;
    case SelectMode::Rect: {
        RectSel rr;
        rr.frame0 = std::min(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(bit));
        rr.frame1 = std::max(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(bit));
        rr.cell0  = std::min(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(bit));
        rr.cell1  = std::max(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(bit));
        st.sel.AddRect(rr);
        break;
    }
    case SelectMode::Row: {
        const uint64_t f0 = std::min(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(bit));
        const uint64_t f1 = std::max(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(bit));
        if (f0 == f1)
            st.sel.ToggleRow(f0);   // 单击已选行 = 取消该行
        else
            st.sel.AddRowRange(f0, std::min<uint64_t>(f1, f0 + 99999));
        break;
    }
    case SelectMode::Col: {
        const uint64_t c0 = std::min(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(bit));
        const uint64_t c1 = std::max(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(bit));
        if (c0 == c1)
            st.sel.ToggleCol(c0);   // 单击已选列 = 取消该列
        else
            st.sel.AddColRange(c0, std::min<uint64_t>(c1, c0 + 99999));
        break;
    }
    default:
        break;
    }

    m_doc->SetCursorBit(bit);
    m_doc->UpdateAllViews();
}

// 拖动中的「待提交」预览：不改文档状态，只把锚点..当前端点叠加到绘制用的副本。
void BesView::BuildPreview(SelectionSet& out) const
{
    out = m_doc->State().sel;
    const SelectMode mode = out.Mode();
    if (mode == SelectMode::None || !m_hasAnchor) return;

    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());

    switch (mode) {
    case SelectMode::Continuous:
        out.AddContinuous(std::min(m_anchorBit, m_dragBit), std::max(m_anchorBit, m_dragBit));
        break;
    case SelectMode::Rect: {
        RectSel rr;
        rr.frame0 = std::min(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(m_dragBit));
        rr.frame1 = std::max(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(m_dragBit));
        rr.cell0  = std::min(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(m_dragBit));
        rr.cell1  = std::max(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(m_dragBit));
        out.AddRect(rr);
        break;
    }
    case SelectMode::Row: {
        const uint64_t f0 = std::min(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(m_dragBit));
        const uint64_t f1 = std::max(r.FrameIndexOfBit(m_anchorBit), r.FrameIndexOfBit(m_dragBit));
        out.AddRowRange(f0, std::min<uint64_t>(f1, f0 + 99999));
        break;
    }
    case SelectMode::Col: {
        const uint64_t c0 = std::min(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(m_dragBit));
        const uint64_t c1 = std::max(r.CellColumnOfBit(m_anchorBit), r.CellColumnOfBit(m_dragBit));
        out.AddColRange(c0, std::min<uint64_t>(c1, c0 + 99999));
        break;
    }
    default:
        break;
    }
}

// Shift+导航键的端点移动（原版 KeyDown@0x445B0C：行 ±1 / PgUp/PgDn ±可视化行数、
// 列 ±1 / Home=行首 / End=行尾；纵向移动保持列，横向移动保持行）。
void BesView::BeginKeyboardSelection()
{
    if (!m_doc) return;
    m_keySelecting = true;
    m_keyMoved = false;
    m_hasAnchor = true;
    m_anchorBit = m_dragBit = m_doc->CursorBit();
}

void BesView::MoveKeyboardEndpoint(int dRow, int dCol, int absCol)
{
    if (!m_doc) return;
    const DocState& st = m_doc->State();
    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());

    const uint64_t totalBits = m_doc->Map().SizeBits();
    const uint64_t totalRows = r.Compute(VisibleCols()).rows;
    if (totalBits == 0 || totalRows == 0) return;

    const int bpc = Renderer::BitsPerCell(st.params.view);
    const uint64_t ref = m_keyMoved ? m_dragBit : m_anchorBit;
    uint64_t row = RowForBit(ref);
    uint64_t rowStart = r.RowStartBit(row);
    const uint64_t col = ref > rowStart ? (ref - rowStart) / static_cast<uint64_t>(bpc) : 0;

    int64_t nr = static_cast<int64_t>(row) + dRow;
    if (nr < 0) nr = 0;
    if (nr >= static_cast<int64_t>(totalRows)) nr = static_cast<int64_t>(totalRows) - 1;
    row = static_cast<uint64_t>(nr);

    rowStart = r.RowStartBit(row);
    uint64_t maxCol = 0;
    if (rowStart < totalBits) {
        const uint64_t len = r.FrameLenOfRow(row);
        maxCol = (len + static_cast<uint64_t>(bpc) - 1) / static_cast<uint64_t>(bpc);
        maxCol = maxCol > 0 ? maxCol - 1 : 0;
        const uint64_t lastCol = (totalBits - rowStart - 1) / static_cast<uint64_t>(bpc);
        if (lastCol < maxCol) maxCol = lastCol;
    }

    int64_t nc = static_cast<int64_t>(col);
    if (absCol == 0)      nc = 0;
    else if (absCol == 1) nc = static_cast<int64_t>(maxCol);
    else                  nc += dCol;
    if (nc < 0) nc = 0;
    if (nc > static_cast<int64_t>(maxCol)) nc = static_cast<int64_t>(maxCol);

    m_dragBit = rowStart + static_cast<uint64_t>(nc) * static_cast<uint64_t>(bpc);
    m_keyMoved = true;
    m_dragging = true;   // 仅用于绘制预览，松开/取消时清除
    m_doc->SetCursorBit(m_dragBit);
    m_doc->UpdateAllViews();
    EnsureBitVisible(m_dragBit);
    viewport()->update();
}

// 无 Shift 的方向键：移动光标（原版 +0x54/+0x5c），并清掉当前模式的列表（0x4404CC dl=0）。
// 左/右在行首/行尾折返到相邻行（0x4463D8 / 0x446417）；行越界钳到首/末行。
void BesView::MoveCaret(int dRow, int dCol, int absCol)
{
    if (!m_doc) return;
    DocState& st = m_doc->State();
    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());

    const uint64_t totalBits = m_doc->Map().SizeBits();
    const uint64_t totalRows = r.Compute(VisibleCols()).rows;
    if (totalBits == 0 || totalRows == 0) return;

    const int bpc = Renderer::BitsPerCell(st.params.view);
    const uint64_t bit = m_doc->CursorBit();
    uint64_t row = RowForBit(bit);
    uint64_t rowStart = r.RowStartBit(row);
    int64_t col = bit > rowStart ? static_cast<int64_t>((bit - rowStart) / static_cast<uint64_t>(bpc)) : 0;

    auto lastColOf = [&](uint64_t rr) -> int64_t {
        const uint64_t rs = r.RowStartBit(rr);
        if (rs >= totalBits) return 0;
        const uint64_t len = r.FrameLenOfRow(rr);
        uint64_t last = (len + static_cast<uint64_t>(bpc) - 1) / static_cast<uint64_t>(bpc);
        last = last > 0 ? last - 1 : 0;
        const uint64_t endCol = (totalBits - rs - 1) / static_cast<uint64_t>(bpc);
        if (endCol < last) last = endCol;
        return static_cast<int64_t>(last);
    };

    int64_t nr = static_cast<int64_t>(row) + dRow;
    if (nr < 0) nr = 0;
    if (nr >= static_cast<int64_t>(totalRows)) nr = static_cast<int64_t>(totalRows) - 1;

    if (absCol == 0) {
        col = 0;
    } else if (absCol == 1) {
        col = lastColOf(static_cast<uint64_t>(nr));
    } else if (dCol != 0) {
        col += dCol;
        if (col < 0) {
            if (nr > 0) { --nr; col = lastColOf(static_cast<uint64_t>(nr)); }
            else col = 0;
        } else if (col > lastColOf(static_cast<uint64_t>(nr))) {
            if (nr + 1 < static_cast<int64_t>(totalRows)) { ++nr; col = 0; }
            else col = lastColOf(static_cast<uint64_t>(nr));
        }
    }
    if (col < 0) col = 0;

    row = static_cast<uint64_t>(nr);
    rowStart = r.RowStartBit(row);
    const int64_t maxCol = lastColOf(row);
    if (col > maxCol) col = maxCol;

    const uint64_t newBit = rowStart + static_cast<uint64_t>(col) * static_cast<uint64_t>(bpc);
    if (st.sel.Mode() != SelectMode::None && !st.sel.Empty())
        st.sel.ClearMode(st.sel.Mode());
    m_doc->SetCursorBit(newBit);
    m_doc->UpdateAllViews();
    EnsureBitVisible(newBit);
    viewport()->update();
}

void BesView::EndKeyboardSelection(bool commit)
{
    if (!m_keySelecting) return;
    m_keySelecting = false;
    const bool moved = m_keyMoved;
    m_keyMoved = false;
    m_dragging = false;
    if (commit && moved) CommitSelection(m_dragBit);
    else viewport()->update();
}

void BesView::EnsureBitVisible(uint64_t bit)
{
    if (!m_doc) return;
    const uint64_t row = RowForBit(bit);
    QScrollBar* vs = verticalScrollBar();
    QScrollBar* hs = horizontalScrollBar();
    const int visRows = std::max(1, vs->pageStep());
    if (row < m_scrollRow)
        vs->setValue(static_cast<int>(std::min<uint64_t>(row, INT_MAX)));
    else if (row >= m_scrollRow + static_cast<uint64_t>(visRows))
        vs->setValue(static_cast<int>(std::min<uint64_t>(row - visRows + 1, INT_MAX)));

    Renderer r;
    PrepareRenderer(r);
    r.SetColsPerRow(VisibleCols());
    const int bpc = Renderer::BitsPerCell(m_doc->State().params.view);
    const uint64_t rowStart = r.RowStartBit(row);
    const uint64_t col = bit > rowStart ? (bit - rowStart) / static_cast<uint64_t>(bpc) : 0;
    const uint64_t visCols = static_cast<uint64_t>(std::max(1, VisibleCols()));
    if (col < m_hOffset)
        hs->setValue(static_cast<int>(std::min<uint64_t>(col, INT_MAX)));
    else if (col >= m_hOffset + visCols)
        hs->setValue(static_cast<int>(std::min<uint64_t>(col - visCols + 1, INT_MAX)));
}

// 原版 0x443F5C 用 PtInRect 判断鼠标是否还在客户区：不在则置 +0x52 并启动定时器，
// 定时按行/列滚动并把端点跟到最近格的边缘。这里用 60ms 的 QTimer 等价实现。
void BesView::UpdateAutoScroll(const QPoint& pos)
{
    const QRect rc = viewport()->rect();
    m_autoDx = pos.x() < 0 ? -1 : (pos.x() >= rc.width() ? 1 : 0);
    m_autoDy = pos.y() < 0 ? -1 : (pos.y() >= rc.height() ? 1 : 0);
    if (m_autoDx != 0 || m_autoDy != 0) {
        if (!m_autoScrollTimer->isActive()) m_autoScrollTimer->start();
    } else {
        m_autoScrollTimer->stop();
    }
}

void BesView::OnAutoScrollTick()
{
    if (!m_dragging || !m_hasAnchor || !m_doc || m_keySelecting) {
        m_autoScrollTimer->stop();
        return;
    }
    if (m_autoDy)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + m_autoDy);
    if (m_autoDx)
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + m_autoDx);

    const QRect rc = viewport()->rect();
    if (rc.width() <= m_yaxisW || rc.height() <= 0) return;
    QPoint p(m_lastMouse);
    p.setX(std::max(m_yaxisW, std::min(p.x(), rc.width() - 1)));
    p.setY(std::max(0, std::min(p.y(), rc.height() - 1)));
    uint64_t bit = 0;
    if (HitBit(p, bit)) {
        m_dragBit = bit;
        m_doc->SetCursorBit(bit);
        m_doc->UpdateAllViews();
        viewport()->update();
    }
}

void BesView::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        if (m_keySelecting) EndKeyboardSelection(false);
        uint64_t bit = 0;
        if (HitBit(ev->pos(), bit)) {
            m_dragging = true;
            m_anchorBit = bit;
            m_dragBit = bit;
            m_hasAnchor = true;
            m_lastMouse = ev->pos();
            m_autoScrollTimer->stop();
            m_doc->SetCursorBit(bit);
            viewport()->update();
        }
    }
    QAbstractScrollArea::mousePressEvent(ev);
}

void BesView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton && m_dragging) {
        // 松开才提交；拖动出视口时仍用最后记录的端点
        m_autoScrollTimer->stop();
        m_autoDx = m_autoDy = 0;
        CommitSelection(m_hasAnchor ? m_dragBit : 0);
        m_dragging = false;
        viewport()->update();
    }
    QAbstractScrollArea::mouseReleaseEvent(ev);
}

void BesView::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging && m_hasAnchor && (ev->buttons() & Qt::LeftButton) && !m_keySelecting) {
        m_lastMouse = ev->pos();
        uint64_t bit = 0;
        if (HitBit(ev->pos(), bit)) {
            m_dragBit = bit;
            m_doc->SetCursorBit(bit);
            viewport()->update();
        }
        UpdateAutoScroll(ev->pos());
    }
    QAbstractScrollArea::mouseMoveEvent(ev);
}

QMenu* BesView::CreateContextMenu()
{
    auto* mw = qobject_cast<MainWindow*>(window());
    if (!mw) return nullptr;
    Actions* A = mw->Cmds();
    if (!A) return nullptr;

    // 顺序与分隔完全按 DFM 中隐藏栏「快捷操作」的 ItemLinks
    QMenu* menu = new QMenu(this);
    auto add = [&](int id) {
        if (QAction* a = A->Find(id)) menu->addAction(a);
    };
    add(CMD_VIEW_LINK);
    add(CMD_SET_START);
    menu->addSeparator();
    add(CMD_SELECT_CONTINUOUS);
    add(CMD_SELECT_RECT);
    add(CMD_SELECT_ROW);
    add(CMD_SELECT_COL);
    QMenu* sel = menu->addMenu(QStringLiteral("选取参数"));
    if (QAction* a = A->Find(CMD_SELECT_LOAD)) sel->addAction(a);
    if (QAction* a = A->Find(CMD_SELECT_SAVE)) sel->addAction(a);
    sel->addSeparator();
    if (QAction* a = A->Find(CMD_SELECT_CLEAR_CURRENT)) sel->addAction(a);
    if (QAction* a = A->Find(CMD_SELECT_CLEAR_ALL)) sel->addAction(a);
    menu->addSeparator();
    add(CMD_DATA_OUTPUT);
    add(CMD_FILE_SAVE_AS);
    menu->addSeparator();
    add(CMD_ANALYZE_SEARCH_FL);
    add(CMD_ANALYZE_SEARCH_UNIQUE);
    add(CMD_ANALYZE_QUIT_SEARCH);
    menu->addSeparator();
    add(CMD_BOOKMARK_TOGGLE);
    add(CMD_BOOKMARK_NEXT);
    add(CMD_BOOKMARK_PREV);
    add(CMD_BOOKMARK_CLEAR);
    menu->addSeparator();
    add(CMD_ANALYZE_GOTO);
    return menu;
}

void BesView::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = CreateContextMenu();
    if (!menu) {
        ev->ignore();
        return;
    }
    menu->exec(ev->globalPos());
    delete menu;
    ev->accept();
}

void BesView::wheelEvent(QWheelEvent* ev)
{
    // Ctrl+滚轮 = 缩放
    if ((ev->modifiers() & Qt::ControlModifier) && m_doc) {
        RenderParams& p = m_doc->State().params;
        p.zoom = (ev->angleDelta().y() > 0)
                ? std::min(16, p.zoom * 2)
                : std::max(1, p.zoom / 2);
        EnsureFont();
        UpdateScrollRange();
        m_doc->UpdateAllViews();
        ev->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(ev);
}

void BesView::keyPressEvent(QKeyEvent* ev)
{
    if (!m_doc) { QAbstractScrollArea::keyPressEvent(ev); return; }

    // Shift 松开事件丢失（焦点切换等）时，用下一次按键补提交
    if (m_keySelecting && !(ev->modifiers() & Qt::ShiftModifier))
        EndKeyboardSelection(true);
    if (ev->key() == Qt::Key_Escape && m_keySelecting) {
        EndKeyboardSelection(false);
        ev->accept();
        return;
    }

    const SelectMode mode = m_doc->State().sel.Mode();
    const bool shift = (ev->modifiers() & Qt::ShiftModifier) != 0;

    // 原版 KeyDown@0x445B0C：Shift 置 +0x51，方向/PgUp/PgDn/Home/End 移动当前端点；
    // 无选取模式时这些键在 Shift 下不动作（模式 -1 早退）。
    if (shift) {
        int dr = 0, dc = 0, absCol = -1;
        const int page = std::max(1, verticalScrollBar()->pageStep());
        switch (ev->key()) {
        case Qt::Key_Up:       dr = -1; break;
        case Qt::Key_Down:     dr = +1; break;
        case Qt::Key_Left:     dc = -1; break;
        case Qt::Key_Right:    dc = +1; break;
        case Qt::Key_Home:     absCol = 0; break;
        case Qt::Key_End:      absCol = 1; break;   // 1 = 行尾
        case Qt::Key_PageUp:   dr = -page; break;
        case Qt::Key_PageDown: dr = +page; break;
        default: break;
        }
        if (dr != 0 || dc != 0 || absCol >= 0) {
            if (mode != SelectMode::None) {
                if (!m_keySelecting) BeginKeyboardSelection();
                MoveKeyboardEndpoint(dr, dc, absCol);
            }
            ev->accept();
            return;
        }
    }

    QScrollBar* vs = verticalScrollBar();
    QScrollBar* hs = horizontalScrollBar();
    const int page = std::max(1, vs->pageStep());

    // 原版 KeyDown@0x445B0C 尾段 0x44611D：无 Shift 时方向键移动光标（标记），
    // 并调用 0x4404CC(dl=0) 清掉当前模式的列表；Up/Down 行 ±1、Left/Right 列 ±1
    // （左/右越界时在相邻行折返）、Home/End 行首/行尾、PgUp/PgDn ±可视化行数。
    switch (ev->key()) {
    case Qt::Key_Up:       MoveCaret(-1, 0, -1); break;
    case Qt::Key_Down:     MoveCaret(+1, 0, -1); break;
    case Qt::Key_Left:     MoveCaret(0, -1, -1); break;
    case Qt::Key_Right:    MoveCaret(0, +1, -1); break;
    case Qt::Key_Home:     MoveCaret(0, 0, 0); break;
    case Qt::Key_End:      MoveCaret(0, 0, 1); break;
    case Qt::Key_PageUp:   MoveCaret(-page, 0, -1); break;
    case Qt::Key_PageDown: MoveCaret(+page, 0, -1); break;
    case Qt::Key_Insert:
        if (auto* mw = qobject_cast<MainWindow*>(window()))
            mw->ToggleInsertMode();
        break;
    default:
        QAbstractScrollArea::keyPressEvent(ev);
        return;
    }
}

void BesView::keyReleaseEvent(QKeyEvent* ev)
{
    // 原版 KeyUp@0x4477DC：松开 Shift 直接调用 MouseUp（0x444924）提交待定记录
    if (ev->key() == Qt::Key_Shift && m_keySelecting) {
        EndKeyboardSelection(true);
        ev->accept();
        return;
    }
    QAbstractScrollArea::keyReleaseEvent(ev);
}
