// BesApp.h —— 应用对象（单实例 / 文件关联 / 命令行打开）
#pragma once

#include <QApplication>
#include <memory>

namespace bes { class ComponentHost; }
class MainWindow;

class BesApp : public QApplication {
    Q_OBJECT
public:
    BesApp(int& argc, char** argv);
    ~BesApp() override;

    int Run();

    // 单实例（对应原程序 _BESAPPLICATION_ 互斥体）
    bool EnsureSingleInstance();
    // 文件关联（修复原程序注册表 bug：写空键名导致 "Failed to set data for ''"）
    void RegisterFileAssociation();
    void UnregisterFileAssociation();

    static BesApp* Get();

    bes::ComponentHost& Components() { return *m_components; }
    MainWindow* MainWnd() const { return m_main; }

private:
    void*                 m_mutex = nullptr;   // HANDLE
    bool                  m_registered = false;
    std::unique_ptr<bes::ComponentHost> m_components;
    MainWindow*           m_main = nullptr;
};
