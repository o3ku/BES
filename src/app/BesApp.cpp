// BesApp.cpp
#include "app/BesApp.h"

#include <windows.h>

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPixmap>
#include <QImage>
#include <QCoreApplication>
#include <QPalette>

#include "core/DataTypes.h"
#include "core/CrashLog.h"
#include "plugin/ComponentHost.h"
#include "app/MainWindow.h"
#include "app/EventPane.h"
#include "app/BesDoc.h"
#include "app/BesView.h"

BesApp::BesApp(int& argc, char** argv)
    : QApplication(argc, argv)
{
    const QString suffix = qEnvironmentVariable("BES_APP_SUFFIX");
    setApplicationName(QStringLiteral("BES") + suffix);
    setOrganizationName(QStringLiteral("BES"));
    m_components = std::make_unique<bes::ComponentHost>();
}

BesApp::~BesApp() = default;

BesApp* BesApp::Get()
{
    return static_cast<BesApp*>(qApp);
}

bool BesApp::EnsureSingleInstance()
{
    HANDLE h = ::CreateMutexW(nullptr, TRUE, L"_BESAPPLICATION_");
    if (!h)
        return true;
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(h);
        return false;
    }
    m_mutex = h;
    return true;
}

void BesApp::RegisterFileAssociation()
{
    // 显式创建 HKCU\Software\Classes\*\shell\浏览\command（原程序写空键名失败）
    HKEY hKey = nullptr;
    LONG l = ::RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\*\\shell\\浏览\\command", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (l != ERROR_SUCCESS)
        return;

    wchar_t exePath[MAX_PATH]{};
    ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t cmd[MAX_PATH + 16]{};
    swprintf_s(cmd, L"\"%s\" \"%%1\"", exePath);
    ::RegSetValueExW(hKey, L"", 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(cmd),
                     static_cast<DWORD>((::wcslen(cmd) + 1) * sizeof(wchar_t)));
    ::RegCloseKey(hKey);
    m_registered = true;
}

void BesApp::UnregisterFileAssociation()
{
    if (!m_registered)
        return;
    HKEY hKey = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\浏览",
                        0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        ::RegDeleteKeyW(hKey, L"command");
        ::RegCloseKey(hKey);
    }
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell",
                        0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        ::RegDeleteKeyW(hKey, L"浏览");
        ::RegCloseKey(hKey);
    }
    m_registered = false;
}

int BesApp::Run()
{
    bes::InstallCrashHandler();

    if (!EnsureSingleInstance()) {
        QMessageBox::information(nullptr, QStringLiteral("BES"),
                                  QStringLiteral("程序已经在运行。"));
        return 0;
    }
    RegisterFileAssociation();

    m_main = new MainWindow;
    m_main->show();

    // 命令行文件（右键 → 浏览）；--screenshot <png> 用于黄金截图对比
    const QStringList args = arguments();
    QString screenshot;
    QString viewName;
    int zoom = 0;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--screenshot") && i + 1 < args.size()) {
            screenshot = args[++i];
            continue;
        }
        if (args[i] == QStringLiteral("--view") && i + 1 < args.size()) {
            viewName = args[++i].toLower();
            continue;
        }
        if (args[i] == QStringLiteral("--zoom") && i + 1 < args.size()) {
            zoom = args[++i].toInt();
            continue;
        }
        if (QFileInfo::exists(args[i]))
            m_main->OpenDocumentFile(args[i]);
    }

    if (BesDoc* doc = m_main->ActiveDoc()) {
        if (!viewName.isEmpty()) {
            if (viewName == QStringLiteral("bit"))          doc->State().params.view = bes::ViewMode::BitBitmap;
            else if (viewName == QStringLiteral("bitchar")) doc->State().params.view = bes::ViewMode::BitChar;
            else if (viewName == QStringLiteral("hex"))     doc->State().params.view = bes::ViewMode::Hex;
            else if (viewName == QStringLiteral("char"))    doc->State().params.view = bes::ViewMode::Char;
            else if (viewName == QStringLiteral("pixel"))   doc->State().params.view = bes::ViewMode::Pixel;
            else if (viewName == QStringLiteral("hexchar")) doc->State().params.view = bes::ViewMode::HexChar;
        }
        if (zoom > 0) doc->State().params.zoom = zoom;
        if (BesView* v = m_main->ActiveView()) v->UpdateScrollRange();
        m_main->UpdateAllViews();
    }

    if (!screenshot.isEmpty()) {
        // 黄金截图不包含事件时间等易变内容
        if (auto* ep = m_main->findChild<EventPane*>())
            ep->setRowCount(0);
        // 无前台焦点时 Qt 认为窗口处于 Inactive 态，菜单栏改用调色板的 Inactive
        // 组（文字变灰），同一画面会产生两种哈希；截图模式把 Active 组复制进
        // Inactive 组，抓取结果便与桌面激活状态无关。
        QPalette pal = QApplication::palette();
        for (int r = 0; r < QPalette::NColorRoles; ++r) {
            const auto role = static_cast<QPalette::ColorRole>(r);
            pal.setColor(QPalette::Inactive, role, pal.color(QPalette::Active, role));
        }
        QApplication::setPalette(pal);
        QCoreApplication::processEvents();
        const QPixmap pm = m_main->grab();
        // 统一按不透明 RGB 保存：Windows 上 grab() 是否带 alpha 通道取决于
        // 桌面合成（DWM/RDP）状态，会让同一画面产生不同的 PNG 字节。
        QImage img = pm.toImage().convertToFormat(QImage::Format_RGB32);
        img.save(screenshot);
        return 0;
    }
    return exec();
}
