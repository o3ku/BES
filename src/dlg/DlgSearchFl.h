// DlgSearchFl.h —— 搜索帧长
#pragma once

#include <QDialog>
#include <atomic>

#include "core/FileMap.h"
#include "core/SearchEngine.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QRadioButton;

class DlgSearchFl : public QDialog {
    Q_OBJECT
public:
    // frameLenBits 为当前帧长（“从当前帧长开始”的基准）
    DlgSearchFl(QWidget* parent, const bes::FileMap& map, uint64_t currentBit,
                uint64_t frameLenBits = 0);

    uint64_t FoundFrameLength() const { return m_foundLen; }
    uint64_t SyncColumns() const { return m_syncCols; }

signals:
    void FrameLengthFound(quint64 frameLenBits, quint64 syncColumns);

private slots:
    void OnSearch();

private:
    const bes::FileMap* m_map = nullptr;
    uint64_t m_currentBit = 0;
    uint64_t m_frameLenBits = 0;
    uint64_t m_foundLen = 0;
    uint64_t m_syncCols = 0;
    std::atomic<bool> m_stop{false};
    QLineEdit*  m_edRows = nullptr;
    QLineEdit*  m_edCols = nullptr;
    QLabel*     m_lblResult = nullptr;
    QPushButton* m_btnSearch = nullptr;
    QRadioButton* m_rbHead = nullptr;
    QRadioButton* m_rbCur = nullptr;
    QRadioButton* m_rbNext = nullptr;
};
