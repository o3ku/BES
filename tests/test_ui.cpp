// test_ui.cpp —— Qt 界面冒烟测试（offscreen）
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLibrary>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <algorithm>
#include <cstdio>
#include <vector>

#include "app/MainWindow.h"
#include "app/Actions.h"
#include "app/GroupPane.h"
#include "app/BesDoc.h"
#include "app/BesView.h"
#include "app/ViewArea.h"
#include "core/Renderer.h"
#include "dlg/DlgParamSet.h"
#include "dlg/DlgFilter.h"
#include "dlg/DlgPosition.h"
#include "dlg/DlgPrint.h"
#include "dlg/DlgSearchUnique.h"
#include "dlg/DlgSearchFl.h"
#include "dlg/DlgVarFrame.h"
#include "dlg/DlgOutputSet.h"
#include "dlg/DlgComponent.h"
#include "dlg/DlgComSelect.h"
#include "dlg/DlgProcess.h"
#include "dlg/DlgProgress.h"
#include "dlg/DlgGroupItems.h"
#include "dlg/DlgCustomize.h"
#include "dlg/DlgAbout.h"

using namespace bes;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); ++g_fail; } \
                              else printf("ok  : %s\n", msg); } while(0)

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    setvbuf(stdout, nullptr, _IONBF, 0);   // 诊断用：崩溃时保留最后一条输出
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("BES_TEST"));
    QCoreApplication::setApplicationName(QStringLiteral("BES_TEST_UI"));

    // 隔离 QSettings，避免读到开发机上的最近文件/插件
    QSettings().clear();

    const QString tmp = QDir::tempPath() + QStringLiteral("/bes_ui_test.dat");
    {
        QFile f(tmp);
        if (f.open(QIODevice::WriteOnly)) {
            std::vector<uint8_t> data(4096);
            for (size_t i = 0; i < data.size(); ++i)
                data[i] = static_cast<uint8_t>((i * 13 + (i >> 2)) & 0xFF);
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<qint64>(data.size()));
            f.close();
        }
    }

    MainWindow w;
    w.resize(1100, 720);
    w.show();
    QApplication::processEvents();

    BesDoc* doc = w.OpenDocumentFile(tmp);
    CHECK(doc != nullptr, "MainWindow opens file");
    BesView* view = w.ActiveView();
    CHECK(view != nullptr, "ActiveView available");
    CHECK(w.ActiveDoc() == doc, "ActiveDoc matches");
    CHECK(view->CellBits() == 1, "default BitChar cell = 1 bit");
    CHECK(view->FrameLenBits() > 0, "auto frame length > 0");

    // 自动帧长：垂直滚动可用
    view->UpdateScrollRange();
    CHECK(view->verticalScrollBar()->maximum() > 0, "auto frame: vertical scroll range > 0");

    // 显式长帧：水平滚动可用
    doc->State().frame.frameLen = 4096ull * 8ull;
    view->UpdateScrollRange();
    CHECK(view->horizontalScrollBar()->maximum() > 0, "long frame: horizontal scroll range > 0");
    doc->State().frame.frameLen = 0;
    view->UpdateScrollRange();

    // 标题：独立开关，默认选中；切换不影响布局
    CHECK(doc->State().params.showTitle, "title shown by default");
    if (QAction* a = w.Cmds()->Find(CMD_VIEW_TITLE)) a->trigger();
    CHECK(!doc->State().params.showTitle, "title toggles off");
    CHECK(doc->State().params.layout == ViewLayout::Paged, "title toggle keeps layout");
    if (QAction* a = w.Cmds()->Find(CMD_VIEW_TITLE)) a->trigger();
    CHECK(doc->State().params.showTitle, "title toggles on");

    // 格式栏 SpinEdit：箭头按显示单位 ±1，文本仍支持 64 位
    {
        auto* edStart = w.findChild<QLineEdit*>(QStringLiteral("edStart"));
        auto* up = w.findChild<QToolButton*>(QStringLiteral("edStartUp"));
        auto* down = w.findChild<QToolButton*>(QStringLiteral("edStartDown"));
        CHECK(edStart && up && down, "format spin controls exist");
        doc->State().params.view = ViewMode::Hex;
        w.UpdateAllViews();
        CHECK(doc->State().params.view == ViewMode::Hex, "spin test in hex unit");
        up->click();
        CHECK(edStart->text() == QStringLiteral("1"), "spin up shows display unit");
        CHECK(doc->State().frame.startBit == 8, "spin up commits 8 bits");
        down->click();
        down->click();
        CHECK(doc->State().frame.startBit == 0 &&
              edStart->text() == QStringLiteral("0"),
              "spin down clamps at zero");
        edStart->setText(QStringLiteral("2305843009213693951"));
        edStart->editingFinished();
        CHECK(doc->State().frame.startBit == 2305843009213693951ull * 8ull,
              "spin field still accepts 64-bit start");
        doc->State().frame.startBit = 0;
        w.UpdateAllViews();
    }

    // 格式栏文字：原版图在上、文字在下，标题不含助记符
    {
        auto* fb = w.findChild<QToolBar*>(QStringLiteral("formatBar"));
        CHECK(fb && fb->toolButtonStyle() == Qt::ToolButtonTextUnderIcon,
              "format bar text under icon");
        QToolButton* hex = fb ? qobject_cast<QToolButton*>(
              fb->widgetForAction(w.Cmds()->Find(CMD_VIEW_MODE_HEX))) : nullptr;
        CHECK(hex && hex->text() == QStringLiteral("十六进制") && !hex->icon().isNull(),
              "format button shows icon and short caption");
        QToolButton* param = fb ? qobject_cast<QToolButton*>(
              fb->widgetForAction(w.Cmds()->Find(CMD_ANALYZE_PARAM))) : nullptr;
        CHECK(param && param->text() == QStringLiteral("参数"),
              "format param button uses original caption");
    }

    // 原版格式栏「显示」下拉框只有 5 项（ViewModeComboBox，无位图）；
    // 位图是 Qt 保留的扩展入口，不占用下拉框项。
    {
        auto* disp = w.findChild<QComboBox*>(QStringLiteral("cboViewMode"));
        CHECK(disp && disp->count() == 5,
              "format combo matches original 5 items (no bitmap item)");
        CHECK(disp && disp->itemText(4) == QStringLiteral("十六进制字符对应"),
              "fifth combo item is hex+ASCII");
        const ViewMode saved = doc->State().params.view;
        w.Cmds()->Find(CMD_VIEW_MODE_PIXEL)->trigger();
        QApplication::processEvents();
        CHECK(doc->State().params.view == ViewMode::Pixel,
              "bitmap toolbar button selects the extended pixel view");
        CHECK(disp && disp->currentIndex() == -1,
              "pixel view is not shown in the original combo");
        w.Cmds()->Find(CMD_VIEW_MODE_HEXCHAR)->trigger();
        QApplication::processEvents();
        CHECK(doc->State().params.view == ViewMode::HexChar &&
              disp && disp->currentIndex() == 4,
              "combo index 4 maps to hex+ASCII view");
        doc->State().params.view = saved;
        w.UpdateAllViews();
    }

    // 倒序与「高位在前(0-7)」是同一选项：按钮状态与参数一致
    {
        QAction* rev = w.Cmds()->Find(CMD_DATA_REVERSE);
        CHECK(rev && rev->isCheckable() && rev->isChecked() == doc->State().params.highBitFirst,
              "reverse button mirrors high-bit-first");
        const bool before = doc->State().params.highBitFirst;
        if (rev) rev->trigger();
        CHECK(doc->State().params.highBitFirst == !before,
              "reverse button toggles high-bit-first");
        if (rev) rev->trigger();
        CHECK(doc->State().params.highBitFirst == before,
              "reverse button toggles back");
    }

    // 单步（工具栏，切换单步/连续）与迭代（分析菜单）是两个命令
    {
        QAction* step = w.Cmds()->Find(CMD_ANALYZE_STEP);
        QAction* iter = w.Cmds()->Find(CMD_ANALYZE_ITERATE);
        auto* mainBar = w.findChild<QToolBar*>(QStringLiteral("mainBar"));
        CHECK(step && iter && step != iter,
              "single-step and iterate are distinct commands");
        CHECK(mainBar && mainBar->actions().contains(step) &&
              !mainBar->actions().contains(iter),
              "main toolbar hosts single-step, not iterate");
        CHECK(step && step->isCheckable() && !step->isChecked(),
              "single-step starts unchecked");
        CHECK(step && step->toolTip().contains(QStringLiteral("数据流输入")),
              "single-step hint mentions data-stream input");
        const bool before = doc->State().singleStep;
        if (step) step->trigger();
        CHECK(doc->State().singleStep == !before, "single-step toggles mode");
        CHECK(step && step->isChecked() == doc->State().singleStep,
              "single-step button mirrors mode");
        if (step) step->trigger();
        CHECK(doc->State().singleStep == before, "single-step toggles back");

        // 原版 DiedaiBtnClick 为空过程：迭代不移动视图
        const uint64_t oldStart = doc->State().frame.startBit;
        const uint64_t oldStep = doc->State().frame.step;
        doc->State().frame.startBit = 8;
        doc->State().frame.step = 4;
        if (iter) iter->trigger();
        CHECK(doc->State().frame.startBit == 8,
              "iterate is an empty placeholder");
        doc->State().frame.startBit = oldStart;
        doc->State().frame.step = oldStep;
        w.UpdateAllViews();
    }

    // 状态栏插入/改写指示（原版最右侧 30px 面板，Insert 切换）
    {
        auto* ins = w.findChild<QLabel*>(QStringLiteral("statusInsert"));
        CHECK(ins && ins->text() == QStringLiteral("插入"), "status insert indicator");
        CHECK(ins && ins->minimumWidth() == 30 && ins->maximumWidth() == 30,
              "insert indicator is 30px wide");
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Insert, Qt::NoModifier);
        QApplication::sendEvent(view, &key);
        CHECK(ins && ins->text() == QStringLiteral("改写"), "Insert shows overwrite");
        QApplication::sendEvent(view, &key);
        CHECK(ins && ins->text() == QStringLiteral("插入"), "Insert toggles back");
    }

    // 左侧行号标尺：分页与横排均保留（横排为数据行）
    {
        const int pagedW = view->YAxisWidth();
        CHECK(pagedW > 0, "left ruler in paged layout");
        if (QAction* a = w.Cmds()->Find(CMD_VIEW_HORIZ)) a->trigger();
        CHECK(doc->State().params.layout == ViewLayout::Horizontal,
              "switch to horizontal layout");
        CHECK(view->YAxisWidth() > 0, "left ruler in horizontal layout");
        CHECK(view->VisibleCols() > 0, "horizontal keeps visible columns");
        // offscreen 平台没有字体，无法断言文字；用左侧标尺底色（深蓝）确认横排仍绘制行号栏
        const QImage himg = view->viewport()->grab().toImage();
        const QColor gutter = himg.pixelColor(1, himg.height() / 2);
        CHECK(gutter == QColor(0, 0, 128), "horizontal view paints left ruler gutter");
        if (QAction* a = w.Cmds()->Find(CMD_VIEW_PAGE)) a->trigger();
        CHECK(doc->State().params.layout == ViewLayout::Paged &&
              view->YAxisWidth() == pagedW, "paged ruler width restored");
    }

    // Hex 两位数（显式切到 Hex 模式）
    {
        doc->State().params.view = ViewMode::Hex;
        Renderer r;
        r.SetData(&doc->Map());
        r.SetFrame(&doc->State().frame);
        r.SetParams(doc->State().params);
        r.SetColsPerRow(view->VisibleCols());
        Cell c; c.bitPos = 0; c.bitLen = 8;
        r.FillCell(c);
        CHECK(c.ch != 0 && c.ch2 != 0, "Hex renders both nibbles");
    }

    // 参数设置：4 页、窗口列表、逐窗口应用、原版钳制与构件选项
    {
        std::vector<DlgParamSet::Window> wins(2);
        wins[0].title = QStringLiteral("win1");
        wins[0].params = doc->State().params;
        wins[0].frame = doc->State().frame;
        wins[1].title = QStringLiteral("win2");
        wins[1].params = doc->State().params;
        wins[1].params.view = ViewMode::Char;
        wins[1].frame = doc->State().frame;
        wins[1].frame.startBit = 64;

        DlgParamSet dlg(&w, wins, 0, doc->State().colors, w.Components().Opts());
        dlg.show(); QApplication::processEvents();

        auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("paramTabs"));
        auto* list = dlg.findChild<QTreeWidget*>(QStringLiteral("windowList"));
        auto* apply = dlg.findChild<QPushButton*>(QStringLiteral("applyButton"));
        auto* start = dlg.findChild<QLineEdit*>(QStringLiteral("startEdit"));
        auto* frameLen = dlg.findChild<QLineEdit*>(QStringLiteral("frameLenEdit"));
        auto* xTab = dlg.findChild<QLineEdit*>(QStringLiteral("xTabEdit"));
        auto* cache = dlg.findChild<QLineEdit*>(QStringLiteral("cacheEdit"));
        auto* block = dlg.findChild<QLineEdit*>(QStringLiteral("blockEdit"));
        auto* mode = dlg.findChild<QComboBox*>(QStringLiteral("modeCombo"));
        auto* fromPort = dlg.findChild<QRadioButton*>(QStringLiteral("fromPortRadio"));
        auto* fromManual = dlg.findChild<QRadioButton*>(QStringLiteral("fromManualRadio"));
        auto* formatGroup = dlg.findChild<QGroupBox*>(QStringLiteral("formatGroup"));
        auto* highBit = dlg.findChild<QCheckBox*>(QStringLiteral("highBitCheck"));

        CHECK(tabs && tabs->count() == 4, "param dialog has 4 pages");
        // 原版 FormCreate：状态基为 nil 时隐藏「构件选项」页（独立启动即如此）
        CHECK(tabs && !tabs->isTabVisible(3), "component-options page hidden without status base");
        CHECK(list && list->topLevelItemCount() == 2, "param window list has 2 windows");
        CHECK(apply && start && frameLen && xTab && cache && block && mode,
              "param controls exist");
        CHECK(apply && !apply->isEnabled(), "apply starts disabled");
        CHECK(fromPort && !fromPort->isEnabled() && fromManual->isChecked() &&
              formatGroup->isEnabled(),
              "no status port: from-port disabled, manual settings enabled");
        CHECK(mode && mode->count() == 5, "format combo has original 5 modes");
        CHECK(highBit && highBit->isChecked(), "high-bit-first checked from params");

        int applied = 0;
        QObject::connect(&dlg, &DlgParamSet::Applied, [&applied] { ++applied; });

        if (list) list->setCurrentItem(list->topLevelItem(1));
        CHECK(start->text() == QStringLiteral("64"), "window switch loads its start");
        CHECK(mode->currentIndex() == 3, "window switch loads its view mode");

        start->setText(QStringLiteral("128"));
        CHECK(apply->isEnabled(), "edit enables apply");
        apply->click();
        CHECK(applied == 1 && !apply->isEnabled(), "apply emits and resets");
        CHECK(dlg.Windows()[1].frame.startBit == 128, "apply commits selected window");

        frameLen->setText(QStringLiteral("999999999"));
        xTab->setText(QStringLiteral("1"));
        apply->click();
        CHECK(dlg.Windows()[1].frame.frameLen == 90000000ull,
              "frame length clamps to 90000000");
        CHECK(frameLen->text() == QStringLiteral("90000000"),
              "clamped frame length written back");
        CHECK(dlg.Windows()[1].params.xTab == 2, "x tab clamps to >= 2");

        cache->setText(QStringLiteral("0"));
        block->setText(QStringLiteral("0"));
        apply->click();
        CHECK(dlg.Options().cacheMB == 1, "cache 0 -> 1MB");
        CHECK(dlg.Options().blockKB == 32, "block 0 -> 32KB");

        highBit->setChecked(false);
        CHECK(apply->isEnabled(), "unchecking high-bit-first enables apply");
        apply->click();
        CHECK(!dlg.Windows()[1].params.highBitFirst,
              "unchecked \"高位在前\" commits low-bit-first");
        dlg.close();
    }
    // 帧过滤：条件列表（帧长 / 数值低字节在前 / CRC）与“与”组合
    {
        const QString filterPath = QDir::tempPath() + QStringLiteral("/bes_filter_ui.dat");
        {
            QFile f(filterPath);
            CHECK(f.open(QIODevice::WriteOnly), "filter ui sample created");
            const std::vector<uint8_t> data = {
                0x12, 0x34, 0x0E, 0xC9,   // CRC 通过
                0x12, 0x34, 0x00, 0x01,   // CRC 失败
                0x56, 0x78, 0x46, 0x89,   // CRC 通过
                0x56, 0x78, 0x00, 0x03    // CRC 失败
            };
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<qint64>(data.size()));
        }
        FileMap fmap;
        CHECK(fmap.Open(filterPath.toStdWString()), "filter ui map opens");
        FrameModel ffm;
        ffm.startBit = 0;
        ffm.frameLen = 32;

        {
            DlgFilter dlg(&w, fmap, ffm);
            auto* list = dlg.findChild<QListWidget*>(QStringLiteral("filterList"));
            auto* add = dlg.findChild<QPushButton*>(QStringLiteral("addButton"));
            auto* frameEdit = dlg.findChild<QLineEdit*>(QStringLiteral("frameEdit"));
            auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
            auto* apply = dlg.findChild<QPushButton*>(QStringLiteral("applyButton"));
            auto* status = dlg.findChild<QLabel*>(QStringLiteral("filterStatus"));
            CHECK(list && add && frameEdit && ok && apply, "filter controls exist");
            int appliedCount = 0;
            QObject::connect(&dlg, &DlgFilter::FilterApplied,
                             [&appliedCount]() { ++appliedCount; });
            if (frameEdit) frameEdit->setText(QStringLiteral("4"));
            if (add) add->click();
            CHECK(list && list->count() == 1 &&
                  list->item(0)->text() == QStringLiteral("帧长 == 32比特"),
                  "filter frame description");
            if (apply) apply->click();
            CHECK(appliedCount == 1 && dlg.Matched().size() == 4,
                  "filter apply runs without closing");
            CHECK(status && status->text() == QStringLiteral("已完成：100%"),
                  "filter apply reports progress");
            if (ok) ok->click();
            CHECK(appliedCount == 1, "filter ok reuses applied result");
            CHECK(dlg.result() == QDialog::Accepted, "filter ok accepts");
            CHECK(dlg.Filter().conditions.size() == 1 &&
                  dlg.Filter().conditions[0].kind == FilterKind::FrameLength &&
                  dlg.Filter().conditions[0].frameBits == 32,
                  "filter frame condition stored");
            CHECK(dlg.Matched().size() == 4, "filter frame length matches 4 frames");
        }
        {
            DlgFilter dlg(&w, fmap, ffm);
            auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("filterTabs"));
            auto* pos = dlg.findChild<QLineEdit*>(QStringLiteral("dataPositionEdit"));
            auto* type = dlg.findChild<QComboBox*>(QStringLiteral("dataTypeCombo"));
            auto* judge = dlg.findChild<QComboBox*>(QStringLiteral("dataJudgeCombo"));
            auto* edit = dlg.findChild<QLineEdit*>(QStringLiteral("dataEdit"));
            auto* add = dlg.findChild<QPushButton*>(QStringLiteral("addButton"));
            auto* list = dlg.findChild<QListWidget*>(QStringLiteral("filterList"));
            auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
            if (type) type->setCurrentIndex(0);
            CHECK(judge && judge->count() == 1 && judge->itemText(0) == QStringLiteral("等于"),
                  "bit type restricts judge to equal");
            if (tabs) tabs->setCurrentIndex(1);
            if (type) type->setCurrentIndex(2);
            if (pos) pos->setText(QStringLiteral("0"));
            if (edit) edit->setText(QStringLiteral("12 34"));
            if (add) add->click();
            CHECK(list && list->count() == 1 &&
                  list->item(0)->text() == QStringLiteral("0开始的双字节 == 0X3412"),
                  "filter value description");
            if (ok) ok->click();
            CHECK(dlg.Filter().conditions.size() == 1 &&
                  dlg.Filter().conditions[0].valueBits == 16 &&
                  dlg.Filter().conditions[0].value == 0x3412ull,
                  "filter value low-byte-first");
            CHECK(dlg.Matched().size() == 2 && dlg.Matched()[0] == 0 &&
                  dlg.Matched()[1] == 1, "filter value matches frames 0,1");
        }
        {
            DlgFilter dlg(&w, fmap, ffm);
            auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("filterTabs"));
            auto* add = dlg.findChild<QPushButton*>(QStringLiteral("addButton"));
            auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
            auto* list = dlg.findChild<QListWidget*>(QStringLiteral("filterList"));
            if (tabs) tabs->setCurrentIndex(2);
            if (add) add->click();
            CHECK(list && list->count() == 1 &&
                  list->item(0)->text() == QStringLiteral("CRC校验通过"),
                  "filter crc description");
            if (ok) ok->click();
            CHECK(dlg.Filter().conditions.size() == 1 &&
                  dlg.Filter().conditions[0].kind == FilterKind::Crc &&
                  dlg.Filter().conditions[0].crcPass, "filter crc pass stored");
            CHECK(dlg.Matched().size() == 2 && dlg.Matched()[0] == 0 &&
                  dlg.Matched()[1] == 2, "filter crc pass matches frames 0,2");
        }
        {
            DlgFilter dlg(&w, fmap, ffm);
            auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("filterTabs"));
            auto* crcError = dlg.findChild<QRadioButton*>(QStringLiteral("crcErrorRadio"));
            auto* add = dlg.findChild<QPushButton*>(QStringLiteral("addButton"));
            auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
            auto* list = dlg.findChild<QListWidget*>(QStringLiteral("filterList"));
            if (tabs) tabs->setCurrentIndex(2);
            if (crcError) crcError->setChecked(true);
            if (add) add->click();
            CHECK(list && list->count() == 1 &&
                  list->item(0)->text() == QStringLiteral("CRC校验失败"),
                  "filter crc fail description");
            if (ok) ok->click();
            CHECK(dlg.Matched().size() == 2 && dlg.Matched()[0] == 1 &&
                  dlg.Matched()[1] == 3, "filter crc fail matches frames 1,3");
        }
        {
            // 帧长 + 数值 + CRC 三个条件相“与”
            DlgFilter dlg(&w, fmap, ffm);
            auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("filterTabs"));
            auto* add = dlg.findChild<QPushButton*>(QStringLiteral("addButton"));
            auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
            auto* list = dlg.findChild<QListWidget*>(QStringLiteral("filterList"));
            auto* frameEdit = dlg.findChild<QLineEdit*>(QStringLiteral("frameEdit"));
            if (frameEdit) frameEdit->setText(QStringLiteral("4"));
            if (add) add->click();
            if (tabs) tabs->setCurrentIndex(1);
            if (auto* type = dlg.findChild<QComboBox*>(QStringLiteral("dataTypeCombo")))
                type->setCurrentIndex(2);
            if (auto* edit = dlg.findChild<QLineEdit*>(QStringLiteral("dataEdit")))
                edit->setText(QStringLiteral("12 34"));
            if (add) add->click();
            if (tabs) tabs->setCurrentIndex(2);
            if (add) add->click();
            CHECK(list && list->count() == 3, "filter list holds 3 conditions");
            if (ok) ok->click();
            CHECK(dlg.Filter().conditions.size() == 3, "filter AND conditions stored");
            CHECK(dlg.Matched().size() == 1 && dlg.Matched()[0] == 0,
                  "filter AND matches frame 0");
        }
        fmap.Close();
        QFile::remove(filterPath);
    }
    {
        FrameModel fm = doc->State().frame;
        fm.frameLen = view->FrameLenBits();
        DlgPosition dlg(&w, fm, doc->State().marks);
        dlg.show(); QApplication::processEvents(); dlg.close();
        CHECK(true, "DlgPosition constructs");
    }
    {
        DlgPrint dlg(&w);
        CHECK(dlg.Left() == 10 && dlg.Right() == 10 &&
              dlg.Top() == 10 && dlg.Bottom() == 10,
              "print default margins = 10 mm");
        CHECK(dlg.First() == 1 && dlg.Last() == 0, "print default range = all");
        auto* lr = dlg.findChild<QLineEdit*>(QStringLiteral("marginLR"));
        auto* tb = dlg.findChild<QLineEdit*>(QStringLiteral("marginTB"));
        auto* range = dlg.findChild<QLineEdit*>(QStringLiteral("pageRange"));
        CHECK(lr && tb && range, "print margin/range controls exist");
        if (lr) lr->setText(QStringLiteral("5"));
        if (tb) tb->setText(QStringLiteral("6"));
        if (range) range->setText(QStringLiteral("2-3"));
        auto* printBtn = dlg.findChild<QPushButton*>(QStringLiteral("printButton"));
        CHECK(printBtn != nullptr, "print button exists");
        if (printBtn) printBtn->click();
        CHECK(dlg.Left() == 5 && dlg.Right() == 5 &&
              dlg.Top() == 6 && dlg.Bottom() == 6,
              "print margins apply to both sides");
        CHECK(dlg.First() == 2 && dlg.Last() == 3, "print page range parsed");
    }
    {
        DlgPrint dlg(&w);
        auto* range = dlg.findChild<QLineEdit*>(QStringLiteral("pageRange"));
        if (range) range->setText(QStringLiteral("4"));
        auto* printBtn = dlg.findChild<QPushButton*>(QStringLiteral("printButton"));
        if (printBtn) printBtn->click();
        CHECK(dlg.First() == 4 && dlg.Last() == 4, "print single page range");
    }
    {
        DlgSearchUnique dlg(&w, doc->Map());
        dlg.show(); QApplication::processEvents(); dlg.close();
        CHECK(true, "DlgSearchUnique constructs");
    }
    {
        DlgSearchFl dlg(&w, doc->Map(), 0, view->FrameLenBits());
        dlg.show(); QApplication::processEvents();
        auto* rows = dlg.findChild<QLineEdit*>(QStringLiteral("rowCountEdit"));
        auto* cols = dlg.findChild<QLineEdit*>(QStringLiteral("colCountEdit"));
        CHECK(rows && rows->text() == QStringLiteral("30") &&
              cols && cols->text() == QStringLiteral("2"),
              "search fl uses row/col defaults");
        dlg.close();
        CHECK(true, "DlgSearchFl constructs");
    }
    // 帧长查找：按检测行数统计全0/1列
    {
        const QString syncPath = QDir::tempPath() + QStringLiteral("/bes_sync_ui.dat");
        {
            QFile f(syncPath);
            CHECK(f.open(QIODevice::WriteOnly), "sync ui sample created");
            std::vector<uint8_t> data(512);
            uint32_t rnd = 0x12345678;
            for (size_t i = 0; i < data.size(); ++i) {
                rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;
                data[i] = static_cast<uint8_t>(rnd & 0xFF);
            }
            for (size_t i = 0; i + 8 <= data.size(); i += 8)
                data[i] = 0xEB;
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<qint64>(data.size()));
        }
        FileMap smap;
        CHECK(smap.Open(syncPath.toStdWString()), "sync ui map opens");
        DlgSearchFl dlg(&w, smap, 0, 64);
        quint64 gotLen = 0;
        QObject::connect(&dlg, &DlgSearchFl::FrameLengthFound,
                         [&gotLen](quint64 len, quint64) { gotLen = len; });
        if (auto* rows = dlg.findChild<QLineEdit*>(QStringLiteral("rowCountEdit")))
            rows->setText(QStringLiteral("16"));
        if (auto* cols = dlg.findChild<QLineEdit*>(QStringLiteral("colCountEdit")))
            cols->setText(QStringLiteral("4"));
        if (auto* btn = dlg.findChild<QPushButton*>(QStringLiteral("searchButton")))
            btn->click();
        CHECK(dlg.FoundFrameLength() == 64, "search fl finds 64-bit frame");
        CHECK(gotLen == 64, "search fl emits found length");
        CHECK(dlg.SyncColumns() >= 8, "search fl reports sync columns");
        QFile::remove(syncPath);
    }
    {
        DlgVarFrame dlg(&w, doc->Map(), doc->State().frame);
        dlg.show(); QApplication::processEvents(); dlg.close();
        CHECK(true, "DlgVarFrame constructs");
    }
    // 不等长帧处理：前导码检测 + .bnt 输出
    {
        const QString varPath = QDir::tempPath() + QStringLiteral("/bes_varframe_test.dat");
        const QString bntPath = QDir::tempPath() + QStringLiteral("/bes_varframe_test.bnt");
        {
            QFile f(varPath);
            CHECK(f.open(QIODevice::WriteOnly), "varframe sample created");
            std::vector<uint8_t> data(64, 0xAA);
            for (int frame = 0; frame < 4; ++frame) {
                data[static_cast<size_t>(frame) * 16] = 0xEB;
                data[static_cast<size_t>(frame) * 16 + 1] = 0x90;
            }
            f.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<qint64>(data.size()));
        }
        FileMap vmap;
        CHECK(vmap.Open(varPath.toStdWString()), "varframe sample opens");
        DlgVarFrame dlg(&w, vmap, doc->State().frame, true);
        dlg.show();
        QApplication::processEvents();
        auto* precode = dlg.findChild<QLineEdit*>(QStringLiteral("precodeEdit"));
        auto* outFile = dlg.findChild<QLineEdit*>(QStringLiteral("outputEdit"));
        auto* start   = dlg.findChild<QPushButton*>(QStringLiteral("startButton"));
        CHECK(precode && outFile && start, "varframe processing controls");
        CHECK(dlg.findChild<QComboBox*>(QStringLiteral("formatCombo")) != nullptr &&
              dlg.findChild<QCheckBox*>(QStringLiteral("ambiguityCheck")) != nullptr,
              "varframe format/ambiguity options");
        if (precode && outFile && start) {
            precode->setText(QStringLiteral("EB90"));
            outFile->setText(bntPath);
            QApplication::processEvents();
            CHECK(start->isEnabled(), "varframe start enabled");
            start->click();
            QApplication::processEvents();
            CHECK(dlg.Frames().Size() == 4, "varframe preamble found 4 frames");
            QFile bnt(bntPath);
            CHECK(bnt.open(QIODevice::ReadOnly), "varframe output created");
            const QString text = QString::fromUtf8(bnt.readAll());
            CHECK(text.contains(QStringLiteral("BES-BNT 1.0")) &&
                  text.contains(QStringLiteral("FrameCount=4")),
                  "varframe output content");
        }

        // 读回 .bnt 帧表
        {
            DlgVarFrame d2(&w, vmap, doc->State().frame, true);
            CHECK(d2.findChild<QPushButton*>(QStringLiteral("loadFramesButton")) != nullptr,
                  "bnt import button exists");
            QString impErr;
            CHECK(d2.LoadFrameFile(bntPath, &impErr), "bnt frame table loads");
            CHECK(d2.Frames().Size() == 4, "bnt frame table has 4 frames");
            CHECK(!d2.Frames().Frames().empty() &&
                  d2.Frames().Frames().front().startBit == 0,
                  "bnt first frame starts at 0");
            auto* pre2 = d2.findChild<QLineEdit*>(QStringLiteral("precodeEdit"));
            CHECK(pre2 && pre2->text() == QStringLiteral("EB90"),
                  "bnt restores precode");

            const QString junk = QDir::tempPath() + QStringLiteral("/bes_bad.bnt");
            {
                QFile f(junk);
                if (f.open(QIODevice::WriteOnly)) f.write("not a bnt file\n", 15);
            }
            QString badErr;
            CHECK(!d2.LoadFrameFile(junk, &badErr) && !badErr.isEmpty(),
                  "invalid bnt file rejected");
            QFile::remove(junk);
        }

        dlg.close();
        QFile::remove(varPath);
        QFile::remove(bntPath);
    }
    {
        DlgOutputSet dlg(&w);
        dlg.SetBitsPerCell(8);
        dlg.SetPortChoices({ QStringLiteral("A / 端口7"), QStringLiteral("B / 端口9") },
                           QVector<int>{ 7, 9 });
        auto* tree = dlg.findChild<QTreeWidget*>(QStringLiteral("portsTree"));
        CHECK(tree && tree->topLevelItemCount() == 2, "output ports tree filled");
        auto* portRadio = dlg.findChild<QRadioButton*>(QStringLiteral("portRadio"));
        CHECK(portRadio != nullptr, "output port radio exists");
        auto* rb7 = dlg.findChild<QRadioButton*>(QStringLiteral("portSelect7"));
        auto* rb8 = dlg.findChild<QRadioButton*>(QStringLiteral("portSelect8"));
        auto* rb9 = dlg.findChild<QRadioButton*>(QStringLiteral("portSelect9"));
        CHECK(rb7 && rb8 && rb9, "32 port radios exist");
        if (portRadio) portRadio->click();
        CHECK(rb7 && rb7->isEnabled() && rb9 && rb9->isEnabled(),
              "component ports enabled in port mode");
        CHECK(rb8 && !rb8->isEnabled(), "unregistered port disabled");
        if (tree && tree->topLevelItemCount() > 1)
            tree->setCurrentItem(tree->topLevelItem(1));
        CHECK(portRadio && portRadio->isChecked() && rb9 && rb9->isChecked(),
              "tree selection syncs port radio");
        for (QPushButton* b : dlg.findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("确定"))) { b->click(); break; }
        }
        CHECK(dlg.Options().target == OutputTarget::Port &&
              dlg.Options().portIndex == 9,
              "output port uses component port index");
        dlg.show(); QApplication::processEvents(); dlg.close();
    }
    {
        DlgGroupItems dlg(&w, QStringList{ QStringLiteral("a"), QStringLiteral("b") },
                          QStringLiteral("分组1"));
        dlg.show(); QApplication::processEvents(); dlg.close();
        CHECK(true, "DlgGroupItems constructs");
    }
    {
        DlgAbout about(&w);
        CHECK(about.windowTitle() == QStringLiteral("关于BES"), "about title");
        auto* product = about.findChild<QLabel*>(QStringLiteral("aboutProductName"));
        CHECK(product && product->text() == QStringLiteral("浏览/编辑/选取"),
              "about product name");
        auto* ok = about.findChild<QPushButton*>(QStringLiteral("aboutOkButton"));
        CHECK(ok && ok->text() == QStringLiteral("确定"), "about OK button");
    }

    // 构件数据流处理（原版 TProcessForm 的行集、按钮状态机与校验）
    {
        SelectionSet sel;
        DlgProcess dlg(&w, QString(), sel);
        auto* run   = dlg.findChild<QPushButton*>(QStringLiteral("runButton"));
        auto* pause = dlg.findChild<QPushButton*>(QStringLiteral("pauseButton"));
        auto* stop  = dlg.findChild<QPushButton*>(QStringLiteral("stopButton"));
        auto* reset = dlg.findChild<QPushButton*>(QStringLiteral("resetButton"));
        auto* quit  = dlg.findChild<QPushButton*>(QStringLiteral("quitButton"));
        auto* bar   = dlg.findChild<QProgressBar*>(QStringLiteral("progressBar"));
        auto* pri   = dlg.findChild<QComboBox*>(QStringLiteral("priorityCombo"));
        auto* mode  = dlg.findChild<QComboBox*>(QStringLiteral("modeCombo"));
        auto* deSel = dlg.findChild<QCheckBox*>(QStringLiteral("deselectCheck"));
        auto* param = dlg.findChild<QLineEdit*>(QStringLiteral("paramEdit"));
        auto* dtype = dlg.findChild<QComboBox*>(QStringLiteral("dataTypeCombo"));
        auto* oFlag = dlg.findChild<QComboBox*>(QStringLiteral("outFlagCombo"));
        auto* oFile = dlg.findChild<QLineEdit*>(QStringLiteral("outFileEdit"));

        // 空闲态（0x415650）：仅「开始」「退出」可用，进度条隐藏
        CHECK(run && run->isEnabled() && pause && !pause->isEnabled() &&
              stop && !stop->isEnabled() && reset && !reset->isEnabled() &&
              quit && quit->isEnabled(),
              "数据流处理：空闲态仅「开始/退出」可用");
        CHECK(bar && bar->minimum() == 0 && bar->maximum() == 256 && bar->isHidden(),
              "进度条 0..256 且空闲时隐藏");
        CHECK(pri && pri->count() == 5 && pri->currentIndex() == 3 &&
              dlg.priorityValue() == 0, "优先级默认「正常」= 0（0x5D7600 表）");
        CHECK(dlg.autoQuit() && dlg.autoOpen(), "完成后退出/自动打开输出文件默认勾选");

        // 输入选择 → 反选/选取参数的启用规则（SelectModeRowChange 0x4163B0）
        CHECK(mode && mode->count() == 5 && dlg.inputMode() == DlgProcess::kAll &&
              param->isEnabled() &&
              param->text() == QStringLiteral("全部数据"),
              "全部数据：选取参数可编辑且填入「全部数据」");
        CHECK(deSel && deSel->isEnabled(), "全部数据：反选可用");
        mode->setCurrentIndex(int(DlgProcess::kRow));
        CHECK(deSel && !deSel->isEnabled() && !deSel->isChecked(), "行选取：反选禁用并清空");
        CHECK(!param->isEnabled() && param->text() == QStringLiteral("行选取"),
              "行选取：选取参数显示段名且不可手工输入");
        mode->setCurrentIndex(int(DlgProcess::kBlock));
        CHECK(deSel && deSel->isEnabled(), "块选取：反选可用");
        dlg.setDeselectForCol(true);
        mode->setCurrentIndex(int(DlgProcess::kCol));
        CHECK(deSel && deSel->isEnabled(), "构件允许列选取反选时可用");
        dlg.setManualInputAllowed(true);
        CHECK(param->isEnabled(), "构件接受手工输入时选取参数可编辑");

        // 数据类型：DFM 16 项，末项值 255
        CHECK(dtype && dtype->count() == 16 && dlg.dataTypeValue() == 0 &&
              dtype->itemText(0) == QStringLiteral("比特(7-0)"),
              "数据类型 16 项、默认「比特(7-0)」");
        dtype->setCurrentIndex(15);
        CHECK(dlg.dataTypeValue() == 255 &&
              dlg.dataTypeName() == QStringLiteral("未知数据类型"),
              "未知数据类型 = 255");
        CHECK(oFlag && oFlag->count() == 2 && dlg.outFlagEoi() && oFlag->isEnabled(),
              "输出标志 EOI/EOF，无输出文件时可选");
        CHECK(oFile && oFile->text() == QStringLiteral("不输出"), "无输出端口时「不输出」");

        // 校验：mode≠0 且选取参数为空 → 选取参数不能为空！（0x5D7670）
        param->clear();
        QString verr;
        CHECK(!dlg.validate(&verr) && verr == QStringLiteral("选取参数不能为空！"),
              "选取参数为空时拒绝开始");

        // 运行状态机
        mode->setCurrentIndex(int(DlgProcess::kAll));
        CHECK(dlg.startRun(), "开始：校验通过并进入运行态");
        CHECK(!run->isEnabled() && pause->isEnabled() && stop->isEnabled() &&
              reset->isEnabled() && !bar->isHidden(),
              "运行态：禁「开始」、启「暂停/停止/复位」（0x416198，[form+0x344]=1）");
        pause->click();
        CHECK(dlg.paused(), "暂停按钮按下 → 暂停（命令 3）");
        pause->click();
        CHECK(!dlg.paused(), "暂停按钮弹起 → 继续（命令 4）");
        dlg.setProgress(50, 100);
        CHECK(bar->value() == 128, "进度按 0..256 映射");
        dlg.setProgress(100, 100);
        CHECK(bar->value() == 256, "进度封顶 256");
        stop->click();
        CHECK(!dlg.running() && !pause->isEnabled() && !stop->isEnabled() &&
              !dlg.paused(), "停止：清运行标志并禁用暂停/停止");
        dlg.setResetSupported(false);
        CHECK(dlg.startRun() && !reset->isEnabled(),
              "构件不支持复位时运行态禁用「复位」（[form+0x344]=0）");
        stop->click();
        dlg.setResetSupported(true);
        CHECK(dlg.startRun() && reset->isEnabled(), "构件支持复位时运行态「复位」可用");
        reset->click();
        CHECK(!dlg.running() && run->isEnabled() && bar->isHidden(),
              "复位回到空闲（窗口保持打开）");
        quit->click();
        CHECK(dlg.exitRequested() && !run->isEnabled() && !pause->isEnabled() &&
              !stop->isEnabled() && !reset->isEnabled(),
              "退出：四个按钮全禁并请求退出（0x4162AC）");
        auto* cParam = dlg.findChild<QPushButton*>(QStringLiteral("customParamButton"));
        auto* cView  = dlg.findChild<QPushButton*>(QStringLiteral("customViewButton"));
        CHECK(cParam && cView && !cParam->isEnabled() && !cView->isEnabled(),
              "未绑定构件时自定义两行不可用");

        DlgProcess bound(&w, QStringLiteral("TEST PLUGIN 1"), sel);
        auto* bParam = bound.findChild<QPushButton*>(QStringLiteral("customParamButton"));
        auto* bFlag  = bound.findChild<QComboBox*>(QStringLiteral("outFlagCombo"));
        auto* bFile  = bound.findChild<QLineEdit*>(QStringLiteral("outFileEdit"));
        CHECK(bParam && bParam->isEnabled() &&
              bParam->text() == QStringLiteral("点击按钮打开..."),
              "绑定构件后自定义参数可用（DFM Text）");
        bound.setOutputPort(true);
        CHECK(bound.outputFile() == QStringLiteral("temp.dat") &&
              bFile->text() == QStringLiteral("文件: temp.dat") &&
              bFlag && !bFlag->isEnabled(),
              "有输出端口：「文件: temp.dat」且输出标志不可选（0x413AF7）");
        bound.setOutputPort(false);
        CHECK(bound.outputFile().isEmpty() && bFlag && bFlag->isEnabled(),
              "取消输出端口后回到「不输出」");

        // 数据流管线：「开始」经 FlowEngine 把选区字节推入数据流输入端口
        // （原版把主窗体的数据流上下文 [form+0x360] 交给构件，本版本端口由调用方绑定）
        {
            const QString flowPath = QDir::tempPath() + QStringLiteral("/bes_flow_ui_test.dat");
            {
                QFile f(flowPath);
                CHECK(f.open(QIODevice::WriteOnly), "flow ui sample created");
                std::vector<uint8_t> data(256, 0x5A);
                f.write(reinterpret_cast<const char*>(data.data()),
                        static_cast<qint64>(data.size()));
            }
            struct Collect : IFlowPort {
                std::vector<uint8_t> bytes;
                bool Write(const uint8_t* d, size_t n) override
                { bytes.insert(bytes.end(), d, d + n); return true; }
            } port;
            FileMap fmap;
            CHECK(fmap.Open(flowPath.toStdWString()), "flow ui map opens");
            FrameModel fm; fm.frameLen = 32; fm.startBit = 0; fm.step = 1;
            DlgProcess flow(&w, QString(), sel);
            flow.setSource(&fmap, sel, fm, 8);
            flow.bindFlowPort(&port);
            CHECK(flow.flowParams().mode == SelectMode::None,
                  "输入选择「全部数据」映射为 SelectMode::None");
            CHECK(flow.startRun(), "绑定端口后「开始」进入运行态");
            CHECK(port.bytes.size() == 256 &&
                  flow.engine().bytesWritten() == 256 &&
                  flow.engine().state() == FlowState::Finished,
                  "「开始」把选区字节推入数据流输入端口");
            QFile::remove(flowPath);
        }
    }

    // 进度对话框（TProgressForm：302×86、进度 0..256、默认按钮「取消」）
    {
        DlgProgress prog(&w, QStringLiteral("查找独特码"));
        auto* bar  = prog.findChild<QProgressBar*>(QStringLiteral("progressBar"));
        auto* info = prog.findChild<QLabel*>(QStringLiteral("progressInfo"));
        auto* cancel = prog.findChild<QPushButton*>(QStringLiteral("cancelButton"));
        CHECK(prog.width() == 302 && prog.height() == 86, "进度窗客户区 302×86");
        CHECK(bar && bar->minimum() == 0 && bar->maximum() == 256, "进度窗刻度 0..256");
        CHECK(info && info->geometry() == QRect(9, 22, 201, 13), "信息标签位置按 DFM");
        CHECK(cancel && cancel->text() == QStringLiteral("取消") &&
              cancel->isDefault(), "取消按钮 75×23 且为默认按钮");
        prog.SetProgress(50, 100);
        CHECK(bar->value() == 128, "进度映射到 0..256");
        prog.SetText(QStringLiteral("正在处理..."));
        CHECK(info->text() == QStringLiteral("正在处理..."), "SetText 生效");
        cancel->click();
        CHECK(prog.Cancelled() && info->text() == QStringLiteral("正在取消..."),
              "点击取消置标志并提示");
    }

    // 构件 ABI + 构件管理（选中行不得崩溃）
    const QString plugin = QCoreApplication::applicationDirPath() +
                           QStringLiteral("/test_plugin.dll");
    CHECK(QFileInfo::exists(plugin), "test_plugin.dll deployed");
    std::wstring err;
    CHECK(w.Components().RegisterDll(plugin.toStdWString(), &err),
          "test plugin registers (RegComNum/RegComInfo)");
    CHECK(w.Components().Count() == 1, "component count = 1");
    CHECK(w.Components().Components()[0].name == L"TEST PLUGIN 1",
          "RegComInfo buffer B is the display name");
    CHECK(w.Components().Components()[0].regNameAnsi == "TEST.PLUGIN.1",
          "RegComInfo buffer A is the registration name");

    // 实例信息：vtable 索引 18 上报的内联结构
    {
        bes::InstanceInfo info;
        std::wstring iErr;
        CHECK(bes::ComponentHost::QueryInstanceInfo(plugin.toStdWString(),
                                                    "TEST.PLUGIN.1", &info, &iErr) ==
                  bes::ComponentHost::InstanceInfoStatus::Ok,
              "instance info is readable through vtable slot 18");
        CHECK(info.author == L"Test Author" && info.version == L"1.2.3",
              "instance info author/version");
        CHECK(info.desc == L"Qt rewrite test plugin", "instance info description");
        CHECK(info.comType == 2, "instance info comType (2 = 数据源构件)");
        CHECK(info.ports.size() == 2, "instance info port count");
        if (info.ports.size() == 2) {
            CHECK(info.ports[0].index == 1 &&
                  info.ports[0].kind == bes::PortKind::DataIn &&
                  info.ports[0].rawDataType == 1 &&
                  info.ports[0].maxCount == 1 &&
                  info.ports[0].defaultCount == 1,
                  "data-in port fields");
            CHECK(info.ports[1].index == 2 &&
                  info.ports[1].kind == bes::PortKind::DataOut &&
                  info.ports[1].rawDataType == 1 &&
                  info.ports[1].maxCount == 2,
                  "data-out port fields");
        }
    }

    void* inst = nullptr;
    CHECK(w.Components().CreateInstance(L"TEST PLUGIN 1", &inst),
          "CreateInstance 4-arg ABI");
    CHECK(inst != nullptr, "component instance non-null");

    // 构件命令 100：TProcessForm.SetComponent 用 ContainerPanel 客户区矩形
    // 经 vtable 索引 13 同步自定义视图；-100 = 构件不提供自定义视图。
    {
        QLibrary lib(plugin);
        CHECK(lib.load(), "test plugin loaded for command stats");
        using ResetFn = void (__stdcall*)();
        using StatsFn = int (__stdcall*)(int*, int);
        auto reset = reinterpret_cast<ResetFn>(lib.resolve("TestPluginResetStats"));
        auto stats = reinterpret_cast<StatsFn>(lib.resolve("TestPluginStats"));
        CHECK(reset && stats, "test plugin exports command stats");
        if (reset && stats) {
            reset();
            bes::ViewRect rc{5, 7, 405, 307};
            int r = -1;
            CHECK(w.Components().SendCommand(inst, bes::kCmdSetViewRect, &rc,
                                             static_cast<int>(sizeof(rc)), &r) &&
                      r == 0,
                  "command 100 dispatches through vtable slot 13");
            int st[8] = {0};
            CHECK(stats(st, 8) == 8 && st[0] == bes::kCmdSetViewRect && st[1] == 16 &&
                      st[2] == 5 && st[3] == 7 && st[4] == 405 && st[5] == 307,
                  "component receives the 16-byte rect unchanged");
            CHECK(w.Components().SendCommand(inst, 77, nullptr, 0, &r) && r == -100,
                  "unsupported command returns -100 (no custom view)");
            bool hasView = false;
            CHECK(w.Components().SetViewRect(inst, rc, &hasView) && hasView,
                  "SetViewRect reports the component provides a view");
            hasView = true;
            CHECK(!w.Components().SetViewRect(nullptr, rc, &hasView) && !hasView,
                  "SetViewRect without an instance reports no view");
            CHECK(stats(st, 8) == 8 && st[6] == 3,
                  "three command calls recorded on the component");
            w.Components().DestroyInstance(inst);
            inst = nullptr;
            CHECK(stats(st, 8) == 8 && st[7] == 1,
                  "DestroyInstance calls vtable slot 1 (release)");
        } else {
            w.Components().DestroyInstance(inst);
            inst = nullptr;
        }
    }

    // 构件 ShowModal ABI：按 ANSI 路径调用，构件自身创建标记文件
    {
        const QString modalPath = QDir::tempPath() + QStringLiteral("/bes_modal_test.dat");
        {
            QFile f(modalPath);
            if (f.open(QIODevice::WriteOnly)) f.write("x", 1);
        }
        std::wstring mErr;
        CHECK(w.Components().ShowModal(L"TEST PLUGIN 1", modalPath.toStdWString(), &mErr),
              "ShowModal ABI invocation");
        CHECK(QFileInfo::exists(modalPath + QStringLiteral(".modal.log")),
              "ShowModal received file path");
        QFile::remove(modalPath);
        QFile::remove(modalPath + QStringLiteral(".modal.log"));
    }

    {
        DlgComSelect dlg(&w, w.Components());
        auto* label = dlg.findChild<QLabel*>(QStringLiteral("Label1"));
        auto* list  = dlg.findChild<QListWidget*>(QStringLiteral("ListBox1"));
        auto* ok    = dlg.findChild<QPushButton*>(QStringLiteral("OKBtn"));
        CHECK(label && label->text() == QStringLiteral("找到1个构件"),
              "ComSelectForm shows 找到N个构件");
        CHECK(list && list->count() == 1 &&
              list->item(0)->text() == QStringLiteral("TEST PLUGIN 1"),
              "component list shows display names");
        dlg.show();
        QApplication::processEvents();
        CHECK(ok != nullptr, "determine button present");
        if (ok) ok->click();   // 原版 OKBtnClick：CreateInstance + 端口校验后关闭
        CHECK(dlg.result() == QDialog::Accepted, "determine accepts a valid component");
        CHECK(dlg.SelectedName() == L"TEST PLUGIN 1" &&
              dlg.SelectedRegName() == "TEST.PLUGIN.1",
              "selection keeps display name and registration name");
        CHECK(dlg.IsDataSource() && dlg.DataInPortId() == 1 &&
              dlg.HasOutputPort() && dlg.PortCount() == 2 && dlg.ComType() == 2,
              "OKBtnClick stores comType/data-in port/output-port info");
    }

    // 快速执行（原版 FastExecuteBtnClick）：临时宿主 + 构件 ABI 校验
    {
        bes::ComponentHost temp;
        std::wstring fErr;
        CHECK(MainWindow::LoadFastExecuteDll(plugin, &temp, &fErr),
              "fast execute loads the DLL into a temporary host");
        CHECK(temp.Count() == 1, "fast execute enumerates that DLL's components");
        CHECK(temp.Components()[0].name == L"TEST PLUGIN 1",
              "fast execute component display name");

        // 原版 LoadLibrary / GetProcAddress 任一失败都只报「不是局级系统构件！」
        bes::ComponentHost bad;
        std::wstring bErr;
        const QString notCom = QCoreApplication::applicationDirPath() +
                               QStringLiteral("/test_ui.exe");
        CHECK(QFileInfo::exists(notCom), "test_ui.exe deployed");
        CHECK(!MainWindow::LoadFastExecuteDll(notCom, &bad, &bErr),
              "non-component binary is rejected");
        CHECK(bad.Count() == 0, "rejected binary registers nothing");
        CHECK(bErr == L"不是局级系统构件！", "fast execute rejection message");

        QAction* fast = w.Cmds()->Find(CMD_CUSTOM_FAST_EXEC);
        QAction* refresh = w.Cmds()->Find(CMD_VIEW_REFRESH);
        CHECK(fast && refresh && fast != refresh,
              "fast execute is its own command, not 刷新");
        CHECK(fast && fast->text().contains(QStringLiteral("快速执行")),
              "fast execute menu text");
    }
    {
        // 定制构件管理（原版 TComCustomForm）：DFM 布局、选择联动、探测/复制注册
        bes::ComponentHost host2;
        std::wstring rErr;
        CHECK(host2.RegisterDll(plugin.toStdWString(), &rErr),
              "management host registers the plugin");

        DlgComponent dlg(&w, host2);
        dlg.show();
        QApplication::processEvents();

        CHECK(dlg.width() == 468 && dlg.height() == 312,
              "TComCustomForm client is 468x312");
        auto* list   = dlg.findChild<QListWidget*>(QStringLiteral("ComListBox"));
        auto* edName = dlg.findChild<QLineEdit*>(QStringLiteral("ComNameEdit"));
        auto* edFile = dlg.findChild<QLineEdit*>(QStringLiteral("ComFileEdit"));
        auto* memo   = dlg.findChild<QPlainTextEdit*>(QStringLiteral("ComInfoMemo"));
        auto* browse = dlg.findChild<QPushButton*>(QStringLiteral("BrowseBtn"));
        auto* add    = dlg.findChild<QPushButton*>(QStringLiteral("AddBtn"));
        auto* del    = dlg.findChild<QPushButton*>(QStringLiteral("DelBtn"));
        auto* edit   = dlg.findChild<QPushButton*>(QStringLiteral("EditBtn"));
        auto* quit   = dlg.findChild<QPushButton*>(QStringLiteral("QuitBtn"));
        CHECK(list && edName && edFile && memo && browse && add && del && edit && quit,
              "dialog has the DFM's named controls");
        if (list && edName && edFile && memo && add && del && edit && quit) {
            CHECK(list->count() == 1 && list->currentRow() == 0,
                  "FormCreate selects the first component");
            CHECK(edName->maxLength() == 32, "ComNameEdit MaxLength = 32");
            CHECK(!edName->isReadOnly(), "ComNameEdit stays editable");
            CHECK(edFile->isReadOnly(), "ComFileEdit is read-only per DFM");
            CHECK(memo->isReadOnly(), "ComInfoMemo is read-only per DFM");
            CHECK(edName->text() == QLatin1String("TEST PLUGIN 1"),
                  "selection fills 构件名称 with the display name");
            CHECK(edFile->text() == QStringLiteral("test_plugin.dll: TEST.PLUGIN.1"),
                  "构件程序文件 = 文件名 + ': ' + 注册名");
            const QString memoText = memo->toPlainText();
            CHECK(memoText.contains(QStringLiteral("作者: Test Author\n")),
                  "构件信息 author comes from the instance info");
            CHECK(memoText.contains(QStringLiteral("版本: 1.2.3\n")),
                  "构件信息 version comes from the instance info");
            CHECK(memoText.contains(QStringLiteral("构件类型: 数据源构件\n")),
                  "构件信息 uses the 1-based comType into the kind table");
            CHECK(memoText.contains(QStringLiteral("物理端口数量: 2\n\n")),
                  "构件信息 reports the physical port count and a blank line");
            CHECK(memoText.contains(QStringLiteral(
                      "物理端口 ID:1\n"
                      "端口数据类型: 1\n"
                      "端口类型: 数据流输入\n"
                      "缺省端口数量: 1\n"
                      "最大端口数量: 1\n")),
                  "port block: ID has no space, data type is numeric, default before max");
            CHECK(add->isEnabled() && del->isEnabled() && edit->isEnabled(),
                  "selecting a component enables 添加/删除/修改");

            edName->setText(QStringLiteral("TEST.RENAMED"));
            edit->click();
            QApplication::processEvents();
            CHECK(host2.Count() == 1 && host2.Components()[0].name == L"TEST.RENAMED",
                  "修改 writes the edited name back into the record");
            CHECK(list->item(0)->text() == QStringLiteral("TEST.RENAMED"),
                  "修改 refreshes the list caption");

            // 浏览探测：PeekAbi 只枚举不登记（原版 BrowseBtnClick）
            std::vector<bes::ComponentInfo> probed;
            std::wstring pErr;
            CHECK(bes::ComponentHost::PeekAbi(plugin.toStdWString(), &probed, &pErr),
                  "PeekAbi probes the component DLL");
            CHECK(probed.size() == 1 &&
                  probed[0].regNameAnsi == "TEST.PLUGIN.1" &&
                  probed[0].name == L"TEST PLUGIN 1",
                  "PeekAbi enumerates registration/display name pairs without registering");

            std::wstring nErr;
            const std::wstring notCom = QCoreApplication::applicationDirPath().toStdWString()
                                        + L"\\test_ui.exe";
            CHECK(!bes::ComponentHost::PeekAbi(notCom, nullptr, &nErr)
                      && nErr == L"不是局级系统构件！",
                  "PeekAbi rejects a non-component binary with the original text");
            CHECK(!bes::ComponentHost::ComDir().empty(), "ComDir resolves");

            del->click();
            QApplication::processEvents();
            CHECK(host2.Count() == 0, "删除 unregisters the component");
            CHECK(QFileInfo::exists(plugin), "删除 leaves the DLL on disk");
            CHECK(add->isEnabled() && !del->isEnabled() && !edit->isEnabled(),
                  "an empty list disables 删除/修改 but leaves 添加 untouched");

            quit->click();
            CHECK(dlg.result() == QDialog::Rejected, "退出 = ModalResult mrCancel");
        }
        dlg.close();
    }

    // 32 位构件已不再支持：注册应明确失败，而不是崩溃
    {
        const QString bescom = QDir::current().filePath(QStringLiteral("BesCom.dll"));
        if (QFileInfo::exists(bescom)) {
            std::wstring err32;
            CHECK(!w.Components().RegisterDll(bescom.toStdWString(), &err32),
                  "32-bit component rejected");
            CHECK(err32.find(L"32 位") != std::wstring::npos,
                  "32-bit rejection explains 32-bit support is gone");
        } else {
            printf("skip: BesCom.dll not found in working dir\n");
        }
    }

    // 编辑模式（原版工具栏「进行编辑」+ editIface 剪切/复制/粘贴/撤销/重做）
    {
        QByteArray orig;
        {
            QFile rf(tmp);
            if (rf.open(QIODevice::ReadOnly)) orig = rf.readAll();
            rf.close();
        }
        CHECK(orig.size() == 4096, "edit block starts from pristine data");
        auto byteAt = [&doc](uint64_t off) {
            uint8_t b = 0;
            doc->Map().ReadByte(off, b);
            return b;
        };

        QAction* mode  = w.Cmds()->Find(CMD_EDIT_MODE);
        QAction* cut   = w.Cmds()->Find(CMD_EDIT_CUT);
        QAction* copyA = w.Cmds()->Find(CMD_EDIT_COPY);
        QAction* paste = w.Cmds()->Find(CMD_EDIT_PASTE);
        QAction* undoA = w.Cmds()->Find(CMD_EDIT_UNDO);
        QAction* redoA = w.Cmds()->Find(CMD_EDIT_REDO);
        QComboBox* disp = w.findChild<QComboBox*>(QStringLiteral("cboViewMode"));
        CHECK(mode && mode->isCheckable(), "进行编辑 是可按下的工具栏按钮");
        CHECK(mode && mode->text() == QStringLiteral("进行编辑"), "进行编辑 caption");
        CHECK(!doc->InEdit(), "文档默认不在编辑态");
        w.UpdateMenuState();
        CHECK(cut && !cut->isEnabled(), "剪切 needs edit mode");
        CHECK(paste && !paste->isEnabled(), "粘贴 needs edit mode");
        CHECK(undoA && !undoA->isEnabled(), "撤销 needs edit mode");
        CHECK(copyA && copyA->isEnabled(), "复制 outside edit mode");
        CHECK(disp && disp->isEnabled(), "非编辑态显示模式下拉框可用");

        mode->trigger();
        QApplication::processEvents();
        CHECK(doc->InEdit() && doc->Map().IsEditable(), "进入编辑换成可增长内存副本");
        CHECK(mode->isChecked(), "进行编辑按下态跟随文档");
        CHECK(cut->isEnabled() && paste->isEnabled(), "剪切/粘贴 in edit mode");
        CHECK(disp && !disp->isEnabled(), "编辑态禁用显示模式下拉框（原版 ViewModeComboBox）");

        QLabel* ins = w.findChild<QLabel*>(QStringLiteral("statusInsert"));
        doc->State().sel.SetContinuous(0, 31);      // 第 0..3 字节
        if (w.InsertMode()) w.ToggleInsertMode();   // 切到改写
        CHECK(!w.InsertMode(), "Insert 切到改写");
        CHECK(ins && ins->text() == QStringLiteral("改写"), "状态栏显示改写");
        w.DoCopy();
        const QByteArray copied = QApplication::clipboard()->mimeData()->data(
                                      QStringLiteral("application/x-bes-bytes"));
        CHECK(copied.size() == 4 && copied == QByteArray(orig.constData(), 4),
              "复制把选取字节放入剪贴板");

        doc->SetCursorBit(64);                      // 第 8 字节
        w.DoPaste();
        CHECK(doc->Map().SizeBytes() == 4096, "改写粘贴长度不变");
        CHECK(byteAt(8) == static_cast<uint8_t>(orig[0]) &&
              byteAt(11) == static_cast<uint8_t>(orig[3]), "改写粘贴覆盖光标处字节");
        CHECK(doc->Editor().Dirty(), "改动置脏标志");

        w.DoUndo();
        CHECK(byteAt(8) == static_cast<uint8_t>(orig[8]), "撤销回到改前内容");
        CHECK(redoA->isEnabled(), "撤销后可重做");
        CHECK(!undoA->isEnabled(), "撤销栈空时撤销不可用");
        w.DoRedo();
        CHECK(byteAt(8) == static_cast<uint8_t>(orig[0]), "重做重新写入");

        doc->State().sel.SetContinuous(0, 31);
        w.DoCut();
        CHECK(doc->Map().SizeBytes() == 4092, "剪切删除选取字节");
        w.DoUndo();
        CHECK(doc->Map().SizeBytes() == 4096, "撤销剪切恢复长度");

        if (!w.InsertMode()) w.ToggleInsertMode();  // 切到插入
        CHECK(ins && ins->text() == QStringLiteral("插入"), "状态栏显示插入");
        doc->SetCursorBit(64);
        w.DoPaste();
        CHECK(doc->Map().SizeBytes() == 4100, "插入粘贴使文件变长");
        w.DoUndo();
        CHECK(doc->Map().SizeBytes() == 4096, "撤销插入粘贴恢复长度");

        CHECK(!w.LeaveEditMode(doc, 0), "取消时不退出编辑态");
        CHECK(doc->InEdit() && mode->isChecked(), "取消后按钮仍按下");
        CHECK(disp && !disp->isEnabled(), "拒绝退出后显示模式仍禁用");
        CHECK(w.LeaveEditMode(doc, 1), "保存并退出编辑");
        CHECK(!doc->InEdit() && !mode->isChecked(), "退出编辑后按钮弹回");
        CHECK(disp && disp->isEnabled(), "退出编辑恢复显示模式下拉框");
        CHECK(!doc->Editor().Dirty(), "保存清脏标志");
        QByteArray onDisk;
        {
            QFile rf(tmp);
            if (rf.open(QIODevice::ReadOnly)) onDisk = rf.readAll();
            rf.close();
        }
        QByteArray expect = orig;
        expect.replace(8, 4, QByteArray(orig.constData(), 4));
        CHECK(onDisk == expect, "退出编辑时按脏标志回写原文件");

        // 只读文件：原版提示「文件不可写！」
        QFile::setPermissions(tmp, QFile::ReadOwner | QFile::ReadUser);
        CHECK(doc->EnterEdit(), "重新进入编辑");
        const uint8_t zz = 0x5A;
        CHECK(doc->Editor().Overwrite(0, &zz, 1), "只读文件仍可内存改写");
        QString rerr;
        CHECK(!doc->SaveEdits(&rerr), "只读文件保存失败");
        CHECK(rerr == QStringLiteral("文件不可写！"), "只读保存提示文件不可写");
        CHECK(doc->Editor().Dirty(), "保存失败保持脏标志");
        CHECK(doc->Editor().Undo(), "撤销只读文件上的改写");
        QFile::setPermissions(tmp, QFile::ReadOwner | QFile::WriteOwner |
                                   QFile::ReadUser | QFile::WriteUser);

        // 回到初始内容，后续用例不受编辑用例影响
        {
            QFile wf(tmp);
            if (wf.open(QIODevice::WriteOnly)) { wf.write(orig); wf.close(); }
        }
        CHECK(doc->Editor().Discard(), "丢弃改动并重读磁盘内容");
        CHECK(doc->Map().SizeBytes() == 4096 && byteAt(0) == static_cast<uint8_t>(orig[0]),
              "文档回到初始内容");
        doc->LeaveEdit();
        w.UpdateAllViews();
        w.UpdateMenuState();
        CHECK(!doc->InEdit(), "编辑用例结束时退出编辑态");
        CHECK(disp && disp->isEnabled(), "离开编辑态后显示模式可用");
    }

    // 多视图共享文档
    w.DuplicateCurrentTab();
    QApplication::processEvents();
    CHECK(doc->Views().size() == 2, "two views share one document");

    // 联动浏览：同步范围 = 同一容器内的其它视图（排除自己），
    // 且把「本次移动量」加到每个视图自己的位置上（原版 0x407B4C + 0x405C34/0x4061C6）
    {
        const bool wasLink = w.LinkBrowse();
        if (wasLink) w.ToggleLinkBrowse();          // 先把两个视图摆到各自的位置
        const QVector<BesView*>& vs = doc->Views();
        CHECK(vs.size() == 2, "联动用例：两个视图");
        if (vs.size() == 2) {
            BesView* a = vs[0];
            BesView* b = vs[1];
            const int amax = a->verticalScrollBar()->maximum();
            const int bmax = b->verticalScrollBar()->maximum();
            CHECK(amax >= 4 && bmax >= 4, "联动用例：两个视图都有可滚动范围");
            a->verticalScrollBar()->setValue(0);
            b->verticalScrollBar()->setValue(bmax / 2);   // B 待在自己的位置
            const uint64_t brow = b->ScrollRow();

            w.ToggleLinkBrowse();
            CHECK(w.LinkBrowse(), "联动浏览已开启");
            a->verticalScrollBar()->setValue(2);          // A 从 0 移动 2 行
            CHECK(a->ScrollRow() == 2, "联动用例：源视图移动 2 行");
            CHECK(b->ScrollRow() == brow + 2, "联动：其它视图加上同一移动量，而非跳到源视图的绝对行");
            CHECK(b->ScrollRow() != a->ScrollRow(), "联动保持各视图自己的位置基准");

            w.ToggleLinkBrowse();
            CHECK(!w.LinkBrowse(), "联动浏览已关闭");
            const uint64_t brow2 = b->ScrollRow();
            a->verticalScrollBar()->setValue(4);
            CHECK(b->ScrollRow() == brow2, "关闭联动后其它视图不跟随");

            w.ToggleLinkBrowse();                         // 反向：B 滚动同样带动 A
            b->verticalScrollBar()->setValue(static_cast<int>(brow2) - 2);
            CHECK(a->ScrollRow() == 2, "联动是双向的：其它视图按同一移动量位移本视图");

            if (w.LinkBrowse() != wasLink) w.ToggleLinkBrowse();
            a->verticalScrollBar()->setValue(0);
            b->verticalScrollBar()->setValue(0);
            CHECK(w.LinkBrowse() == wasLink, "联动开关恢复初值");
        }
    }

    // 窗口排布：原版 [form+0x498] 是主窗体的全局字段，五个按钮写完后立即遍历
    // 全部停靠站点重排，所以「排布」不是当前文档的绘制方式，而是视图区布局。
    {
        auto* area = qobject_cast<ViewArea*>(w.centralWidget());
        auto* split = area ? area->findChild<QSplitter*>() : nullptr;
        const int nViews = area ? area->count() : 0;
        CHECK(area && split, "视图区 = 标签条 + 分隔窗口");
        CHECK(area && area->arrangement() == bes::ViewLayout::Paged &&
              w.GetViewLayout() == bes::ViewLayout::Paged,
              "默认分页（PageBtnClick@0x408B20 置 [form+0x498]=0）");
        CHECK(w.VisibleViewCount() == 1, "分页：只有一个视图占屏");

        w.SetViewLayout(bes::ViewLayout::Horizontal);
        CHECK(area->arrangement() == bes::ViewLayout::Horizontal &&
              split->orientation() == Qt::Horizontal &&
              w.VisibleViewCount() == nViews,
              "横排：全部视图横向并排（HorizBtnClick@0x408B94 置 1 并重排所有站点）");

        w.SetViewLayout(bes::ViewLayout::Vertical);
        CHECK(area->arrangement() == bes::ViewLayout::Vertical &&
              split->orientation() == Qt::Vertical &&
              w.VisibleViewCount() == nViews,
              "竖排：全部视图纵向堆叠（VertBtnClick@0x408C24 置 2）");

        // 组内为空时分组排布退回「全部视图」，但仍是全局状态
        w.SetViewLayout(bes::ViewLayout::GroupH);
        CHECK(area->arrangement() == bes::ViewLayout::GroupH &&
              split->orientation() == Qt::Horizontal &&
              w.VisibleViewCount() == nViews,
              "分组横排：PageHorizBtnClick@0x408CB4 置 3，按组遍历子站点");
        w.SetViewLayout(bes::ViewLayout::GroupV);
        CHECK(area->arrangement() == bes::ViewLayout::GroupV &&
              split->orientation() == Qt::Vertical,
              "分组竖排：PageVertBtnClick@0x408E2C 置 4");

        // 全局生效：每个视图的文档都拿到同一模式，而不是只有活动文档；
        // 排布只重插停靠站点（0x544BC8），不改 XTAB/YTAB 开关。
        bool layoutSame = true, tabsUntouched = true;
        for (int i = 0; i < area->count(); ++i) {
            auto* v = dynamic_cast<BesView*>(area->widget(i));
            if (!v || !v->Doc()) continue;
            if (v->Doc()->State().params.layout != bes::ViewLayout::GroupV) layoutSame = false;
            if (v->Doc()->State().params.showXTab || v->Doc()->State().params.showYTab)
                tabsUntouched = false;
        }
        CHECK(layoutSame, "分组竖排把模式写进全部视图（遍历所有站点）");
        CHECK(tabsUntouched, "排布不改 XTAB/YTAB 开关（原版只重插停靠站点）");

        w.SetViewLayout(bes::ViewLayout::Paged);
        CHECK(area->arrangement() == bes::ViewLayout::Paged &&
              w.VisibleViewCount() == 1, "回到分页后其余视图不再同屏");
    }

    // 窗口分组拖动（原版停靠/拖动分组的等价交互）：分组树内把窗口拖到目标组，
    // 落到「数据窗口」= 移出所有自定义分组；一个窗口只属于一个自定义组。
    {
        auto* gp = w.findChild<GroupPane*>(QStringLiteral("groupPane"));
        auto* area = qobject_cast<ViewArea*>(w.centralWidget());
        CHECK(gp && gp->dragDropMode() == QAbstractItemView::DragDrop,
              "分组树接受拖动");
        CHECK(area && area->count() >= 2, "分组拖动：至少两个窗口");
        if (gp && area && area->count() >= 2) {
            const int idx0 = 0;
            const int idx1 = 1;

            QMimeData* md = GroupPane::CreateTabMimeData(idx1);
            int decoded = -1;
            CHECK(md && GroupPane::DecodeTabMimeData(md, &decoded) && decoded == idx1,
                  "分组拖动 MIME 往返");
            delete md;
            QMimeData empty;
            CHECK(!GroupPane::DecodeTabMimeData(&empty, &decoded) &&
                  GroupPane::CreateTabMimeData(-1) == nullptr,
                  "非分组拖动数据被拒绝");

            auto groupNode = [&](const QString& name) -> QTreeWidgetItem* {
                for (int i = 0; i < gp->topLevelItemCount(); ++i)
                    if (gp->topLevelItem(i)->text(0) == name) return gp->topLevelItem(i);
                return nullptr;
            };
            auto hasChild = [&](const QString& group, int idx) {
                QTreeWidgetItem* g = groupNode(group);
                for (int i = 0; g && i < g->childCount(); ++i)
                    if (g->child(i)->data(0, Qt::UserRole).toInt() == idx) return true;
                return false;
            };
            auto childCount = [&](const QString& group, int idx) {
                int n = 0;
                QTreeWidgetItem* g = groupNode(group);
                for (int i = 0; g && i < g->childCount(); ++i)
                    if (g->child(i)->data(0, Qt::UserRole).toInt() == idx) ++n;
                return n;
            };

            CHECK(w.CreateGroup(QStringLiteral("测试组"), {}), "新建空分组");
            w.MoveTabToGroup(idx1, QStringLiteral("测试组"));
            QApplication::processEvents();
            CHECK(hasChild(QStringLiteral("测试组"), idx1), "窗口移动到目标组");
            w.MoveTabToGroup(idx1, QStringLiteral("测试组"));
            CHECK(childCount(QStringLiteral("测试组"), idx1) == 1,
                  "重复拖入同一组不产生重复成员");

            CHECK(w.CreateGroup(QStringLiteral("测试组2"), {}), "新建第二个分组");
            w.MoveTabToGroup(idx1, QStringLiteral("测试组2"));
            QApplication::processEvents();
            CHECK(hasChild(QStringLiteral("测试组2"), idx1) &&
                  !hasChild(QStringLiteral("测试组"), idx1),
                  "窗口在分组间移动（一个窗口只属于一个停靠组）");

            // 对话框新建分组（勾选窗口）与拖动共用同一归属规则
            CHECK(w.CreateGroup(QStringLiteral("测试组3"), {idx1}), "新建分组可直接包含窗口");
            CHECK(hasChild(QStringLiteral("测试组3"), idx1) &&
                  !hasChild(QStringLiteral("测试组2"), idx1),
                  "新建分组把已归组的窗口移出原组");
            w.MoveTabToGroup(idx1, QStringLiteral("测试组2"));
            QApplication::processEvents();

            // 分组排布跟随拖动更新可见集合
            w.SetViewLayout(bes::ViewLayout::GroupH);
            w.MoveTabToGroup(idx0, QStringLiteral("测试组2"));
            CHECK(w.VisibleViewCount() == 2, "同组两个窗口在分组横排时同屏");
            w.MoveTabToGroup(idx0, QStringLiteral("数据窗口"));
            CHECK(w.VisibleViewCount() == 1, "移出一个窗口后只剩组内成员同屏");
            w.MoveTabToGroup(idx1, QStringLiteral("数据窗口"));
            CHECK(hasChild(QStringLiteral("数据窗口"), idx1) &&
                  !hasChild(QStringLiteral("测试组2"), idx1),
                  "拖回「数据窗口」移出所有自定义分组");
            CHECK(w.VisibleViewCount() == area->count(),
                  "分组为空时分组排布回退为全部视图");
            w.SetViewLayout(bes::ViewLayout::Paged);

            // 真实 drop 事件：解出 MIME → 队列信号 → 重新归组
            QTreeWidgetItem* target = groupNode(QStringLiteral("测试组"));
            if (target) {
                gp->expandItem(target);
                QApplication::processEvents();
                const QPoint pos = gp->visualItemRect(target).center();
                QMimeData* dragData = GroupPane::CreateTabMimeData(idx1);
                // QApplication::notify 只把 Drop 转发给 QDragManager 的 currentTarget，
                // 必须先让 DragEnter 被接受，Drop 才会真正进入视图。
                QDragEnterEvent enter(pos, Qt::MoveAction, dragData,
                                      Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(gp->viewport(), &enter);
                CHECK(enter.isAccepted(), "分组树接受窗口拖动进入");
                QDropEvent drop(pos, Qt::MoveAction, dragData, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(gp->viewport(), &drop);
                delete dragData;
                QApplication::processEvents();
                QApplication::processEvents();
                CHECK(hasChild(QStringLiteral("测试组"), idx1),
                      "分组树 drop 事件把窗口移入落点分组");
            }
            w.MoveTabToGroup(idx1, QStringLiteral("数据窗口"));
        }
    }

    // 快捷键按原版 TdxBarManager ShortCut（DFM 二进制流）核对
    {
        auto sc = [&](int id) {
            QAction* a = w.Cmds()->Find(id);
            return a ? a->shortcut() : QKeySequence();
        };
        CHECK(sc(CMD_FILE_NEW) == QKeySequence(QStringLiteral("Ctrl+N")) &&
              sc(CMD_FILE_OPEN) == QKeySequence(QStringLiteral("Ctrl+O")) &&
              sc(CMD_FILE_SAVE) == QKeySequence(QStringLiteral("Ctrl+S")) &&
              sc(CMD_FILE_PRINT) == QKeySequence(QStringLiteral("Ctrl+P")),
              "文件快捷键与原版一致（Ctrl+N/O/S/P）");
        CHECK(sc(CMD_EDIT_CUT) == QKeySequence(QStringLiteral("Ctrl+X")) &&
              sc(CMD_EDIT_COPY) == QKeySequence(QStringLiteral("Ctrl+C")) &&
              sc(CMD_EDIT_PASTE) == QKeySequence(QStringLiteral("Ctrl+V")) &&
              sc(CMD_EDIT_UNDO) == QKeySequence(QStringLiteral("Ctrl+Z")) &&
              sc(CMD_EDIT_REDO) == QKeySequence(QStringLiteral("Ctrl+Shift+Z")),
              "编辑快捷键与原版一致（X/C/V/Z、重做 Shift+Ctrl+Z）");
        CHECK(sc(CMD_VIEW_LINK) == QKeySequence(QStringLiteral("Ctrl+A")),
              "联动沿用原版 Ctrl+A");
        CHECK(sc(CMD_SET_START) == QKeySequence(QStringLiteral("Ctrl+B")),
              "设为起点沿用原版 Ctrl+B");
        CHECK(sc(CMD_DATA_NOT) == QKeySequence(QStringLiteral("N")),
              "取反沿用原版裸键 N");
    }

    // 截图
    // 主菜单结构按 BesCom.dll DFM 的 TdxBarManager 对齐
    {
        auto findMenu = [&](const QString& text) -> QMenu* {
            for (QAction* a : w.menuBar()->actions())
                if (a->text() == text) return a->menu();
            return nullptr;
        };
        QMenu* cus = findMenu(QStringLiteral("定制(&C)"));
        QStringList cusCaps;
        if (cus)
            for (QAction* a : cus->actions())
                if (!a->isSeparator()) cusCaps << a->text();
        CHECK(cusCaps == (QStringList{QStringLiteral("管理..."),
                                      QStringLiteral("快速执行...")}),
              "custom menu holds only manage/fast-execute per DFM");

        QMenu* view = findMenu(QStringLiteral("查看(&V)"));
        CHECK(view && view->actions().contains(w.Cmds()->Find(CMD_ANALYZE_PARAM)) &&
              view->actions().contains(w.Cmds()->Find(CMD_ANALYZE_VARFRAME)) &&
              view->actions().contains(w.Cmds()->Find(CMD_DATA_NOT)) &&
              view->actions().contains(w.Cmds()->Find(CMD_DATA_REVERSE)) &&
              view->actions().contains(w.Cmds()->Find(CMD_VIEW_RULER)),
              "view menu carries ruler/not/reverse/param/varframe");

        // 查看菜单内嵌格式栏的四个栏控件（原版同一 ItemLinks 同时挂在两处）
        {
            QLineEdit* mStart = w.findChild<QLineEdit*>(QStringLiteral("menuEdStart"));
            QLineEdit* mFrame = w.findChild<QLineEdit*>(QStringLiteral("menuEdFrameLen"));
            QLineEdit* mStep  = w.findChild<QLineEdit*>(QStringLiteral("menuEdStep"));
            QComboBox* mMode  = w.findChild<QComboBox*>(QStringLiteral("menuCboViewMode"));
            QLineEdit* fStart = w.findChild<QLineEdit*>(QStringLiteral("edStart"));
            QComboBox* fMode  = w.findChild<QComboBox*>(QStringLiteral("cboViewMode"));
            CHECK(view && mStart && mFrame && mStep && mMode && fStart && fMode,
                  "view menu embeds start/frame-length/step/display controls");
            if (mStart && mFrame && mStep && mMode && fStart && fMode) {
                CHECK(mStart->text() == fStart->text(),
                      "menu start control mirrors format bar");
                CHECK(mMode->count() == 5,
                      "menu display combo has the original 5 items");

                const ViewMode savedView = doc->State().params.view;
                const uint64_t savedStart = doc->State().frame.startBit;
                mStart->setText(QStringLiteral("16"));
                mStart->editingFinished();
                QApplication::processEvents();
                CHECK(doc->State().frame.startBit ==
                      16ull * static_cast<uint64_t>(Renderer::BitsPerCell(savedView)),
                      "menu start edit changes the document start");
                CHECK(fStart->text() == QStringLiteral("16"),
                      "format bar start follows the menu edit");
                mMode->setCurrentIndex(4);
                QApplication::processEvents();
                CHECK(doc->State().params.view == ViewMode::HexChar &&
                      fMode->currentIndex() == 4,
                      "menu display combo selects hex+ASCII and syncs format bar");
                doc->State().frame.startBit = savedStart;
                doc->State().params.view = savedView;
                w.UpdateAllViews();
            }
        }

        QMenu* win = findMenu(QStringLiteral("窗口(&W)"));
        bool layouts = win != nullptr;
        for (int id : { CMD_VIEW_PAGE, CMD_VIEW_HORIZ, CMD_VIEW_VERT,
                        CMD_VIEW_GROUP_HORIZ, CMD_VIEW_GROUP_VERT, CMD_VIEW_TITLE })
            layouts = layouts && win->actions().contains(w.Cmds()->Find(id));
        CHECK(layouts, "window menu carries all five layouts and title");

        QMenu* edit = findMenu(QStringLiteral("编辑(&E)"));
        bool hasSelSub = false;
        if (edit)
            for (QAction* a : edit->actions())
                if (a->menu() && a->text() == QStringLiteral("选取参数")) hasSelSub = true;
        CHECK(edit && edit->actions().contains(w.Cmds()->Find(CMD_EDIT_MODE)) &&
              edit->actions().contains(w.Cmds()->Find(CMD_ANALYZE_SEARCH_UNIQUE)) &&
              edit->actions().contains(w.Cmds()->Find(CMD_ANALYZE_SEARCH_FL)) && hasSelSub,
              "edit menu carries edit-mode, searches and selection submenu");

        CHECK(findMenu(QStringLiteral("识别(&R)")) != nullptr,
              "identify menu uses original (&R) mnemonic");

        bool phantom = false;
        for (QAction* a : w.findChildren<QAction*>())
            if (a->text().contains(QStringLiteral("串口管理"))) phantom = true;
        CHECK(!phantom, "no phantom serial-port command");
    }

    QApplication::processEvents();
    const QPixmap pm = w.grab();
    CHECK(!pm.isNull(), "window grab works");
    const QString shot = QDir::tempPath() + QStringLiteral("/bes_ui_test.png");
    CHECK(pm.save(shot), "screenshot saved");

    // 视图右键菜单：原版 TMDIChild.FormMouseUp@0x405AE0 在右键时弹出隐藏栏「快捷操作」的 ItemLinks，
    // 顺序/分隔与该栏一致；「清除标签」对应 ClearBookMarkBtn（书签接口 vtbl+0x48）。
    {
        QAction* bmClear = w.Cmds()->Find(CMD_BOOKMARK_CLEAR);
        CHECK(bmClear && bmClear->text() == QStringLiteral("清除标签"),
              "clear-bookmark command uses original caption");
        auto* quickBar = w.findChild<QToolBar*>(QStringLiteral("quickBar"));
        CHECK(quickBar && quickBar->actions().contains(bmClear) &&
              !quickBar->actions().contains(w.Cmds()->Find(CMD_SELECT_CLEAR_CURRENT)),
              "quick bar clears bookmarks, not the current selection");

        QAction* bmToggle = w.Cmds()->Find(CMD_BOOKMARK_TOGGLE);
        doc->SetCursorBit(64);
        if (bmToggle) bmToggle->trigger();
        doc->SetCursorBit(128);
        if (bmToggle) bmToggle->trigger();
        CHECK(doc->State().marks.Size() == 2, "two bookmarks set");
        if (bmClear) bmClear->trigger();
        CHECK(doc->State().marks.Size() == 0, "clear-bookmark empties all marks");

        QMenu* ctx = view->CreateContextMenu();
        CHECK(ctx != nullptr, "view context menu available");
        if (ctx) {
            const QList<QAction*> acts = ctx->actions();
            const QVector<int> ids = {
                CMD_VIEW_LINK, CMD_SET_START, 0,
                CMD_SELECT_CONTINUOUS, CMD_SELECT_RECT, CMD_SELECT_ROW, CMD_SELECT_COL, -1,
                0,
                CMD_DATA_OUTPUT, CMD_FILE_SAVE_AS, 0,
                CMD_ANALYZE_SEARCH_FL, CMD_ANALYZE_SEARCH_UNIQUE,
                CMD_ANALYZE_QUIT_SEARCH, 0,
                CMD_BOOKMARK_TOGGLE, CMD_BOOKMARK_NEXT, CMD_BOOKMARK_PREV,
                CMD_BOOKMARK_CLEAR, 0,
                CMD_ANALYZE_GOTO
            };
            bool order = (acts.size() == ids.size());
            for (int i = 0; order && i < ids.size(); ++i) {
                if (ids[i] == 0) order = acts[i]->isSeparator();
                else if (ids[i] == -1) order = acts[i]->menu() != nullptr;
                else order = (acts[i] == w.Cmds()->Find(ids[i]));
            }
            CHECK(order, "view context menu follows the hidden 快捷操作 bar");
            QMenu* selMenu = nullptr;
            for (QAction* a : acts)
                if (a->menu()) selMenu = a->menu();
            auto selHas = [&](int id) {
                return selMenu && selMenu->actions().contains(w.Cmds()->Find(id));
            };
            CHECK(selHas(CMD_SELECT_LOAD) && selHas(CMD_SELECT_SAVE) &&
                  selHas(CMD_SELECT_CLEAR_CURRENT) && selHas(CMD_SELECT_CLEAR_ALL),
                  "context selection submenu carries load/save/clear");
            delete ctx;
        }
    }

    // 工具栏按钮级定制（原版 dxBarManager1 + TdxBarCustomizingForm）：
    // 每条工具栏保存命令 ID 的有序列表，0 = 分隔符，-1 = 格式栏纵轴下拉框占位。
    {
        CHECK(w.ToolBarItems(MainWindow::BarMain) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarMain) &&
              w.ToolBarItems(MainWindow::BarFormat) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarFormat) &&
              w.ToolBarItems(MainWindow::BarQuick) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarQuick),
              "toolbars start with the DFM ItemLinks order");

        auto* fb = w.findChild<QToolBar*>(QStringLiteral("formatBar"));
        const QVector<int> custom = {
            CMD_VIEW_MODE_HEX, MainWindow::BarSeparator, CMD_ANALYZE_PARAM };
        w.SetToolBarItems(MainWindow::BarFormat, custom);
        QApplication::processEvents();
        CHECK(w.ToolBarItems(MainWindow::BarFormat) == custom &&
              fb && fb->actions().contains(w.Cmds()->Find(CMD_VIEW_MODE_HEX)) &&
              fb->actions().contains(w.Cmds()->Find(CMD_ANALYZE_PARAM)) &&
              !fb->actions().contains(w.Cmds()->Find(CMD_VIEW_RULER)) &&
              w.Cmds()->Find(CMD_VIEW_RULER) != nullptr,
              "customizing a bar only changes its buttons, not the commands");
        int sepCount = 0;
        for (QAction* a : fb->actions())
            if (a->isSeparator()) ++sepCount;
        CHECK(sepCount == 1, "custom separator is rebuilt exactly once");
        CHECK(w.findChild<QComboBox*>(QStringLiteral("cboViewMode")) != nullptr,
              "format view-mode combo survives a bar rebuild");
        CHECK(w.findChild<QComboBox*>(QStringLiteral("cboYDisp")) != nullptr,
              "format YDisp combo survives a bar rebuild");
        CHECK(w.findChild<QLineEdit*>(QStringLiteral("edStart")) != nullptr,
              "format start spin survives a bar rebuild");

        auto* tbQuick = w.findChild<QAction*>(QStringLiteral("tb_quick"));
        auto* quickBar = w.findChild<QToolBar*>(QStringLiteral("quickBar"));
        auto* mainBar = w.findChild<QToolBar*>(QStringLiteral("mainBar"));
        w.SetBarVisible(MainWindow::BarQuick, false);
        QApplication::processEvents();
        CHECK(quickBar && quickBar->isHidden() && tbQuick && !tbQuick->isChecked(),
              "hiding one bar syncs the window-menu check state");
        CHECK(mainBar && !mainBar->isHidden(), "other bars stay visible");
        w.SetBarVisible(MainWindow::BarQuick, true);
        CHECK(quickBar && !quickBar->isHidden() && tbQuick && tbQuick->isChecked(),
              "showing the bar restores it");

        QMenu* cm = w.BuildToolBarContextMenu();
        bool ctxOk = cm && cm->actions().size() == 2 * MainWindow::BarCount - 1;
        for (int i = 0; cm && i < MainWindow::BarCount; ++i) {
            QAction* a = cm->actions()[i];
            ctxOk = ctxOk && a->isCheckable() && a->isChecked() && a->data().toInt() == i &&
                    a->text() == MainWindow::ToolBarName(i);
        }
        CHECK(ctxOk && cm->actions().last()->objectName() ==
                  QStringLiteral("barCtxCustomize"),
              "toolbar context menu lists every bar plus customize");
        delete cm;

        DlgCustomize dlg(&w);
        auto* tabs = dlg.findChild<QTabWidget*>(QStringLiteral("custTabs"));
        CHECK(dlg.windowTitle() == QStringLiteral("Customize") && tabs && tabs->count() == 2 &&
              tabs->tabText(0) == QStringLiteral(" Toolbars ") &&
              tabs->tabText(1) == QStringLiteral(" Commands "),
              "customize dialog mirrors TdxBarCustomizingForm");
        auto* barCombo = dlg.findChild<QComboBox*>(QStringLiteral("custBarCombo"));
        auto* cur = dlg.findChild<QListWidget*>(QStringLiteral("custCurrent"));
        auto* avail = dlg.findChild<QListWidget*>(QStringLiteral("custAvailable"));
        auto* addBtn = dlg.findChild<QPushButton*>(QStringLiteral("custAdd"));
        auto* remBtn = dlg.findChild<QPushButton*>(QStringLiteral("custRemove"));
        auto* upBtn = dlg.findChild<QPushButton*>(QStringLiteral("custUp"));
        auto* resetBtn = dlg.findChild<QPushButton*>(QStringLiteral("custReset"));
        auto* closeBtn = dlg.findChild<QPushButton*>(QStringLiteral("custClose"));
        CHECK(barCombo && cur && avail && addBtn && remBtn && upBtn && resetBtn && closeBtn &&
              barCombo->count() == MainWindow::BarCount,
              "customize dialog exposes bars, lists and commands");

        if (barCombo) barCombo->setCurrentIndex(barCombo->findData(MainWindow::BarQuick));
        QApplication::processEvents();
        QListWidgetItem* src = nullptr;
        for (int i = 0; avail && i < avail->count(); ++i)
            if (avail->item(i)->data(Qt::UserRole).toInt() == CMD_FILE_NEW)
                src = avail->item(i);
        CHECK(src != nullptr, "available list carries every command");
        const int beforeAdd = cur ? cur->count() : 0;
        if (src) avail->setCurrentItem(src);
        CHECK(addBtn && addBtn->isEnabled(), "add enabled once a command is selected");
        if (addBtn) addBtn->click();
        CHECK(cur && cur->count() == beforeAdd + 1 &&
              w.ToolBarItems(MainWindow::BarQuick).last() == CMD_FILE_NEW,
              "adding a command appends it to the chosen bar");
        if (cur) {
            cur->setCurrentRow(cur->count() - 1);
            if (upBtn) upBtn->click();
        }
        const QVector<int> moved = w.ToolBarItems(MainWindow::BarQuick);
        CHECK(moved.size() >= 2 && moved[moved.size() - 2] == CMD_FILE_NEW &&
              moved.last() == CMD_ANALYZE_GOTO,
              "up moves the selected command one position");
        int newRow = -1;
        for (int i = 0; cur && i < cur->count(); ++i)
            if (cur->item(i)->data(Qt::UserRole).toInt() == CMD_FILE_NEW) newRow = i;
        if (cur && newRow >= 0) {
            cur->setCurrentRow(newRow);
            if (remBtn) remBtn->click();
        }
        CHECK(!w.ToolBarItems(MainWindow::BarQuick).contains(CMD_FILE_NEW),
              "remove drops the selected command");

        w.SetToolBarItems(MainWindow::BarFormat,
                          MainWindow::DefaultToolBarItems(MainWindow::BarFormat));
        if (barCombo) barCombo->setCurrentIndex(barCombo->findData(MainWindow::BarFormat));
        QApplication::processEvents();
        bool markerLocked = false;
        for (int i = 0; cur && i < cur->count(); ++i)
            if (cur->item(i)->data(Qt::UserRole).toInt() == MainWindow::BarWidgetMarker)
                markerLocked = !(cur->item(i)->flags() & Qt::ItemIsSelectable);
        CHECK(markerLocked, "YDisp widget placeholder cannot be removed");

        if (resetBtn) resetBtn->click();
        CHECK(w.ToolBarItems(MainWindow::BarFormat) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarFormat) &&
              w.ToolBarItems(MainWindow::BarQuick) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarQuick),
              "Reset restores the original layouts");

        auto* cbQuick = dlg.findChild<QCheckBox*>(QStringLiteral("custBar2"));
        CHECK(cbQuick && cbQuick->isChecked(), "toolbars page mirrors visibility");
        if (cbQuick) cbQuick->setChecked(false);
        QApplication::processEvents();
        CHECK(quickBar && quickBar->isHidden(), "unchecking hides the bar immediately");
        if (cbQuick) cbQuick->setChecked(true);
        if (closeBtn) closeBtn->click();
        CHECK(dlg.result() == QDialog::Accepted, "Close dismisses the customize dialog");
        QApplication::processEvents();
    }

    // 状态栏：原版 StatusBar1 三面板（文件 380 / 光标 400 / 插入 30），
    // 文本格式取自 BesCom.dll 中的 GBK 字符串（标记 / 选取了 ...）。
    {
        auto* fileLbl = w.findChild<QLabel*>(QStringLiteral("statusFile"));
        auto* curLbl  = w.findChild<QLabel*>(QStringLiteral("statusCursor"));
        CHECK(fileLbl && curLbl, "status file/cursor panels present");
        CHECK(fileLbl && fileLbl->minimumWidth() == 380 &&
              fileLbl->maximumWidth() == 380, "file panel is 380px wide");
        CHECK(curLbl && curLbl->minimumWidth() == 400,
              "cursor panel reserves 400px");
        CHECK(fileLbl && fileLbl->text() ==
              QStringLiteral("文件: %1").arg(doc->FilePath()),
              "file panel shows only 文件: path");

        doc->State().sel.ClearAll();
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.startBit = 0;
        doc->State().frame.frameLen = 16;
        doc->SetCursorBit(0);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() ==
              QStringLiteral("标记: 文件第0比特 第0比特 0行 0列"),
              "marker text in bit units without distance");

        doc->State().frame.startBit = 8;
        doc->SetCursorBit(20);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() ==
              QStringLiteral("标记: 文件第8比特 第20比特 0行 12列 距离[12]"),
              "marker text appends 距离 when cursor moved");

        doc->State().params.view = ViewMode::Hex;
        doc->State().frame.startBit = 8;
        doc->State().frame.frameLen = 32;
        doc->SetCursorBit(24);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() ==
              QStringLiteral("标记: 第0行 2列 距离[2]"),
              "byte views report row/col only, distance in bytes");

        doc->State().sel.SetContinuous(0, 31);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 4 字节"),
              "byte view continuous selection counts 字节");

        doc->State().params.view = ViewMode::BitBitmap;
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 32 比特"),
              "bit view continuous selection counts 比特");

        doc->State().sel.ClearAll();
        RectSel rect;
        rect.frame0 = 1; rect.cell0 = 2;
        rect.frame1 = 3; rect.cell1 = 5;
        doc->State().sel.AddRect(rect);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 3行×4列"),
              "rect selection reports row/col extents");

        doc->State().sel.ClearAll();
        doc->State().sel.AddRow(0);
        doc->State().sel.AddRow(1);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 2 行"),
              "bit view row selection keeps the space before 行");

        doc->State().params.view = ViewMode::Hex;
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 2行"),
              "byte view row selection has no space before 行");

        doc->State().sel.ClearAll();
        doc->State().sel.AddCol(3);
        doc->State().sel.AddCol(5);
        doc->State().sel.AddCol(7);
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text() == QStringLiteral("选取了 3 列"),
              "col selection reports column count");

        doc->State().sel.ClearAll();
        doc->State().params.view = ViewMode::BitBitmap;
        w.UpdateStatusText();
        CHECK(curLbl && curLbl->text().startsWith(QStringLiteral("标记: ")),
              "empty selection restores marker text");
        doc->State().frame.frameLen = 0;   // 还原按屏宽自动的帧长
    }

    // 选取模式按钮与累积语义：原版 SelectBtnClick@0x40AC88 + SetSelectMode@0x43FFBC +
    // 清列表核心 0x4404CC + 松开提交 0x444924。
    {
        doc->State().sel.ClearAll();
        QAction* cont = w.Cmds()->Find(CMD_SELECT_CONTINUOUS);
        QAction* rect = w.Cmds()->Find(CMD_SELECT_RECT);
        CHECK(cont && rect && cont->isCheckable() && rect->isCheckable(),
              "selection mode actions are checkable (bsChecked)");

        cont->trigger();
        CHECK(doc->State().sel.Mode() == SelectMode::Continuous && cont->isChecked(),
              "continuous button activates continuous mode");
        doc->State().sel.AddContinuous(0, 7);
        doc->State().sel.AddContinuous(16, 23);
        CHECK(doc->State().sel.Continuouses().size() == 2,
              "two drags accumulate two continuous records (no Ctrl needed)");

        w.DoSelectMode(SelectMode::Rect);
        CHECK(doc->State().sel.Mode() == SelectMode::Rect &&
              doc->State().sel.Continuouses().empty(),
              "switching mode clears only the previous mode's list");
        CHECK(rect->isChecked() && !cont->isChecked(),
              "checked state follows the active selection mode");

        RectSel rr; rr.frame0 = 0; rr.cell0 = 0; rr.frame1 = 0; rr.cell1 = 1;
        doc->State().sel.AddRect(rr);
        w.DoSelectMode(SelectMode::Rect);
        CHECK(doc->State().sel.Empty() && doc->State().sel.Mode() == SelectMode::None,
              "re-clicking the active mode clears all lists and exits the mode");
        CHECK(!rect->isChecked(), "no mode button stays checked after exit");

        // 鼠标：按下/移动只预览，松开提交；第二次拖动继续累积
        w.DoSelectMode(SelectMode::Continuous);
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.startBit = 0;
        doc->State().frame.frameLen = 64;
        // 前面的排布测试把视图隐藏后宽度压到 0；恢复显示并让布局跑一轮
        view->show();
        w.resize(1100, 720);
        QApplication::processEvents();
        view->resize(800, 400);
        QApplication::processEvents();
        view->UpdateScrollRange();
        view->SyncToBit(0);
        const QPoint a(view->YAxisWidth() + 40, 120);
        auto drag = [&](const QPoint& from, const QPoint& to,
                        Qt::KeyboardModifiers mods, size_t before) {
            QMouseEvent press(QEvent::MouseButtonPress, from, Qt::LeftButton,
                              Qt::LeftButton, mods);
            QApplication::sendEvent(view->viewport(), &press);
            QMouseEvent move(QEvent::MouseMove, to, Qt::NoButton,
                             Qt::LeftButton, mods);
            QApplication::sendEvent(view->viewport(), &move);
            CHECK(doc->State().sel.Continuouses().size() == before,
                  "drag preview does not modify the selection list");
            QMouseEvent rel(QEvent::MouseButtonRelease, to, Qt::LeftButton,
                            Qt::NoButton, mods);
            QApplication::sendEvent(view->viewport(), &rel);
        };
        drag(a, a + QPoint(30, 0), Qt::NoModifier, 0);
        CHECK(doc->State().sel.Continuouses().size() == 1,
              "mouse release commits one continuous record");
        drag(a + QPoint(0, 20), a + QPoint(30, 20), Qt::ControlModifier, 1);
        CHECK(doc->State().sel.Continuouses().size() == 2,
              "second drag accumulates (Ctrl makes no difference)");
        CHECK(doc->State().sel.Continuouses()[0].startBit <=
              doc->State().sel.Continuouses()[1].startBit,
              "committed continuous records stay sorted by start bit");
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.frameLen = 0;
        view->UpdateScrollRange();
        doc->State().sel.ClearAll();
    }

    // 键盘扩展（原版 KeyDown@0x445B0C / KeyUp@0x4477DC）：Shift+导航键移动端点，
    // 松开 Shift 才提交；Esc 取消；无选取模式时 Shift+导航键不动作。
    {
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.startBit = 0;
        doc->State().frame.frameLen = 64;
        doc->State().sel.ClearAll();
        w.DoSelectMode(SelectMode::Continuous);
        view->show();
        view->resize(800, 400);
        QApplication::processEvents();
        view->UpdateScrollRange();
        view->SyncToBit(0);
        view->verticalScrollBar()->setValue(0);
        view->horizontalScrollBar()->setValue(0);
        doc->SetCursorBit(16);

        auto key = [&](QEvent::Type t, int k, Qt::KeyboardModifiers m) {
            QKeyEvent e(t, k, m);
            QApplication::sendEvent(view, &e);
        };

        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        CHECK(doc->State().sel.Continuouses().empty(),
              "Shift+Right only previews, no commit");
        CHECK(doc->CursorBit() == 17, "Shift+Right moves the endpoint one cell");
        key(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        CHECK(doc->CursorBit() == 81, "Shift+Down moves one row keeping the column");
        key(QEvent::KeyPress, Qt::Key_End, Qt::ShiftModifier);
        CHECK(doc->CursorBit() == 127, "Shift+End goes to the row end");
        key(QEvent::KeyPress, Qt::Key_Home, Qt::ShiftModifier);
        CHECK(doc->CursorBit() == 64, "Shift+Home goes to the row start");
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Continuouses().size() == 1 &&
              doc->State().sel.Continuouses()[0].startBit == 16 &&
              doc->State().sel.Continuouses()[0].endBit == 64,
              "releasing Shift commits anchor..endpoint as one record");

        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        key(QEvent::KeyPress, Qt::Key_Escape, Qt::ShiftModifier);
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Continuouses().size() == 1,
              "Esc cancels the pending keyboard selection");

        // 块/行/列模式沿用同一端点移动，但提交为对应记录
        w.DoSelectMode(SelectMode::Rect);
        doc->SetCursorBit(0);
        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        key(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Rects().size() == 1 &&
              doc->State().sel.Rects()[0].frame1 == 1 &&
              doc->State().sel.Rects()[0].cell1 == 1,
              "Shift+arrows commit a rect record");

        w.DoSelectMode(SelectMode::Row);
        doc->SetCursorBit(0);
        key(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        key(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Rows().size() == 3,
              "Shift+Down commits a row range");

        w.DoSelectMode(SelectMode::Col);
        doc->SetCursorBit(0);
        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Cols().size() == 3,
              "Shift+Right commits a column range");

        // 无选取模式：Shift+导航键不动作（原版模式 -1 时早退）
        w.DoSelectMode(SelectMode::Col);   // 再点一次 → 清空并退出模式
        doc->SetCursorBit(100);
        key(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        key(QEvent::KeyRelease, Qt::Key_Shift, Qt::NoModifier);
        CHECK(doc->State().sel.Mode() == SelectMode::None &&
              doc->State().sel.Empty(),
              "Shift+arrows do nothing without a selection mode");

        // 拖出视口自动滚动（原版 +0x52 / PtInRect）：端点跟到屏幕边缘格，松开提交
        doc->State().frame.frameLen = 64;
        view->UpdateScrollRange();
        view->SyncToBit(0);
        view->verticalScrollBar()->setValue(0);
        doc->SetCursorBit(0);
        doc->State().sel.ClearAll();
        w.DoSelectMode(SelectMode::Continuous);
        const QPoint inside(view->YAxisWidth() + 40, 100);
        const QPoint outside(view->YAxisWidth() + 40, view->viewport()->height() + 20);
        QMouseEvent press(QEvent::MouseButtonPress, inside, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &press);
        QMouseEvent move(QEvent::MouseMove, outside, Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &move);
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 500 && view->verticalScrollBar()->value() == 0)
            QApplication::processEvents(QEventLoop::AllEvents, 20);
        CHECK(view->verticalScrollBar()->value() > 0,
              "dragging outside the viewport auto-scrolls");
        QMouseEvent rel(QEvent::MouseButtonRelease, outside, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &rel);
        CHECK(doc->State().sel.Continuouses().size() == 1,
              "auto-scroll drag commits on release");

        doc->State().frame.frameLen = 0;
        doc->State().sel.ClearAll();
        view->UpdateScrollRange();
    }

    // 行/列选取：动作进入模式、鼠标提交，再点同一动作取消
    {
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.startBit = 0;
        doc->State().frame.frameLen = 64;
        doc->State().sel.ClearAll();
        view->UpdateScrollRange();
        view->SyncToBit(0);
        view->verticalScrollBar()->setValue(0);
        view->horizontalScrollBar()->setValue(0);

        QAction* row = w.Cmds()->Find(CMD_SELECT_ROW);
        QAction* col = w.Cmds()->Find(CMD_SELECT_COL);
        auto click = [&](const QPoint& pt) {
            QMouseEvent press(QEvent::MouseButtonPress, pt, Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &press);
            QMouseEvent rel(QEvent::MouseButtonRelease, pt, Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &rel);
        };

        row->trigger();
        CHECK(doc->State().sel.Mode() == SelectMode::Row && row->isChecked(),
              "行选取按钮进入行模式");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(!doc->State().sel.Rows().empty(), "行模式单击提交一行");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(doc->State().sel.Rows().empty() && doc->State().sel.Mode() == SelectMode::Row,
              "再次单击已选行取消该行并保持行模式");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(doc->State().sel.Rows().size() == 1, "第三次单击重新选回该行");
        row->trigger();
        CHECK(doc->State().sel.Rows().empty() && doc->State().sel.Mode() == SelectMode::None &&
              !row->isChecked(), "再点行选取按钮取消行选区并退出模式");

        col->trigger();
        CHECK(doc->State().sel.Mode() == SelectMode::Col && col->isChecked(),
              "列选取按钮进入列模式");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(!doc->State().sel.Cols().empty(), "列模式单击提交一列");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(doc->State().sel.Cols().empty() && doc->State().sel.Mode() == SelectMode::Col,
              "再次单击已选列取消该列并保持列模式");
        click(QPoint(view->YAxisWidth() + 20, 100));
        CHECK(doc->State().sel.Cols().size() == 1, "第三次单击重新选回该列");
        col->trigger();
        CHECK(doc->State().sel.Cols().empty() && doc->State().sel.Mode() == SelectMode::None &&
              !col->isChecked(), "再点列选取按钮取消列选区并退出模式");

        // 清除当前选取同样应清掉行/列列表
        row->trigger();
        click(QPoint(view->YAxisWidth() + 20, 100));
        w.DoClearCurrentSelection();
        CHECK(doc->State().sel.Rows().empty() && doc->State().sel.Mode() == SelectMode::Row,
              "清除当前选取清掉行列表但保留模式");
        row->trigger();
        doc->State().frame.frameLen = 0;
        doc->State().sel.ClearAll();
        view->UpdateScrollRange();
    }

    // 无 Shift 的方向键：移动光标（标记）并清掉当前模式列表（原版 0x44611D 尾段）
    {
        doc->State().params.view = ViewMode::BitBitmap;
        doc->State().frame.startBit = 0;
        doc->State().frame.frameLen = 64;
        doc->State().sel.ClearAll();
        w.DoSelectMode(SelectMode::Continuous);
        view->UpdateScrollRange();
        view->SyncToBit(0);
        view->verticalScrollBar()->setValue(0);
        view->horizontalScrollBar()->setValue(0);

        auto key = [&](QEvent::Type t, int k) {
            QKeyEvent e(t, k, Qt::NoModifier);
            QApplication::sendEvent(view, &e);
        };

        doc->SetCursorBit(16);
        doc->State().sel.AddContinuous(0, 7);
        key(QEvent::KeyPress, Qt::Key_Right);
        CHECK(doc->CursorBit() == 17, "Right moves the caret one cell");
        CHECK(doc->State().sel.Continuouses().empty(),
              "moving the caret clears the current mode's list");
        key(QEvent::KeyPress, Qt::Key_Down);
        CHECK(doc->CursorBit() == 81, "Down moves the caret one row");

        doc->SetCursorBit(63);
        key(QEvent::KeyPress, Qt::Key_Right);
        CHECK(doc->CursorBit() == 64, "Right wraps to the next row's first column");
        doc->SetCursorBit(64);
        key(QEvent::KeyPress, Qt::Key_Left);
        CHECK(doc->CursorBit() == 63, "Left wraps to the previous row's last column");
        doc->SetCursorBit(0);
        key(QEvent::KeyPress, Qt::Key_Left);
        CHECK(doc->CursorBit() == 0, "Left clamps at the first cell");

        doc->SetCursorBit(0);
        key(QEvent::KeyPress, Qt::Key_End);
        CHECK(doc->CursorBit() == 63, "End moves the caret to the row end");
        doc->SetCursorBit(63);
        key(QEvent::KeyPress, Qt::Key_Home);
        CHECK(doc->CursorBit() == 0, "Home moves the caret to the row start");

        doc->SetCursorBit(0);
        const int pg = std::max(1, view->verticalScrollBar()->pageStep());
        key(QEvent::KeyPress, Qt::Key_PageDown);
        CHECK(doc->CursorBit() == static_cast<uint64_t>(pg) * 64,
              "PageDown moves the caret by one page");

        doc->State().frame.frameLen = 0;
        doc->State().sel.ClearAll();
        view->UpdateScrollRange();
    }

    // 构件注册持久化：关闭时保存 DLL 列表，下一次启动重新探测（端口来自实例信息）
    {
        CHECK(!w.Components().Components().empty(), "component present before save");
        w.close();
        QApplication::processEvents();

        QSettings st;
        const QStringList dlls = st.value(QStringLiteral("plugins/dlls")).toStringList();
        CHECK(dlls.contains(plugin), "registered DLL path saved to QSettings");

        auto joinIds = [](const QVector<int>& v) {
            QStringList parts;
            for (int id : v) parts << QString::number(id);
            return parts.join(QLatin1Char(','));
        };
        CHECK(st.value(QStringLiteral("view/barMain")).toString() ==
                  joinIds(MainWindow::DefaultToolBarItems(MainWindow::BarMain)) &&
              st.value(QStringLiteral("view/barFormat")).toString() ==
                  joinIds(MainWindow::DefaultToolBarItems(MainWindow::BarFormat)) &&
              st.value(QStringLiteral("view/barQuick")).toString() ==
                  joinIds(MainWindow::DefaultToolBarItems(MainWindow::BarQuick)),
              "toolbar layouts persist on close");
        CHECK(st.value(QStringLiteral("view/tbMain"), false).toBool() &&
              st.value(QStringLiteral("view/tbFormat"), false).toBool() &&
              st.value(QStringLiteral("view/tbQuick"), false).toBool(),
              "toolbar visibility persists on close");

        MainWindow w2;
        CHECK(w2.ToolBarItems(MainWindow::BarFormat) ==
                  MainWindow::DefaultToolBarItems(MainWindow::BarFormat),
              "toolbar layout restored on next start");
        const ComponentInfo* restored = nullptr;
        for (const ComponentInfo& ci : w2.Components().Components())
            if (ci.name == L"TEST PLUGIN 1") restored = &ci;
        CHECK(restored && restored->regNameAnsi == "TEST.PLUGIN.1",
              "component restored on next start");

        InstanceInfo info;
        std::wstring iErr;
        CHECK(ComponentHost::QueryInstanceInfo(plugin.toStdWString(), "TEST.PLUGIN.1",
                                               &info, &iErr) ==
                  ComponentHost::InstanceInfoStatus::Ok &&
              info.ports.size() == 2 && info.ports[0].index == 1,
              "ports come from the instance info, not a cached config");
    }

    QSettings().clear();
    QFile::remove(tmp);
    QFile::remove(shot);

    if (g_fail == 0) printf("\nUI ALL PASSED (0 failures)\n");
    else printf("\nUI %d FAILURES\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
