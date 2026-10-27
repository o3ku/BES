// BesDoc.h —— 文档（一个数据文件 = 一个 MDI 子窗口）
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "core/FileMap.h"
#include "core/Editor.h"
#include "app/DocState.h"

class BesView;

class BesDoc : public QObject {
    Q_OBJECT
public:
    explicit BesDoc(QObject* parent = nullptr);
    ~BesDoc() override;

    bool OpenFile(const QString& path, QString* err = nullptr);
    void Close();
    bool SaveFile(QString* err = nullptr);

    const bes::FileMap& Map() const { return m_map; }
    bes::FileMap&       Map()       { return m_map; }
    DocState&      State()     { return m_state; }
    const DocState& State() const { return m_state; }

    const QString& FilePath() const { return m_path; }

    uint64_t CursorBit() const { return m_cursorBit; }
    void     SetCursorBit(uint64_t b) { m_cursorBit = b; }

    void SetSelectMode(bes::SelectMode m);

    // ---- 编辑模式（原版工具栏「进行编辑」）----
    // 进入：整份文件换成可增长的内存副本（插入/删除会改变长度，映射视图做不到）
    bool EnterEdit(QString* err = nullptr);
    void LeaveEdit();
    bool InEdit() const { return m_state.editMode; }
    bes::BinaryEditor& Editor() { return m_editor; }
    // 把内存副本按原版方式回写（只读文件 → 「文件不可写！」）
    bool SaveEdits(QString* err = nullptr);

    // 有效帧长：frameLen > 0 时使用显式值；否则按屏宽自动（0 = 自动）
    uint64_t FrameLenBits(int colsPerRow) const;
    bool     IsAutoFrame() const { return m_state.frame.frameLen == 0; }

    // 一个文档可以被多个视图（tab）共享
    void AddView(BesView* v);
    void RemoveView(BesView* v);
    const QVector<BesView*>& Views() const { return m_views; }

    void UpdateAllViews();

signals:
    void Changed();

private:
    bes::FileMap m_map;
    bes::BinaryEditor m_editor;
    DocState     m_state;
    QString      m_path;
    QVector<BesView*> m_views;
    uint64_t     m_cursorBit = 0;
};
