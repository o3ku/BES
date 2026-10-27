# BES (bes.exe) 功能分析与复刻方案

分析对象：`D:\BES\bes.exe` (398,848 B, 2012-07-16)、`BesCom.dll` (2,409,984 B, 2008-05-28)、`bes.cfg` (23,271 B)、`bes.exe.manifest`

分析工具（本次生成，可复用）：
- `tools/dfm_all.py` — Delphi 二进制 DFM 解析器（支持 vaList/vaCollection/vaBinary/vaSet 等值类型）
- `tools/forms.dfm.txt` — 从 BesCom.dll 提取的全部窗体定义（24 个 DFM 资源，18 个业务窗体）
- `tools/bes_cfg_clean.txt` — bes.cfg 的 GBK 解码结果（工具栏/菜单/停靠布局）

---

## 一、结论先行

**`bes.exe` 本身不是主程序，它是一个 30~60 KB 逻辑的「启动器外壳」；真正的 BES 程序全部在 `BesCom.dll` 里。**

运行时实测证据（本机实际启动）：

```
bes.exe 启动后仅加载：bes.exe, ntdll, kernel32, advapi32, user32, gdi32, comctl32, ole32, oleaut32, msvcrt ...
→ 从未加载 BesCom.dll
→ 立即弹出 MessageBox: "Failed to set data for ''"
```

静态证据：

| 证据 | 位置 | 结论 |
|---|---|---|
| 静态导入表只有 7 个系统 DLL，无 BesCom.dll | `bes.exe` PE 导入表 | 不是链接期依赖 |
| 导入 `LoadLibraryA/GetProcAddress/FreeLibrary` | 同上 | 运行期动态加载 |
| `.data` 中的 C 字符串常量块 | `bes.exe` @ `0x49908` | 见下表 |
| 导入表无 Delay-Import 目录 | PE DataDirectory[13]=0 | 非延迟加载 |
| 导出表只有 4 个业务导出 | `BesCom.dll` | 见下 |

`bes.exe` @ `0x49908` 的启动描述符（C 字符串，连续存放）：

```
"bes"            -> 程序名
"*\shell\"       -> 注册表路径
"浏览"           -> 动词 (GBK)
"command"        -> 子键
" \"%1\""        -> 命令行模板
"_BESAPPLICATION_" -> 单实例互斥体名
"TMainForm"      -> 主窗体类名
"bescom.dll"     -> 主程序所在 DLL
"ShowModal"      -> DLL 中的入口函数
```

`BesCom.dll` 的 4 个导出（`tools` 中已用 PE 解析确认）：

| 导出 | 反汇编签名（stdcall） | 语义 |
|---|---|---|
| `RegComNum` | `DWORD RegComNum(int* n)` → `*n = 1; return 0` | 已注册「构件」数量 = 1 |
| `RegComInfo` | `DWORD RegComInfo(DWORD idx, wchar_t name[8], wchar_t desc[6])` | 取第 idx 个构件的名称/说明（UTF-16） |
| `CreateInstance` | 多参（约 5~6 个栈参数） | 实例化构件 |
| `ShowModal` | 多参（约 6 个栈参数） | 创建并模态显示 `TMainForm` |

`ShowModal` / `CreateInstance` **不是无参函数**（`add esp,-0x2c` 后读 `[ebp+8..]`），直接当无参调用会 Access Violation —— 这也是为什么不能简单 `LoadLibrary + GetProcAddress` 就跑起来。

### bes.exe 的职责清单
1. 创建单实例互斥体 `_BESAPPLICATION_`
2. 注册文件关联：`HKCR\*\shell\浏览\command = "bes \"%1\""`
3. 运行期 `LoadLibrary("bescom.dll")` → `GetProcAddress("ShowModal")` → 调用 DLL 里的主窗体
4. 把命令行传入的文件「set data」给主窗体打开

### 报错的成因
`"Failed to set data for '%s'"` 是 Borland C++ Builder RTL 的资源串（与它相邻的还有 `"Failed to create key %s"`、`"Menu '%s' is already being used by another form"`、`"Docked control must have a name"`）。它在**写注册表/INI（文件关联注册）时**因键名/数据名为空而抛出 —— 也就是上面描述符里 `command` 后面那个 `\0\0`（`" \"%1\""` 之后的空串）导致的：**外壳的注册逻辑本身是半成品的/依赖外部配置**。

---

## 二、BES 是什么（业务功能全貌）

> 产品名：**BES = 浏览 / 编辑 / 选取**（Browse / Edit / Select）1.0
> 作者：范晓星（五十七所三处 / 信号分析科），2007
> 定位：面向「信号分析」的**二进制/帧数据浏览器 + 编辑器**，带插件（构件）体系
> 关键 ABI：「分析应用层接口版本 2.0」

### 2.1 窗体清单（从 DFM 资源还原）

| 窗体类 | 用途 |
|---|---|
| `TMainForm` | 主框架：4 条 dxBar 工具栏 + 停靠面板（RootPanel 数据区 / EventPanel 事件区 / GroupPanel 窗口分组）+ 状态栏 |
| `TMDIChild` | 一个数据文档：绘图区 + 水平/垂直滚动条 + 标尺栏 |
| `TSearchStringForm` | 查找独特码（二进制/十六进制/字符 三选一 + 独特码输入） |
| `TSearchFlForm` | 查找帧长（从头 / 从当前 / 从下一个） |
| `TFilterForm` | 帧过滤（帧长过滤：= > < !=；数值过滤：位置 + 开始的 N 字节） |
| `TOutSetForm` | 输出设置（不输出 / 输出到文件 / 输出到端口 1..32） |
| `TParamSetForm` | 参数设置（浏览方式、起点、帧长、步进、显示模式、X/Y 步进、X/Y TAB 宽度、颜色方案、缓存块大小、临时目录） |
| `TPositionForm` | 数据定位（绝对起点 / 相对起点 / 行列 / 书签） |
| `TPrintForm` | 打印（左右上下边距、页面范围） |
| `TComCustomForm` / `TComSelectForm` | 定制构件管理（构件列表 / 构件名称 / 构件程序文件 / 构件信息 / 浏览·添加·删除·修改·退出）与构件选择 |
| `TAgentForm` | `AgentUnit` 里的空窗体（359×235，DFM 无任何子控件），疑为构件宿主/代理占位；**不是**构件管理 |
| `TNetDataForm` | 网络数据输入 |
| `TProcessForm` / `TProgressForm` | 后台任务与进度（开始/暂停/停止/复位/退出、优先级、完成后退出） |
| `TfrmAddGroupItems` | 窗口分组项 |
| `TAboutBox` | 关于（作者、版本、简要说明、构件类型、端口数、端口 ID、数据类型…） |
| `TGlobleData` | 全局数据模块（DataModule）：dxDockingManager + 图标资源 + 全局配置 |

### 2.2 菜单 / 工具栏（`bes.cfg` + 中文字符串还原）

**主菜单**：文件 / 编辑 / 查看 / 处理 / 分析 / 识别 / 定制 / 窗口 / 帮助

| 菜单 | 条目 |
|---|---|
| 文件 | 新建、新建文档、打开、保存、另存为…、打印当前文档、最近打开文件、关闭、全部关闭、退出 |
| 编辑 | 剪切、复制、粘贴、撤销、重做、连续选取、块选取、行选取、列选取、选取参数读入…/存盘…、清除当前选取、清除全部选取 |
| 查看 | 放大、缩小、恢复、界面风格（传统 / VS.NET / Office）、标尺、显示或隐藏横轴分割线、分组横排、分组竖排、窗口分组、显示或隐藏纵轴分割线、工具栏、联动浏览、标题、竖轴显示（YFN/YFL/YFB） |
| 处理 | 分页、横排、竖排、数据格式、参数、迭代、不等长帧查看/处理、定位、独特码查找…、帧长查找…、退出查找状态 |
| 识别 | 二进制、十六进制、文本、位图、过滤（帧过滤） |
| 定制 | 定制构件管理、参数设置、界面风格 |
| 窗口 | 新建窗口、窗口分组、联动浏览 |
| 帮助 | 关于 |

**4 条工具栏**（`bes.cfg` 中 `MainForm.dxBarManager1.Bar0..3`）：

| Bar | Caption | 内容 |
|---|---|---|
| Bar0 | 主菜单 | 9 个子菜单 |
| Bar1 | 常用工具 | New / Open / Save / Print / Cut / Copy / Paste / Undo / Redo / Step / Refresh / Page / Horiz / Vert / PageHoriz / PageVert / Title |
| Bar2 | 格式工具 | Fb/Fl/Step 三个 SpinEdit（起点/帧长/步进）、ViewModeComboBox、Bin/Hex/Text/BMP、Filter、DataNot、DataFormat、Param、Stuff、XTab、YTab、TabLock、YDispMode、ZoomOut/In/Reset |
| Bar3 | 选择和查询 | LockScroll、SetStart、ContinueSelect、RectSelect、RowSelect、ColSelect、选择参数子菜单、Output、SaveAs、SearchFrame、SearchString、QuitSearch、书签组（Set/Next/Pre/Clear）、Goto |

### 2.3 核心数据模型

```
IDataProvider（取数）            IDrawEngine（绘制）
 ├ CPixelDataProvider   ←位图    ├ CPixelDrawer
 ├ CBitDataProvider     ←比特    ├ CBitDrawer
 ├ CHexDataProvider     ←十六进制├ CHexDrawer
 ├ CCharDataProvider    ←字符    ├ CCharDrawer
 ├ CHexCharDataProvider ←十六进制字符 ├ CHexCharDrawer
 └ CNetDataProvider     ←网络数据 └ CNetDrawer
        ▲
   TFileMap（内存映射文件）、PasteWrapper（剪贴板粘贴）
```

其它核心类：`CBESCom` / `CBESWrapper`（构件封装）、`TGlobleData`（全局数据模块）、`CUserInterface` / `IAnalyseLayer_S`（对外「分析应用层」接口 v2.0）、`TMDIChild`（文档/视图）。

### 2.4 业务能力清单

1. **等长帧模型**：起点（比特）、帧长、步进 → 按帧解释数据
2. **不等长帧模型**：前导码/独特码（含误码容限、去相位模糊）、帧长查找、迭代
3. **4 种显示模式**：二进制（位图）、十六进制、文本、位图
4. **浏览**：X/Y 方向步进、X/Y TAB 宽度与显示、标尺、分割线、缩放（放大/缩小/恢复）、倒序/取反、Y 轴显示模式（YFN/YFL/YFB）
5. **选取**：连续 / 块（矩形）/ 行 / 列；**选取参数可存盘/读入**（选区配置文件）；标记（Marker）与书签（Bookmark）
6. **查询**：独特码查找、帧长查找、帧过滤（帧长过滤 + 数值过滤）
7. **编辑**：单步/连续（Step）、设置起点、后台任务（开始/暂停/停止/复位/优先级）、数据编辑（取反关/倒序关）
8. **输出**：输出到文件、输出到端口（1..32 物理端口）、打印
9. **构件（插件）体系**：构件程序文件（*.dll）、构件类型（普通构件 / 局级系统构件 / 数据源构件 / 通用显示构件 / 探头构件）、物理端口数量与 ID、端口数据类型（8/16/32/64 位整型、浮点、字符串、未知）、端口类型（数据流输入/输出、状态输入/输出）、缓存块大小、临时文件目录
10. **自定义**：构件管理、快速执行、界面风格（传统/VS.NET/Office）
11. **窗口组织**：MDI 多文档、窗口分组（拖动项目进行分组）、事件窗格、联动浏览、最近文件

### 2.5 数据类型定义（复刻时的语义基准）

```
比特 / 字节 / 双字节 / 四字节
8位无符号整型 / 8位有符号整型
16位无符号整型 / 16位有符号整型 / 16位有符号整型(摩托罗拉格式)
32位无符号整型 / 32位有符号整型 / 64位有符号整型
32位浮点 / 64位浮点 / 字符串 / 未知数据类型
输出标志：EOI [分块结束标志] / EOF [数据结束标志]
```

---

## 三、方案讨论

### 方案 A：汇编实现 —— ❌ 不可行

- 目标是 ~2.4 MB 的 Delphi 7 + DevExpress GUI（MDI、停靠框架、128 个工具栏项、20+ 窗体、插件宿主）。
- 即使只做核心数据通道，x86 汇编也无法在可接受工期内完成绘制/滚动/选取/搜索。
- 唯一合理的汇编场景是**写补丁代码**（见 B2），不是「用汇编实现功能」。

### 方案 B：二进制修改 / 复用旧程序

**B1. 直接复用 `BesCom.dll`（推荐用于「同等功能」目标）**
- BesCom.dll 自带全部 UI 与业务逻辑，导出 `ShowModal`。
- 只需写一个**新的薄壳**（任意语言：Delphi/C++/C#/Rust/Go），按正确签名调 `ShowModal`；或干脆修好现有 `bes.exe` 的注册表 bug。
- 成本：极低（几百行）；保真度：100%；缺点：仍是 32 位、Delphi 运行时、GBK、DevExpress 老 UI，无法演进。

**B2. 逆向打补丁扩展旧程序（中）**
- 手段：改 `.rsrc` 里的 DFM（加菜单项/改标题）、DLL 注入 hook `ShowModal`/菜单回调、重写 `bes.cfg`。
- 成本：中；风险：DFM/DevExpress 结构耦合深、无源码、Delphi 异常处理、32 位地址假设，任何一次微调都可能让程序静默崩溃；且**不能新增真正的功能**（只能改皮）。

**B3. 只复用数据引擎（`IDataProvider`/`IDrawEngine` 语义），UI 全新写（推荐用于「现代化」目标）**
- 把 BesCom.dll 当作**行为规格 + 测试基准**（黄金参考），重新实现全部 UI。
- 插件 ABI（`RegComNum` / `RegComInfo` / `CreateInstance`）是**公开契约**（「分析应用层接口版本 2.0」），可在新宿主中**原样兼容**，从而继续使用已有的构件插件 `.dll`。
- 成本：中高（工作量集中在本报告第 2 节的功能清单上）；收益：可维护、可扩展、64 位、跨平台、UTF-8。

**B4. 纯二进制逆向重写（不推荐）**
- 把 1.9 MB 的 .text 逐函数反编译回 C++ 再重写，等价于 B3 但多付 3~5 倍成本，且仍然只能得到「和原版一样」的东西。

### 方案 C：重新实现一套

- 目标：功能对齐 BES，外加现代化增强（64 位、UTF-8、多文件/多标签、插件热插拔、脚本化、导出到 HDF5/CSV 等）。
- 推荐技术栈：
  - **Windows 优先**（必须兼容 Windows `.dll` 构件插件）→ C++/Qt 6 或 C#/WPF；
  - 若要跨平台 → Rust + egui/Qt（但 32 位 Windows 构件插件仍需宿主进程隔离）。
- 工程分解建议（按风险从低到高）：
  1. **数据层**：mmap 读文件 + 帧模型（等长/不等长）+ 位/字节寻址 + 取反/倒序 + 类型解码表
  2. **渲染层**：Bit / Hex / Char / Pixel 四种绘制器 + 滚动 + 缩放 + 标尺 + 分割线（对齐原版像素级行为）
  3. **选取/搜索层**：连续/块/行/列 选区模型 + 选区存盘/读入 + 独特码/帧长查找 + 帧过滤
  4. **框架层**：MDI/标签 + 停靠面板 + 工具栏 + 菜单 + 快捷键
  5. **插件层**：构件宿主（`RegComNum/RegComInfo/CreateInstance` 兼容层，32 位 out-of-proc 或 in-proc）+ 端口/数据类型映射
  6. **输出层**：文件/端口输出、打印

---

## 四、建议路线（推荐混合路径）

| 阶段 | 目标 | 做法 | 产出 |
|---|---|---|---|
| **0. 取基准** | 让原件能跑起来，拿到「黄金行为」 | 写一个 32 位宿主按**正确签名**调 `BesCom.dll!ShowModal`；同时修 `bes.exe` 的注册表 bug（空键名） | 可用的原版 + 录屏/截图 + 行为用例集 |
| **1. 壳层复用** | 立刻拥有「同等功能」 | 用现代壳（C#/WPF 或 C++/Qt）代替 `bes.exe`：`LoadLibrary` + `GetProcAddress("ShowModal")`，顺带做好文件关联/单实例 | 1~2 周内可交付的等价品 |
| **2. 核心重写** | 可维护、可扩展 | 按上面 6 个子系统重写；用阶段 0 的用例做回归 | 新一代 BES |
| **3. 插件兼容** | 不让存量构件插件作废 | 复刻 `RegComNum/RegComInfo/CreateInstance` ABI（注意：stdcall、UTF-16 名称/描述、ANSI/GBK 其它内容、32/64 位隔离） | 生态延续 |

### 阶段 0 的具体技术要点（最容易踩坑）
- `BesCom.dll` 需 32 位宿主加载（PowerShell 64 位会 `ERROR_BAD_EXE_FORMAT`）；用 `C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe` 或 32 位 C++ 宿主。
- `ShowModal` 是**多参**函数，先用反汇编确定参数个数与含义（`add esp,-0x2c` + 读 `[ebp+8..]`），否则会 AV。
- DLL 依赖 `bes.cfg`（DevExpress 停靠/工具栏布局），路径按 **exe 所在目录**解析，工作目录必须设为 `D:\BES`。
- 字符集：窗体标题/字符串在 DFM 中是 **GBK**；构件名/描述走 `RegComInfo` 是 **UTF-16**；C++Builder 侧的 `bes.exe` 字符串是 **UTF-16**。

### 阶段 2 的复刻检查清单（对照第 2.4 节逐条打勾）
- [ ] 文件：新建/打开/保存/另存为/打印/最近文件/关闭/全部关闭/退出
- [ ] 等长帧：起点·帧长·步进（位/字节单位切换）
- [ ] 不等长帧：前导码·独特码·误码容限·去相位模糊·迭代
- [ ] 显示：二进制/十六进制/文本/位图 + 缩放 + 标尺 + 分割线 + 倒序/取反 + Y 轴显示模式
- [ ] 选取：连续/块/行/列 + 选区存盘/读入 + 标记/书签
- [ ] 搜索：独特码查找 + 帧长查找 + 帧过滤（帧长/数值）
- [ ] 编辑：单步/连续 + 设置起点 + 后台任务（开始/暂停/停止/复位/优先级/完成后退出）
- [ ] 输出：到文件 / 到端口(1..32) / 打印（边距、范围）
- [ ] 构件：增删改查 .dll、类型、端口数/ID/数据类型/类型、缓存块大小、临时目录
- [ ] 自定义：构件管理、快速执行、界面风格（传统/VS.NET/Office）
- [ ] 窗口：MDI、窗口分组、事件窗格、联动浏览
- [ ] 错误文案与边界行为对齐（例如「帧长×步进大于(2^31-1)，步进将设为默认值1」）

### 不推荐的做法
- ❌ 纯汇编重写
- ❌ 大范围二进制 patch（改 DFM / 注入 hook）：投入产出比极差，且无法加新功能
- ⚠️ 直接用 IDA/Ghidra 把 1.9 MB .text 翻成 C++ 再重写：等价于重新实现但多付数倍成本，只在需要 100% 行为一致时考虑

---

## 附：关键文件/偏移速查

| 目标 | 位置 |
|---|---|
| bes.exe 启动描述符（bes/浏览/command/_BESAPPLICATION_/TMainForm/bescom.dll/ShowModal） | `bes.exe` file `0x49908` |
| bes.exe 的 VCL 资源串（"Failed to set data for '%s'"） | `bes.exe` file `0x51E50`（UTF-16LE） |
| bes.exe 唯一 `TPF0`（图标/初始化表） | `bes.exe` file `0x4A0E0` |
| BesCom.dll 导出表 | `.edata`，共 1261 个导出，业务导出 4 个 |
| `RegComNum` / `RegComInfo` / `CreateInstance` / `ShowModal` | RVA `0x1D90` / `0x1DA4` / `0x1DF0` / `0x1ED0` |
| BesCom.dll 的 24 个 DFM 资源 | 见 `tools/forms.dfm.txt` 的 `======== TPF0 @ 0x... ========` |
| 插件 ABI 相关字符串（`*.dll` 掩码、`RegComNum`…） | `BesCom.dll` file `0x1D3325` / `0x1DA61D` |
| 「浏览编辑选取1.0」「分析应用层接口版本: 2.0」 | `BesCom.dll` 字符串区 |
| 工具栏/菜单/停靠布局 | `bes.cfg`（GBK，Delphi `TWriter` 序列化） |
