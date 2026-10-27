// DlgSearchUnique.h —— 查找独特码
#pragma once

#include <QDialog>
#include <atomic>

#include "core/SearchEngine.h"
#include "core/FileMap.h"

class QRadioButton;
class QLineEdit;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QCheckBox;

class DlgSearchUnique : public QDialog {
    Q_OBJECT
public:
    DlgSearchUnique(QWidget* parent, const bes::FileMap& map);

    // 查找结果（供主窗口跳转）
    const std::vector<bes::SearchHit>& Hits() const { return m_hits; }

private slots:
    void OnCodeChanged();
    void OnFormatChanged();
    void OnFind();

private:
    bool ParseCode(std::vector<uint8_t>& bits, QString& err) const;

    const bes::FileMap* m_map = nullptr;
    std::atomic<bool> m_stop{false};
    QLineEdit*  m_edCode = nullptr;
    QLineEdit*  m_edErr = nullptr;
    QCheckBox*  m_chkAmb = nullptr;
    QLabel*     m_lblProgress = nullptr;
    QPlainTextEdit* m_edHits = nullptr;
    QPushButton* m_btnFind = nullptr;
    QRadioButton* m_rbBin = nullptr;
    QRadioButton* m_rbHex = nullptr;
    QRadioButton* m_rbChar = nullptr;
    bes::UniqueCodeFormat m_format = bes::UniqueCodeFormat::Binary;
    std::vector<bes::SearchHit> m_hits;
};
