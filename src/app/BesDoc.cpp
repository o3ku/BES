// BesDoc.cpp
#include "app/BesDoc.h"
#include "app/BesView.h"

#include <algorithm>

BesDoc::BesDoc(QObject* parent)
    : QObject(parent), m_editor(m_map)
{
    m_state.frame.startBit = 0;
    m_state.frame.frameLen = 0;   // 0 = 按屏宽自动
    m_state.frame.step     = 1;
    m_state.params.view    = bes::ViewMode::Hex;
    // 原版初始选取模式为 -1（四个选取按钮均未按下），鼠标不产生选区，
    // 直到用户点选「连续/块/行/列选取」（SelectBtnClick@0x40AC88）。
}

BesDoc::~BesDoc()
{
    m_map.Close();
}

bool BesDoc::OpenFile(const QString& path, QString* err)
{
    std::wstring werr;
    if (!m_map.Open(path.toStdWString(), &werr)) {
        if (err) *err = QString::fromStdWString(werr);
        return false;
    }
    m_path = path;

    m_state.frame.startBit = 0;
    // 与原版打开文件后的默认值一致：帧长 64 比特、比特字符显示
    const uint64_t bits = m_map.SizeBits();
    m_state.frame.frameLen = bits > 0 ? std::min<uint64_t>(64, bits) : 0;
    m_state.frame.step     = 1;
    m_state.params.view    = bes::ViewMode::BitChar;
    m_state.frame.bVariable = false;
    m_state.varFrames.Clear();
    m_state.filter = bes::FrameFilter();
    m_state.visibleFrames.clear();
    m_state.filterActive = false;
    m_state.sel.ClearAll();   // 模式也随之回到 None（原版 -1）
    m_state.marks.Clear();
    m_state.editMode = false;
    m_state.pendingNibble = -1;
    m_editor.Reset();

    m_cursorBit = 0;
    return true;
}

void BesDoc::Close()
{
    m_map.Close();
}

bool BesDoc::EnterEdit(QString* err)
{
    if (m_state.editMode)
        return true;
    if (!m_map.IsOpen()) {
        if (err) *err = QStringLiteral("没有打开的文件");
        return false;
    }
    // 已经是内存副本时不重读磁盘：退出编辑时选择「不保存」留下未写回的改动，
    // 再次进入编辑必须能看到它们（原版脏标志 [engine+0x1a4] 一直保留）
    if (!m_map.IsEditable()) {
        std::wstring werr;
        if (!m_map.OpenEditable(m_path.toStdWString(), &werr)) {
            if (err) *err = QString::fromStdWString(werr);
            return false;
        }
        m_editor.Reset();
    }
    m_state.editMode = true;
    m_state.pendingNibble = -1;
    return true;
}

void BesDoc::LeaveEdit()
{
    m_state.editMode = false;
    m_state.pendingNibble = -1;
    // 未保存的内存副本留在原地：再按「保存」仍可回写（原版退出编辑不丢弃脏数据）
}

bool BesDoc::SaveEdits(QString* err)
{
    std::wstring werr;
    if (!m_editor.Save(&werr)) {
        if (err) *err = QString::fromStdWString(werr);
        return false;
    }
    return true;
}

bool BesDoc::SaveFile(QString* err)
{
    if (m_map.IsEditable())
        return SaveEdits(err);
    if (!m_map.Flush()) {
        if (err) *err = QStringLiteral("保存文件失败！");
        return false;
    }
    return true;
}

void BesDoc::SetSelectMode(bes::SelectMode m)
{
    // 原版 SelectBtnClick@0x40AC88 + SetSelectMode@0x43FFBC：
    //  · 再次点击已选中的模式按钮（AllowAllUp 取消勾选）→ 先清空全部四个列表
    //    （0x4404CC dl=1），再把模式置 -1；
    //  · 切换到另一模式 → 只清上一模式的列表（0x4404CC dl=0），其它模式列表保留。
    if (m == m_state.sel.Mode()) {
        m_state.sel.ClearAll();
        return;
    }
    m_state.sel.ClearMode(m_state.sel.Mode());
    m_state.sel.SetMode(m);
}

uint64_t BesDoc::FrameLenBits(int colsPerRow) const
{
    if (m_state.frame.frameLen > 0)
        return m_state.frame.frameLen;
    if (colsPerRow < 1) colsPerRow = 1;
    return static_cast<uint64_t>(colsPerRow) *
           static_cast<uint64_t>(bes::Renderer::BitsPerCell(m_state.params.view));
}

void BesDoc::AddView(BesView* v)
{
    if (v && !m_views.contains(v))
        m_views.push_back(v);
}

void BesDoc::RemoveView(BesView* v)
{
    m_views.removeAll(v);
}

void BesDoc::UpdateAllViews()
{
    for (BesView* v : m_views) {
        if (v) {
            v->update();
            v->viewport()->update();
        }
    }
    emit Changed();
}
