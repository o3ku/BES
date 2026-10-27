// DlgPosition.h —— 数据定位
#pragma once

#include <QDialog>

#include "core/FrameModel.h"
#include "core/Bookmark.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

class DlgPosition : public QDialog {
    Q_OBJECT
public:
    DlgPosition(QWidget* parent, const bes::FrameModel& frame,
                const bes::BookmarkSet& marks, int bitsPerCell = 8,
                bool byteUnit = false);

    uint64_t ResultBit() const { return m_resultBit; }
    const bes::FrameModel& Frame() const { return m_frame; }
    const bes::BookmarkSet& Marks() const { return m_marks; }

signals:
    void Applied(uint64_t bit);

private slots:
    void OnBookmarkSelected(QListWidgetItem* item);
    void OnMethodChanged(int idx);
    void OnApply();

private:
    bool ComputeResult(uint64_t& out) const;

    bes::FrameModel  m_frame;
    bes::BookmarkSet m_marks;
    uint64_t         m_resultBit = 0;
    int              m_bitsPerCell = 8;
    bool             m_byteUnit = false;

    QComboBox*  m_cboMethod = nullptr;
    QLineEdit*  m_edPos = nullptr;
    QLineEdit*  m_edCol = nullptr;
    QListWidget* m_lbMarks = nullptr;
};
