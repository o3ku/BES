// DlgParamSet.h —— 参数设置（数据格式 / 显示参数 / 颜色设置 / 构件选项）
#pragma once

#include <QDialog>
#include <QString>

#include <vector>

#include "core/DataTypes.h"
#include "core/FrameModel.h"
#include "core/Renderer.h"
#include "plugin/ComponentHost.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;

// 对应原版 TParamSetForm：按窗口列表逐窗口编辑数据格式，
// 颜色与构件选项为全局设置；「应用」不关闭窗口。
class DlgParamSet : public QDialog {
    Q_OBJECT
public:
    struct Window {
        QString title;
        bes::RenderParams params;
        bes::FrameModel frame;
        bool hasStatusPort = false;   // 是否有对应状态端口
        bool fromPort = false;        // 从状态端口读取格式
    };

    DlgParamSet(QWidget* parent, const std::vector<Window>& windows, int current,
                const bes::ColorScheme& colors, const bes::ComponentHost::Options& opts);

    const std::vector<Window>& Windows() const { return m_windows; }
    int CurrentWindow() const { return m_cur; }
    const bes::ColorScheme& Colors() const { return m_colors; }
    const bes::ComponentHost::Options& Options() const { return m_opts; }

signals:
    void Applied();

private slots:
    void OnWindowChanged(int row);
    void OnApply();
    void OnOk();
    void OnBrowseTemp();

private:
    void BuildUi();
    void LoadWindow(int index);
    void CommitCurrent();
    void RefreshFormatEnabled();
    void MarkDirty();
    void ValidateOptions();
    void UpdateFreeSpace();
    bool TestWritable(const QString& dir);

    void PickColor(int index);
    uint32_t ColorAt(int i) const;
    void SetColorAt(int i, uint32_t c);
    void RefreshColorButtons();
    void DefaultScheme();
    void SchemeOne();

    std::vector<Window> m_windows;
    int m_cur = 0;
    bes::ColorScheme m_colors;
    bes::ComponentHost::Options m_opts;
    bool m_loading = false;
    bool m_dirty = false;

    QTreeWidget* m_windowList = nullptr;
    QRadioButton* m_fromPort = nullptr;
    QRadioButton* m_fromManual = nullptr;
    QGroupBox* m_formatGroup = nullptr;
    QComboBox* m_cboFormat = nullptr;
    QLineEdit* m_edStart = nullptr;
    QLineEdit* m_edFrameLen = nullptr;
    QLineEdit* m_edStep = nullptr;
    QCheckBox* m_chkInvert = nullptr;
    QCheckBox* m_chkHigh = nullptr;

    QLineEdit* m_edXStep = nullptr;
    QLineEdit* m_edYStep = nullptr;
    QLineEdit* m_edXTab = nullptr;
    QLineEdit* m_edYTab = nullptr;
    QCheckBox* m_chkXTab = nullptr;
    QCheckBox* m_chkYTab = nullptr;
    QCheckBox* m_chkTabFollow = nullptr;
    QCheckBox* m_chkRuler = nullptr;

    QPushButton* m_colorBtn[9] = { nullptr };

    QLineEdit* m_edCache = nullptr;
    QLineEdit* m_edBlock = nullptr;
    QLineEdit* m_edTemp = nullptr;
    QLabel* m_freeSpace = nullptr;

    QPushButton* m_btnApply = nullptr;
};
