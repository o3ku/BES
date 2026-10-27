# BES Qt 重写版 — 构建与说明（仅 64 位）

## 构建

```bat
build-qt.bat         :: 64 位主程序 build/qt-msvc-x64-release/bes.exe
run-tests.bat        :: 构建 + 核心测试 + UI 测试
tools\golden.bat     :: 黄金截图（首次生成 baseline，之后哈希对比）
```

主程序依赖 Qt 5.15.2（`D:\local\Qt5\5.15.2\msvc2019_64`）。
部署：`windeployqt --release bes.exe`。

## 结构

```
src/
  core/    引擎层（纯 C++17，无框架依赖）
  plugin/  构件宿主（64 位 ABI；32 位明确拒绝）
  app/     Qt 应用层（MainWindow / ViewArea（标签条+分隔窗口） / BesDoc / BesView / Actions / 分组/事件）
  dlg/     13 个对话框
  res/     toolbar.png resources.qrc
tests/     test_core / test_ui / test_plugin
tools/     DFM 解析、黄金截图脚本
docs/      原版/64 位操作对比（OPERATION_COMPARISON.md）
```

## 渲染/数据模型（重要）

- **打开默认**：`帧长=64`、`等长帧比特字符`（与原版一致）；标题栏默认显示（原版 `TitleBtn.Down`），为独立开关；`帧长=0` 表示自动（一屏宽一帧）。
  显式设置帧长后，一帧一行；超过屏宽用水平滚动平移。
- **格（Cell）**：
  - 比特模式：1 比特/格；
  - Hex：1 字节/格，绘制 2 个 nibble；
  - HexChar：1 字节/格，绘制 2 nibble + ASCII；
  - Char/Pixel：1 字节/格。
- **位图（原版 mode 7）**：原版格式栏「显示」下拉只有 5 项；`BMPBtn` 的 `OnClick` 虽为
  `DrawContextChanged`，但该函数从不读取它，正常 UI 无法进入 mode 7。mode 7 只能由保存流里的
  4 字节魔数 `0x1234ABCD`（视图构造 `0x403D50`，写入 `0x47C08D`）恢复，且此时下拉被禁用。
  mode 7 painter `0x47C870` 是**字节网格**：`+0x64` = 0 Bin / 1 Hex / 2 Text / 3 BMP；
  BMP 子格式从 `0x47D374` 起画方格（格宽取缩放，50 格一组、每 100 格标号），循环内不按字节值取色，
  所以不是灰度图像。本版下拉恢复原版 5 项；`ViewMode::Pixel`（灰度）只作为「位图」按钮/菜单的
  文档化扩展，不在 combo 中回显（`ComboIndexFromView` 返回 -1）。
- **行/列坐标**：行 = 帧（横排模式为数据流行）；列 = 帧内格列。
  二维块选区的坐标为 `(frame, cell)`，不是比特。
- **`ViewLayout` 有两个用途**：一是主窗体全局的窗口排布（`MainWindow::SetViewLayout`，对应原版
  `[form+0x498]`，见「窗口排布」），二是渲染器的行语义（`params.layout` = 横排/分组横排 时行是连续数据流）；
  排布切换时两者一起写入全部文档，取值 0..4 与原版一一对应。
- **不等长帧**：`DocState::varFrames`，每行取对应帧的 start/len。
- **过滤**：`DocState::visibleFrames` 将行映射到原始帧号。
- **水平偏移**：`Renderer::Layout(..., firstRow, hCellOffset, ...)`；
  `tabFollow` 开启时滚动按 `xTab`/`yTab` 吸附。

## 构件 ABI

原版 ABI（已反汇编确认；本版本仅实现 64 位构件）：

```c
DWORD __stdcall RegComNum(int* count);
DWORD __stdcall RegComInfo(DWORD idx /*1-based*/, char regName[0x6C],
                           char displayName[0x64], int* reserved);
DWORD __stdcall CreateInstance(const char* regName, void** out,
                               void* p2, void* p3);
DWORD __stdcall ShowModal(const char* file);
```

注意：`regName`/`displayName` 是 **ANSI(GBK)**；`idx` 从 1 开始；`CreateInstance` 是 4 参数。
**两个缓冲区角色不同**（由 `ComListBoxClick` / `ComInfoText` 的取用确认）：第 2 参数是注册名
（如 `BESCOM.BESCOM.1`，即 `CreateInstance` 的入参），第 3 参数是显示名（如「浏览编辑选取」）；
构件列表显示显示名，`ComFileEdit` 显示 `文件名: 注册名`。第 4 参数写入但原版从不读取。
本版本只加载 64 位构件。检测到 32 位 DLL 时会拒绝注册并提示
“本版本仅支持 64 位构件”。

`ComponentHost::ShowModal` 按该 ABI 以 ANSI(GBK) 路径调用构件界面
（识别菜单在构件选择后触发）；构件未导出时给出中文提示。

**实例信息块**（构件类型/作者/版本/说明/端口的唯一来源）：`CreateInstance` 成功后经 vtable
槽 18（x64 字节 `0x90`，32 位 `0x48`）先以 `(nullptr, 0, &size)` 取长度、按长度分配后再
`(buf, size, &size)` 填充，槽 1（x64 `0x08`，32 位 `0x04`）释放。块为指针宽度无关的内联布局：
`+0x00` 作者[20]、`+0x14` 版本[10]、`+0x1E` 简要说明[50]、`+0x50` 构件类型（1 基 1..4）、
`+0x54` 端口数、`+0x58` 起每端口 `0x3C` 字节（`+0x28` 端口 ID、`+0x2C` 原始数据类型、
`+0x30` 最大数量、`+0x34` 缺省数量、`+0x38` 端口类型 1..4）。`QueryInstanceInfo()` 按需
创建/读取/释放实例，不缓存；尺寸或端口数越界归 `BadInfo`，创建失败归 `CreateFailed`。
端口值不再来自持久化配置，`plugins/portcfg` 已删除（只持久化 `plugins/dlls`）。

「定制 → 快速执行」（原版 `FastExecuteBtnClick`）用**临时** `ComponentHost`
载入用户选中的 DLL：`LoadLibrary` 后必须同时取到 `RegComNum`/`RegComInfo`/
`CreateInstance`（`HasComponentAbi()`），否则按原版口径统一报
「不是局级系统构件！」；随后枚举 `RegComInfo(1..n)` 弹出构件选择框，
对所选构件 `ShowModal(当前文件)`。该路径不写入持久构件注册表。

## 已完成的 P0/P1/P2

### P0（阻断）
- 默认 64 比特帧长 + 垂直滚动；自动帧长可选；长帧水平滚动。
- Hex/HexChar 双 nibble；HexChar ASCII。
- 水平滚动/TAB 跟随。
- 构件端口表空指针崩溃修复。
- 正确构件 ABI；仅 64 位构件，32 位明确拒绝。
- 另存为路径、打印页范围（原版打印为占位，本版打印为扩展，见「打印」一节）。
- 独特码任意长度、去四相模糊；帧长查找按原版同步列算法（全0/1列）。

### P1
- 不等长帧参与渲染；按独特码自动切分。
- 参数设置：数据格式/显示参数/颜色设置/构件选项四页 + 窗口列表逐窗编辑；9 种颜色可编辑（含默认方案/方案一）。
- 二维块选取、行/列范围、选区统计。
- 过滤结果应用于视图。
- 书签/标记/光标/起点绘制。
- 多视图共享文档与关闭释放。

### P2
- 构件选择、进度/取消、分组项对话框。
- 分组树（新建/删除/关闭组、双击激活、右键 Tab 加入分组）。
- 每条工具栏独立显隐；按钮级定制（右键/双击打开 `DlgCustomize`，增删/排序按钮，格式栏固定控件不受影响），
  栏内容与显隐写入 QSettings；界面风格互斥。
- UI 冒烟测试 + 构件 ABI 测试。
- 黄金截图脚本（`tools/golden.bat`，baseline 在 `tools/golden/`；截图会清空事件时间）。
- 不等长帧 `.bnt` 帧表读入/写出；状态栏三面板（380/400/30）与标记/选取文本；最近文件 8 条。
- 构件 `ShowModal` 调用；输出端口使用构件真实端口号；输出设置按原版改为端口树 + 32 端口单选（树/单选联动，未注册端口禁用）。
- 打印对话框字段取自废弃的 `TPrintForm` DFM（左右/上下边距毫米、单个页面范围、纸张标签、打印设置）+ 本版 DPI 换算；原版 `PrintBtnClick` 只弹「未完待续...」。
- 搜索帧长：范围 3 项 + 检测行数(30)/最少同步列数(2)，逐比特尝试候选帧长，
  统计检测行数内的全0/1列，命中后立即应用到文档。
- 帧过滤按 `BesCom.dll` 中的 `TFilterForm` DFM 重写：三页条件（帧长/数值/CRC）、
  条件列表与添加/更改/删除/清空/应用；原版每条条件为 11 字节记录
  （字节 0 为类型，+1 数值位置，+5 数值类型，+6 判定，+7 数值/帧长），
  多条件相“与”。数值输入按“从低字节到高字节”，解析时最左侧一组为最低字节；
  双字节/四字节回显同样低字节在前。CRC 条件固定校验帧尾最后 2 字节
  （CRC-16/CCITT-FALSE，高字节在前），仅区分通过/失败。
- 格式栏起点/帧长/步进的文本框加一对上下箭头，等效 `TdxBarSpinEdit`：
  按当前显示单位 ±1、0 处钳制；文本仍保留 64 位十进制输入（不换成 QSpinBox，避免 32 位上限）。
- 倒序与参数设置“高位在前(0-7)”是同一个 `highBitFirst` 选项：`DataFormatBtn`
  的提示原文即“选中为高位在前（0-7）未选为低位在前（7-0）”（`bsChecked`），
  `ReverseCheckBox` 同义。关闭时仅在**每字节内**反转比特——比特模式把比特格
  按 7-0 取位、不跨字节；字节模式先反转整字节再做取反（`~`）。不是整帧倒序，
  也不是 nibble 交换。
- 工具栏「单步」（`StepBtn`，`bsChecked`）与分析菜单「迭代」（`DiedaiBtn`）是两个命令：
  单步只切换 `DocState::singleStep`（原版提示“切换单步和连续状态 / 只对数据流输入有效”，
  没有数据流时不动视图）；迭代按原版 `DiedaiBtnClick`（反汇编为单个 `ret`）保留为空占位。
- 参数设置按 `TParamSetForm` 重写：窗口列表逐窗保存 params+frame+fromPort；
  独立启动（原版状态基为 nil）隐藏「构件选项」页，与 `FormCreate` 的 `ComOptionPage.TabVisible=False` 一致；
  确定/取消/应用（应用不关闭并立即生效，确定仅在有修改时应用）；
  顶部显示参数 4 项与显示标尺/X TAB/Y TAB/TAB 跟随；颜色 9 项按原版顺序；
  构件选项缓存 0→1MB、非法→64、上限 1024；块 32..min(缓存×1024,4096)、非法→640；
  帧长上限 90000000、步进≥1（帧长/步进超 INT_MAX 时步进回 1）、TAB≥2。
- 编辑功能：「进行编辑」切换编辑态（文件转为可增长内存副本），剪切/复制/粘贴按字节工作，
  粘贴方式由状态栏「插入/改写」决定，撤销/重做逐操作回滚（Shift+Ctrl+Z），
  保存按原版 500000 字节分块回写、只读文件报「文件不可写！」（详见「编辑接口」）。
- 联动浏览按原版语义：传播「同一移动量」而非绝对位置，范围只到同一停靠容器/标签组的其它视图。
- 窗口排布（分页/横排/竖排/分组横排/分组竖排）按 `[form+0x498]` 实现为全局状态，用 `ViewArea`
  （`QTabBar` + `QSplitter`）重排全部视图；分组排布只同屏当前组（详见「窗口排布」）。

## 测试

```bat
run-tests.bat
```

- `test_core.exe`：189 断言。
- `test_ui.exe`：440 断言（offscreen，已链接图标资源）。
- 测试会隔离 QSettings（`BES_TEST_UI` / `BES_APP_SUFFIX`）。

`test_ui` 覆盖：

- 打开文件、默认帧长/自动帧长垂直滚动、长帧水平滚动；
- 标题独立开关（默认选中、切换不影响布局）；
- 格式栏起点/帧长/步进箭头：按显示单位 ±1、0 处钳制、仍可输入 64 位值；
- 格式栏按钮为图标+文字（`ToolButtonTextUnderIcon`），文字去掉助记符，参数按钮标题“参数”；
- 倒序按钮与“高位在前”同步：可勾选、镜像参数、切换往返；
- 单步/迭代分离：工具栏「单步」可勾选（提示“切换单步和连续状态 / 只对数据流输入有效”，仅记录文档 `singleStep` 模式），迭代保留在分析菜单且为空占位（不移动起点）；
- 状态栏按原版 `dxStatusBar1` 三面板：文件 380px、光标 400px（伸展）、插入/改写 30px；
  文本格式见「状态栏（dxStatusBar1 / CursorInfoLabel）」；编辑态下插入/改写决定粘贴是后移还是覆盖；
- 编辑模式：「进行编辑」按钮选中态、进入后文件变为可增长内存副本、非编辑态剪切/粘贴/撤销禁用、复制/剪切/覆盖粘贴/插入粘贴及其字节结果、撤销/重做可用性往返、退出编辑三种选择（保持按下/保存后退出/不保存退出）、保存只读文件报「文件不可写！」；
- 左侧行号标尺：分页/横排均有（横排 YFN=行号、YFL=行宽、YFB=行首比特）；上/下标尺带 8 比特刻度；
- Hex 双 nibble；
- 参数设置对话框：4 页构建、独立启动时隐藏「构件选项」页（原版状态基为 nil 时 `ComOptionPage.TabVisible=False`）、窗口列表逐窗编辑、从端口/手工单选（无状态端口时禁用从端口）、应用不关闭与数值裁剪、高位在前勾选回读与提交；
- 14 个对话框全部构造（含 `DlgProcess`/`DlgProgress`）；关于对话框标题/产品名/确定按钮；
- 搜索帧长默认参数（30/2）与 64 比特同步列命中；
- 帧过滤条件列表：描述文本、数值低字节在前、CRC 通过/失败、多条件相“与”；
- 64 位构件注册 + 4 参数 `CreateInstance` + `ShowModal`；
- 快速执行：临时宿主枚举选中 DLL 的构件、非构件二进制（`test_ui.exe`）报「不是局级系统构件！」且不注册任何构件、「快速执行」与「刷新」为两个独立命令；
- 32 位构件被明确拒绝；
- 构件端口增删与持久化；输出端口树/32 端口单选联动与真实编号；
- 不等长帧前导码处理、`.bnt` 写出/读入；
- 构件数据流处理对话框（`DlgProcess`）：空闲/运行/停止/复位/退出五态按钮可用性、进度条 0..256 映射与封顶、
  输入选择启用规则（反选/选取参数按选取模式）、数据类型表（含 255 未知）、输出端口决定输出标志/文件名可编辑、
  校验文案、优先级默认值、复位能力开/关两个方向、「开始」经 `FlowEngine` 把选区字节推入数据流输入端口；
  进度对话框（`DlgProgress`）302×86 几何、0..256 刻度、取消置标志；
- 多视图共享文档；
- 联动浏览：范围 = 同一标签组内的其它视图（排除自己），且按「同一移动量」位移各视图自己的位置，
  关闭后不跟随、反向同样带动、开关恢复初值；
- 窗口排布：默认分页只一个视图同屏；横排/竖排切换 `QSplitter` 方向并让全部视图可见；
  分组横排/分组竖排只让当前组的视图可见（无分组时回退为全部）；排布切换把显示模式一并写进全部视图；
  回到分页后其余视图不再同屏；
- 窗口截图。
- 视图右键菜单：与 DFM 隐藏栏「快捷操作」逐项一致（含分隔符与「选取参数」子菜单），书签四个按钮分别为
  切换/下一/上一/清除，且快捷栏的「清除标签」不再误接「清除当前选取」。
- 工具栏定制：默认布局 = DFM `ItemLinks`；逐栏替换按钮不影响其它栏与命令本身；分隔符唯一、
  格式栏固定控件存活；单栏显隐与窗口菜单勾选同步；右键菜单三条栏 + 「自定义...」；
  `DlgCustomize` 标题/两页/控件、可用命令列表、添加/上移/删除、YDisp 占位锁定、Reset、
  勾选显隐立即生效、关闭；布局与显隐键持久化并在下次启动恢复。
- 窗口分组拖放：`GroupPane` 可拖单个窗口项，MIME 往返/非法数据拒绝；移入空组、重复拖入去重、
  跨组移动（一个窗口只属于一个组，对话框新建分组时勾选窗口同样会移出原组）、同组两窗在分组横排下同屏、
  移出后可见集缩小、拖回「数据窗口」移出全部自定义组、空组回退全部视图；真实 `DragEnter`+`Drop` 事件路径
  （`QApplication::notify` 只把 `Drop` 交给 `QDragManager::currentTarget()`，合成事件必须先接受 `DragEnter`）。
- offscreen 平台没有字体，`test_ui` 抓图里不出现任何文字，只能断言底色/几何；文字与标尺标注的效果以黄金截图（真实 Windows 平台）为准。

## 黄金截图

```bat
tools\golden.bat
```

- 生成 `build\golden_sample.dat`；
- 用 `BES_APP_SUFFIX=_golden` 隔离设置；
- 输出 `tools\golden\current.png`；
- 首次生成 `baseline.png`，之后比较 SHA-256。
- 截图模式下会清空事件面板（去掉时间戳），保证哈希稳定。
- 保存前统一 `convertToFormat(QImage::Format_RGB32)`：Windows 上 `grab()` 是否带 alpha 随桌面合成（DWM/RDP）状态变化，RGBA 编码会让同一画面产生不同字节。
- 抓取前把应用调色板的 Active 组复制进 Inactive 组：进程没拿到前台焦点时 Qt 认为窗口 Inactive，样式按 `QStyle::State_Active` 改用 Inactive 组，菜单文字会变成灰 `(160,160,160)`，同一画面会有两种哈希。`QApplication::setActiveWindow()` 与 `QPalette::setCurrentColorGroup()` 都不起作用（前者被平台的激活状态覆盖，后者会被样式重新判定），只有把 Inactive 组本身改成与 Active 一致才稳定。

## `.slp` 选取参数文件格式（反汇编确认）

原版 `SelectLoadSaveBtnClick@0x40AD58` → 类的 `Load@0x440624` / `Save@0x4407AC`（VMT@0x5DDBF0 的 +04/+08），`TFileStream` 读写。
这两个方法本身与选区语义无关：对象的 `+4/+8/+0xC/+10` 是四个 `TList`，按字段地址顺序写出「Int32 记录数 + 记录数 × 24 字节」；**段的含义由消费方决定**：

```text
's' 'l' 'p'                      ; 3 个裸 ASCII 字节，无长度前缀、无版本号
Int32 n0; n0 × {Int64,Int32,Int64,Int32}   ; 段 0 连续选取（+4  列表）
Int32 n1; n1 × 同上 24 字节记录            ; 段 1 块选取  （+8  列表，两个「行+列」角标）
Int32 n2; n2 × 同上                        ; 段 2 行选取  （+0xC 列表，行号 64 位闭区间）
Int32 n3; n3 × 同上                        ; 段 3 列选取  （+0x10 列表，Int32 起+Int32 止闭区间）
```

- 段序证据链：`0x444924` 把 24 字节记录投递给选区对象时按 `[[self+0x18]+0]`（选取模式）分支 —— 模式 0 → `[self+4]`（并 `Sort`，比较器 `0x4033AC`）、1 → `[self+8]`、2 → `[self+0xC]`、3 → `[self+0x10]`。而「模式」即工具栏按钮的 `Tag`：`SelectBtnClick@0x40AC88` 读 `[Sender+0x83]`（Down 态）后把 `[Sender+0xC]`（Tag；未按下传 `-1`）写入选区对象的模式字段。`forms.dfm.txt` 中 `ContinueSelectBtn`（无 Tag ⇒ 0）、`RectSelectBtn Tag=1`、`RowSelectBtn Tag=2`、`ColSelectBtn Tag=3` 共用 `OnClick=SelectBtnClick` + `GroupIndex=2` + `bsChecked` + `AllowAllUp` ⇒ **段序 = 连续 / 块 / 行 / 列**。
- 各模式的记录字段规则（`0x444924` 的记录构造）：模式 0 逐字拷贝 p1/c1/p2/c2（连续段的两个 Int32 是随选区携带的待定参数，本模型写 0）；模式 1 对两组「行+列」分别取 min/max 归一化；模式 2 只用 Int64（min@+0、max@+0xC），两个 Int32 恒为 0；模式 3 只用 Int32（min@+8、max@+0x14），两个 Int64 恒为 0（`[self+0x68]=-1` 时按最后一列收尾）。
- 原版的行/列列表存的是**区间**（`0x444924` 在 mode 2/3 会合并相邻/重叠区间、必要时拆分）；本模型每行写一条 `row1=row2` 记录、读侧统一按 min..max 展开，因此两个方向都能与原程序互换。
- 读侧逐字节比较 `s/l/p`，不符弹「不是选取参数文件！」（`cmp 0x73/0x6c/0x70`@0x4406AA-0x4406CB）。
- `'BES'`/`'BESFORMAT'` **不属于** `.slp`：它们由 `[self+0x20]` 的 profile 代码用于 `bes.cfg`（`0x44117C` 等）。
- 书签/标记/聚焦/距离只出现在事件窗文本（`0x442754`/`0x443DE0`/`0x47F790`），不写入 `.slp`。
- 实现：`SelectionSet::SaveSlp()/LoadSlp()`；旧文本格式 `Serialize/Deserialize` 因 `legacy_mfc` 仍引用而保留。

## 选取交互（反汇编确认）

选取对象挂在子窗口 `+0x1E4`，按显示模式重建（`0x403F10(child, mode)`，模式 1/比特字符为 VMT `0x5DDBDC`）。关键字段：

```text
+04/+08/+0C/+10  四个 TList：连续 / 块 / 行 / 列
+14  渲染器（child+0x110）      +18  选区状态记录（child+0x180）
+1C  文档/布局对象（child+0x1E0） +20  状态标签
+38  8（每格比特）              +3C  0x10
+50/+51/+52  标志（0x51 = 拖动中，0x52 = 自动滚动中）
+54/+58  锚点比特（Int64）      +5C  锚点列
+60/+64  当前端点比特（Int64）  +68  当前列
```

- 按下 `MouseDown@0x443840`：只接受左键（Delphi `mbLeft = 0`）；把坐标钳进客户区后换算成比特，锚点与当前端点都置为该比特、`+0x51 = 1`。
- 移动 `MouseMove@0x443DE0`：仅更新 `+0x60..+0x68` 并刷新绘制；**不写四个 TList**。鼠标拖出客户区时置 `+0x52` 并自动滚动（越界距离越大 `Sleep` 越长）。
- 松开 `MouseUp@0x444924`：`+0x51` 已置位时构造一条 24 字节记录并追加到 `[state+0]` 模式对应的列表；模式 0（连续）`TList.Add` 后 `Sort`（比较器 `0x4033AC` 按记录首 Int64 升序，相等再比 +8 处的 Int32），模式 1 直接追加，模式 2/3 合并相邻/重叠区间。**因此默认就是多次拖动累积，鼠标处理里没有任何 `GetAsyncKeyState` 调用，Ctrl 不参与。**
- 模式按钮 `SelectBtnClick@0x40AC88`：读按钮 `Down`（`+0x83`），按下时把 Tag（连续 0 / 块 1 / 行 2 / 列 3）写入模式，未按下写 `-1`；四按钮 `bsChecked` + `GroupIndex=2` + `AllowAllUp`，DFM 均无 `Down = True` ⇒ 初始模式 `-1`。`SetSelectMode@0x43FFBC` 切换时调清列表核心 `0x4404CC(dl=0)`，只清**上一模式**的列表；再次点击当前按钮则 `0x4404CC(dl=1)` 清全部四个列表并把模式置 `-1`。
- `[child+0x198]`（`state+0x18`）只在粘贴路径 `0x44135F` 置 1：下一次按下先清当前选区；与累积语义无关。
- 键盘：`KeyDown@0x445B0C` 的跳表把 VK_SHIFT(0x10) 映射为「置 `+0x51`、当前端点 = -1」，方向键/PgUp/PgDn/Home/End 在 Shift 按下时移动当前端点；`KeyUp@0x4477DC` 收到 VK_SHIFT 松开时直接调用 MouseUp 的提交逻辑（`vmt+0x60 = 0x444924`），Esc 则从列表里删除与当前锚点/端点匹配的记录。端点是**行/列分量**：Up/Down 行 ±1（保持列）、Left/Right 列 ±1（保持行）、Home=行首、End=行尾、PgUp/PgDn ±可见行数；首次按键从锚点走一步（Shift 按下时把当前端点置 -1）。
- 落点：`BesView::keyPressEvent/keyReleaseEvent` 实现上述移动与提交，`BeginKeyboardSelection/MoveKeyboardEndpoint/EndKeyboardSelection`；Esc 在本版是**取消未提交的预览**（不删除已提交记录）。拖出客户区自动滚动：原版 `MouseMove@0x443DE0` 用 `PtInRect`（`0x443F5C`）判断并置 `+0x52`、启动定时器，越界越远 `Sleep` 越长；本版用 60ms `QTimer`（`UpdateAutoScroll/OnAutoScrollTick`）按行/列滚动，并把鼠标位置钳进视口后重新命中网格，端点跟到屏幕边缘格。
- 无 Shift 的普通键走 `KeyDown` 尾段 `0x44611D`：要求 `state+0x18 == 0`、Shift 未按下且参数位为 0；移动光标 `+0x54/+0x58`（比特）与 `+0x5c`（列），随后调 `0x4404CC(dl=0)` 只清当前模式列表。跳表之后的落点：Up/Down 行 ±1（保持列并钳到有效行）、Left/Right 列 ±1 并**折返到相邻行**（行首再左 = 上一行末列，行尾再右 = 下一行首列）、Home = 行首、End = 行尾、PgUp/PgDn ±可见行数。本版落点 `BesView::MoveCaret`：光标比特按显示行×格列换算后写回 `BesDoc`，先 `ClearMode(st.sel.Mode())` 再 `EnsureBitVisible`，不再由方向键驱动滚动条。

实现：`SelectionSet` 的连续段是 `std::vector<ContinuousSel>`（`AddContinuous` 排序插入），Renderer / OutputWriter / FlowEngine / 状态栏 / `.slp` 全部按多段遍历；`BesView` 用 `m_anchorBit/m_dragBit` 造预览副本（`BuildPreview`），`mouseReleaseEvent` 才 `CommitSelection`；`BesDoc::SetSelectMode` 实现「切换只清上一模式 / 同模式再点清空退出」，四个命令为 checkable，`UpdateMenuState` 同步勾选。

- **单击反选（本版补充手势）**：原版行/列分支只做区间并集合并（`0x444924`），Esc 在 `0x445B0C` 跳到 `0x44611D` 空操作，取消只能再点模式按钮（`AllowAllUp`）或清除命令。本版保留这些路径，并让「单击（无拖动）已选中的行/列」调 `SelectionSet::ToggleRow/ToggleCol` 取消该行/列（`CommitSelection` 在起止帧/列相同时走 toggle），拖动跨多行/列仍 `AddRowRange/AddColRange` 累加。
- **标尺数字避让**：刻度线仍在每个 8 比特边界绘制，但数字按 `QFontMetrics::horizontalAdvance` 的实际字宽排版，与上一个数字右边界 + 6px 相交则跳过。字节/十六进制模式每格都是 8 比特边界，此前固定 4 格宽的标签矩形会逐格压字；比特模式每 8 格一个数字，故不受影响。

## 编辑接口（反汇编确认）

主窗体按钮（`BesCom.dll`）：`EditBtnClick@0x40B724`、`SaveBtnClick@0x40C888`、
`UndoBtnClick@0x40B89C`、`RedoBtnClick@0x40B8BC`、`CutBtnClick@0x40B8DC`、
`CopyBtnClick@0x40B8FC`、`PasteBtnClick@0x40B91C`，均转发到引擎对象的 `editIface` 虚表：

| 槽位 | 含义 |
|---|---|
| +0x24 | 进入/退出编辑态（`EditBtnClick` 唯一动作） |
| +0x28 / +0x2C / +0x30 | 剪切 / 复制 / 粘贴 |
| +0x34 / +0x38 | 撤销 / 重做 |

引擎侧标志位：脏标志 `[engine+0x1a4]`、后台任务标志 `[engine+0xc]`、插入/改写 `[engine+0x1c]`。
`SaveModified@0x40B93C` 用 `GetMem 0x7A120`（=500000 字节）缓冲**按 500000 字节分块**原位回写，
文件不可写时报「文件不可写！」（`0x5D3C6A`）。退出编辑且脏标志为真时弹
「文件内容已改变，保存吗？」（`0x5D3C32`，Yes/No/Cancel）；`EditBtnClick` 本身不做前置校验，
关闭/保存路径也不询问。状态栏右侧 30px 面板显示 `插入`/`改写`（`0x5D3BA8`/`0x5D3BAD`），
DFM 中重做快捷键为 Shift+Ctrl+Z。

实现落点：`FileMap::OpenEditable`（可增长内存副本，保存时才回写磁盘）、`BinaryEditor`
（Insert/Remove/Overwrite + 逐操作撤销栈）、`BesDoc::EnterEdit/LeaveEdit/SaveEdits`、
`MainWindow::ToggleEditMode/DoCut/DoCopy/DoPaste/DoUndo/DoRedo`。
「进行编辑」按原版是**无图标文字按钮**（`bsChecked`），排在选取参数按钮组之后。

## RTTI 字段表：把 `[form+0x45c]` 还原成控件名

本 DLL 的 VMT 偏移与 Delphi 7 文档不同，实测：`-76` selfptr、`-72` intf、`-68` auto、`-64` init、
`-60` 类型信息、**`-56` 字段表**、**`-52` 方法表**、`-48` 动态方法、**`-44` 类名**、`-40` 实例大小、`-36` 父类。
`TMainForm` VMT=0x5D5710、实例大小 0x4C8、字段表 0x5D57F4、方法表 0x5D5FF9。
字段表格式：`{Word 数量=112; Int32 属性表指针; 数量 × {Int32 Offset; Int16 类型索引; Byte 名字长度; 名字}}`，
首条记录自 `ft+6` 起，112 条正好结束于方法表地址（用它校验解析没有错位）。据此得到
`+0x2D0 OpenDialog`、`+0x2D4 dxBarManager1`、`+0x33C DataFormatBtn`、`+0x360..0x370 Cut/Copy/Paste/Undo/RedoBtn`、
`+0x3E8 EditBtn`、**`+0x45C ViewModeComboBox`**（`+0x494`/`+0x4A0` 是非 published 的私有字段，不在表内）。

`ViewModeComboBox` 是格式栏的 `TdxBarCombo`：DFM `Caption = "显示"`、`Hint = "显示模式"`、设计期
`Enabled = False`、`OnChange = DrawContextChanged`、栏内 `UserWidth = 114`。`EditBtnClick@0x40B724` 进入编辑
时对布尔 Enabled 设置器 `0x509AA4`（写 `[self+0x44]`）传 `dl=0`，每条成功退出路径传 `dl=1`；退出被拒时用
`0x50AA0C` 把 `EditBtn` 重新按下并保持禁用。`0x40A3D8`（把配置记录回填到界面：`esi := 记录 + 0x180`，入口处先置 `[form+0x49e] = 1` 作回填中标记）另有
`[form+0x45c].Enabled := ([esi+0x18] = 0)` 的再断言，同一函数里 `[form+0x334]/0x338/0x340` 等控件也按记录字段设 Enabled，
说明这是原版集中的“状态→控件可用性”同步点。
现版把该控件的 Enabled 收敛在 `MainWindow::UpdateMenuState()` 里：`m_cboViewMode->setEnabled(has && !editing)`，
避免在 `ToggleEditMode`/`LeaveEditMode` 各分支重复设置；`QComboBox::setEnabled(false)` 后程序化 `setCurrentIndex`
仍然有效（且该处有 `QSignalBlocker`），不影响回填。

`SaveModified@0x40B93C` 的三条失败提示为「文件不可写！」「后台任务运行时不能保存文件！」「目标磁盘空间不够！」；
后两条分别依赖 `[engine+0xc]` 后台任务标志和短写判定，现版路径上不可达，只作记录。

明确未复刻的一点：

- `EditKeyPress@0x40FFF0` 只是 `FilePosEdit`/`RowEdit`/`BufSizeEdit` 的十六进制字符过滤，
  **没有**在数据区内直接键入改字节的证据，因此不发明该交互；改字节走剪贴板 + 撤销/重做。

## 联动浏览（反汇编确认）

开关与状态：

- 控件是 `LockScrollBtn`（TMainForm 字段表 `+0x3C8`，字段表约定见上一节），DFM `Caption` 是「联动」、
  `Hint` 是「联动浏览」（按钮说明取 Hint），`Style = bsChecked`、快捷键约 `Ctrl+A`，**没有 OnClick**：`bsChecked` 自己翻转
  `Down`，因此读状态就是读 `[obj+0x83]`（与选取按钮 `SelectBtnClick@0x40AC88` 读 `Sender+0x83` 同一偏移）。
- 收集器 `TMainForm::0x407B4C(当前视图, outList)`：`outList.Clear()` → 若 `[[form+0x3C8]+0x83] = 0` 直接返回
  → `site := 当前视图.[+0x24].vtbl[+0x108]()`（视图所属停靠站点），要求 `is TdxSideContainerDockSite`
  → 遍历 items 取 `item.[+0xC]`（视图）加入，跳过 nil 和触发者。
  ⇒ **联动范围 = 同一停靠容器内的其它视图**，不含自己、不跨容器；`0x407AF8` 那个在
  `[form+0x2F4]`(GroupTreeView)`. [+0x254]` 里按 `item.[+0xC]` 比对的辅助遍历只用于定位视图，不扩大范围。

传播的是「移动量」，不是绝对位置：

- 三个触发点：滚轮 `0x405C34`（行数 = `-WheelDelta div 120`）、滚动条通知 `0x4061C6`
  （按 `TScrollCode` 0..5 换算；源视图基准 `rec+0x160`、其它视图基准 `rec+0x15C`）、跳转表 `0x4064A9`（±1/3/5）。
- 每个被联动视图执行「自己的位置 + 同一移动量」，Int64 加 `0x5C90B0`、存 `0x56EF8C`，
  写回 `rec.[+0x2E0].[+0x1E4].vtbl[+8]`。所以两个视图可以从不同位置出发、保持固定视差。

Qt 侧落点：本版本只有一个 `ViewArea` 作为中心控件（`GroupPane` 只是导航树），
「同一停靠容器」等价于「同一标签组内的其它 Tab」。`BesView::OnVScroll/OnHScroll` 记录前值、
把差值交给 `BesView::LinkShift(dRow, dCol)`；`LinkShift` 在 `MainWindow::LinkBrowse()` 为真时
遍历 `centralWidget()`（`ViewArea`）的视图、跳过自己，把增量加到各视图自己的 `ScrollRow()/HOffset()`（钳到 ≥0），
经 `SetScrollRowQuiet/SetHOffsetQuiet` 静默写入。原来广播绝对位置的 `SetScrollPosQuiet` 已删除。

## 窗口排布（反汇编确认）

分页 / 横排 / 竖排 / 分组横排 / 分组竖排是**主窗体的全局状态**，不是某个视图的绘制方式：

| 命令 | 处理函数 | 写入 `[form+0x498]` | 重排动作 |
|---|---|---|---|
| 分页 | `PageBtnClick@0x408B20` | 0（`xor edx,edx` 后 `mov [ebx+0x498],edx`） | 不重新插入，只做刷新（`0x407DFC/0x408A78/0x5442B4/0x5573CC/0x407F38`） |
| 横排 | `HorizBtnClick@0x408B94` | 1 | 每个站点：`edx := [item+0xC]`，`0x544BC8(eax=控件, edx=[form+0x2E4], cl=3, -1)` |
| 竖排 | `VertBtnClick@0x408C24` | 2 | 同上，`cl=2` |
| 分组横排 | `PageHorizBtnClick@0x408CB4` | 3 | 组内首项 `cl=1` 插进根面板，其后各项相对前一项 `cl=5` 依次插入 |
| 分组竖排 | `PageVertBtnClick@0x408E2C` | 4 | 组内首项 `cl=1`，其后各项相对前一项 `cl=4` 依次插入 |

- 站点集合来自 `[form+0x2F4]`(GroupTreeView)`.+0x254`：`0x426F54` 取数量、`0x4273BC` 按索引取项、
  `item.+0xC` 是该站点承载的视图控件；分组变体先用 `0x426BB0`（子项数）与 `0x4265A0`（是否单项）分流，
  多子项时用 `0x4267D8(item, k)` 取第 k 个子项。`cl` 即 Delphi `TAlign`（None=0/Left=1/Top=2/Client=3/Bottom=4/Right=5）。
- 五个处理函数都**遍历全部站点**，因此排布作用于所有视图；`[form+0x498]` 只有一份，切页/切文档不会改变排布。
- 分组变体只有「重插站点 + 刷新」，**不写视图参数**：XTAB/YTAB 的开关与线宽保持用户设置，组间边界由停靠站点/
  分隔条自身边框呈现。Qt 早期曾在组模式下强制打开 XTAB/YTAB 并加粗到 2px，已按此删除。

Qt 侧落点：新增 `src/app/ViewArea.h/.cpp` 取代原来的 `QTabWidget` 中心控件——`QTabBar` + `QSplitter`
（手柄 4px、子视图不可折叠）。`Paged` 时把未选中的视图 `setVisible(false)` 但**留在分隔窗口里**
（移出 splitter 会丢几何，联动浏览的位移基准就错了）；其余四种按 `shownViews` 决定可见集合并设置分隔方向
（竖排/分组竖排 = 纵向，横排/分组横排 = 横向）。`MainWindow::SetViewLayout(bes::ViewLayout)` 是全局入口
（对应 `[form+0x498]`），一次写入所有文档的 `params.layout` 并 `ApplyArrangement()`；
`GroupH/GroupV` 传入的 `shownViews` 取当前 `currentIndex()` 所在组，空组回退为全部视图。
`bes::ViewLayout` 的枚举值刻意与 `[form+0x498]` 一一对应（0..4），`test_core` 有断言钉住。
**本版本扩展**：横排/分组横排时渲染器把行当作连续数据流（原版排布只重排窗口、不改字节流），见
`FEATURE_COMPARISON.md` 的「明确剩余差距」。

## 构件数据流处理（反汇编确认）

RTTI：`TProcessForm` VMT `0x5D86A0`、实例大小 `0x3A4`、字段表 `0x5D8784`、方法表 `0x5D896F`。
DFM：客户区 434×448、`Position = poScreenCenter`、`KeyPreview`、`OnCreate/OnClose` 齐全，
一条 `TSplitter`（`Top = 226`）把窗口分成上半的三行参数（数据输入 / 数据类型 / 数据输出）
和下半的 `TdxInspector`（自定义参数、自定义显示；现版用两个分组框 + 「点击按钮打开...」按钮代替）。
「识别 → 数据流输入 / 数据格式输入 / 数据流输出」三条命令共用这一个窗口，
所以现版 `CMD_IDS_DATA_IN/CMD_IDS_FORMAT_IN/CMD_IDS_DATA_OUT` 都走 `MainWindow::DoProcessFlow()`
（构件选择 `DlgComSelect` → `DlgProcess`），而不是三个不同对话框。

FormCreate（`0x413A8F`/`0x413C7B`/`0x413AF7`）设的默认值：输入选择 = 全部数据、优先级下拉默认第 4 项（值 0）、
自动退出/自动打开均为选中、进度条 `Min=0 / Max=256`。优先级表在 `0x5D7600`：`{-15, -2, -1, 0, 1}`。

输入选择行的规则（`SelectModeRowChange`）：反选只在「全部数据 / 块选取 / 列选取（且构件允许）」时可用，
其余模式强制弹起；选取参数在「全部数据」下写死为“全部数据”且可编辑，非全部数据下取段名
（连续/块/行/列，来自 `[form+0x340]` 的选取列表，即上一节 `.slp` 的段序），只有构件接受手工输入时可改。
数据类型行 16 项：比特(7..0)、整型/浮点/字符串各形态，末尾「未知数据类型」= 255。

按钮状态机（地址即 handler）：

- 空闲 `0x415650`：仅「开始」可用，暂停/停止/复位禁用，进度条隐藏。
- 开始 `0x416198` 尾部：禁用「开始」，启用「暂停/停止」，「复位」按构件能力 `[form+0x344]` 决定。
- 暂停 `0x4161F8`：读按钮 `Down`（同 `+0x83`），Down→命令 3（暂停）、弹起→命令 4（继续）。
- 停止 `0x416250`：暂停/停止/复位禁用，只清运行标志（进度保留，窗口不关）。
- 复位 `0x41629C`：向构件发命令 5，界面回到空闲；**不关闭窗口**。
- 退出 `0x4162AC`（`FormClose` 也走这里）：四个按钮全禁 + 清运行标志 + 置退出请求。

校验的两条字面量（`0x415C15..0x415C70`、`0x415D8E` 起）：非全部数据且选取参数为空 →「选取参数不能为空！」；
有输出端口却拿不到文件名 →「: 文件名不能为空！」，两者都写进 `InfoLabel` 并置红字后中止。

现版落点：`src/dlg/DlgProcess.{h,cpp}` 把上述行集/默认值/状态机/校验/优先级原样实现，
构件能力（是否接受手工输入、是否有数据输出端口、是否支持复位、列选取是否可反选）
以 `setManualInputAllowed/setOutputPort/setResetSupported/setDeselectForCol` 四个开关传入。
其中「列选取可反选」的开关保留但 `MainWindow` 从不打开：原版条件 `[[ctx+0x74]+4] == 7`
读的是另一个上下文对象的字段，与本 ABI 的 1 基构件类型（1..4）无关。
`TProgressForm` → `DlgProgress`：302×86、`QProgressBar` 范围 0..256、标签 `9,22,201,13`、
取消按钮 `217,47,75,23` 且为默认按钮，`closeEvent` 置取消标志（等价原 `FormClose`）。

数据推送落在新增的 `src/core/FlowEngine.{h,cpp}`：原版把搬运放在主窗体持有的上下文对象里
（`[form+0x360]`，`+0x14` 线程句柄 / `+0x18` 退出请求 / `+0x19` 运行标志 / `+0x8c` 块上限 `0x10000`，
`@0x41067B`/`@0x410689` 交接），窗体只置标志；暂停/继续是 `SuspendThread`/`ResumeThread` 加构件命令 3/4，
复位是命令 5，退出置 `[ctx+0x18]`。构件侧按 vtable 槽调用：命令 `+0x28`、优先级 `+0x30`（`@0x4165E0`）、
自定义参数 `+0x18`（`@0x41714A`/`@0x4174A9`）、自定义显示 `+0x34`（`@0x41660E`、`@0x416133`）。
`BesCom.dll` 只导出 4 个符号（串在文件 `0x1D3325`/`0x1DA61D`，`.edata` 在 `0x206112`），上下文/记录引擎不在其中，
记录的二进制布局无从验证，故不虚构该 ABI：`bes::IFlowPort` 由 `MainWindow::FlowInputFile` 以临时文件承载
（与输出侧 `TempFilePort` 对称），`bes::FlowEngine` 按 `FlowParams` 把连续/块/行/列选区（含反选补集）换算为字节区间
（与 `OutputWriter::emit` 同一换算）、按 `[ctx+0x8c]` 切块推送。本版本无工作线程，改为单线程 + 块间回调泵事件，
`Pause/Resume/Stop/Reset/RequestExit` 与命令 3/4/5、`[ctx+0x18]` 一一对应，因此暂停/停止/退出依然即时生效。
提示文案：「数据长度为0！」、「没有可用的数据流输入端口！」（客户端对端口数量/类型/最大数量的前置校验
已在构件选择时按原版拦截，见「定制构件管理」）、「端口输入失败！」。

命令 100 的分派层已按反汇编补齐：调用点是 `TProcessForm.SetComponent`（`0x414378`，
实例写入 `[form+0x364]`）里的 `0x4154D3`——取 `[form+0x334]`（DFM 的 `TPanel ContainerPanel`，
`Align = alBottom`、`OnResize = ContainerPanelResize`）客户区到 16 字节 `TRect`，再
`push 0x10; push &rect; push 0x64; push com; call [vtbl+0x34]`。槽签名是
`int (WINAPI*)(void* self, int cmd, void* arg, int argSize)`；返回 `-100`（`0xFFFFFF9C`）表示
构件不提供自定义视图，宿主转 `0x4155F8` 隐藏 `ContainerPanel`，否则隐藏默认窗格并重排。
`CreateInstance` 的第 4 个参数就是这个面板的 `Handle`（`0x4180C0..D0`，`TWinControl.Handle` 在 `+0x14C`）。
落点：`bes::ViewRect` + `ComponentHost::SendCommand()`（vtable 索引 13，x64 字节 `0x68`，
`__try/__except` 保护）与 `SetViewRect()`；`DestroyInstance()` 现在会真正调槽 1 释放，
析构函数在 `FreeLibrary` 前释放全部存活实例。`tests/test_plugin.cpp` 在槽 13 记录命令并导出
`TestPluginStats`。**仍未接**的是窗口托管本身：现版 `DlgProcess` 没有 `ContainerPanel`，
托管需要把真实容器 HWND 交给构件，而仓库里没有原版 64 位构件可验证，故不虚构。

## 定制构件管理（`TComCustomForm` / `TComSelectForm`，反汇编确认）

RTTI（本 DLL 的 VMT 偏移：类名 `-44`、实例大小 `-40`、父类 `-36`、字段表 `-56`、方法表 `-52`、自指 `-76`，
用 `vmtSelfPtr == VMT` 校验）与 DFM（`ComCustomForm: TComCustomForm` = 资源 `TPF0 @ 0x213EAC`，
`Caption = 定制构件管理`、`BorderStyle = bsSingle`、`BorderIcons = [biSystemMenu]`、客户区 468×312）给出的发布字段：

| 偏移 | 对象名 | 几何 / 属性 |
|---|---|---|
| `+0x2D0` | `ComListBox` | 11,13,136,285，`ItemHeight = 16` |
| `+0x2D4` | `GroupBox1` | 153,11,308,257，标题「构件属性」（组内坐标以客户区为原点，标题占约 14px） |
| `+0x2DC` | `ComNameEdit` | 8,39,216,20，`MaxLength = 32`，窗体里唯一可编辑项 |
| `+0x2E4` | `ComFileEdit` | 8,83,266,20，`ReadOnly` |
| `+0x2EC` | `ComInfoMemo` | 8,125,289,124，`ReadOnly` |
| `+0x2F0` | `BrowseBtn` | 275,83,26,21，标题「…」 |
| `+0x2F4`…`+0x300` | `AddBtn`/`DelBtn`/`QuitBtn`/`EditBtn` | 添加 151,275｜删除 229,275｜修改 308,275｜退出 386,275，前三项初始禁用 |
| `+0x304` | `OpenDialog` | `Title = 选择构件程序文件` |
| `+0x308/+0x30C` | 私有 | 待「添加」的源路径 / `com\` 目标路径 |
| `+0x310` | 私有 | 字节标志 = `(info.comType == 2)`「数据源构件」；`OKBtnClick` 写、`0x40D6B0` 读，现版以 `InstanceInfo::comType` 等价表达 |

处理函数：入口 `MainComManageClick@0x40D3A0`；`FormCreate@0x436470`（列表非空即选中第 0 项并调一次
`ComListBoxClick@0x436568`）；`BrowseBtnClick@0x4349C8`；`AddBtnClick@0x435EC0`；`DelBtnClick@0x436A3C`；
`EditBtnClick@0x436B80`；`ComInfoText@0x435120`。

`ComFileEdit` 的文本 = `ExtractFileName(路径)`(`0x5BB254`) + `": "` + **注册名**，用 `_LStrCat3@0x5D0AD4`
（`eax=Source1, edx=Source2, ecx=Dest`）拼接；注册名是 `RegComInfo` 的第 2 缓冲区（`CreateInstance` 的入参），
不是完整路径，也不是显示名。

`ComInfoText` 的标签是镜像里 `0x5DB0BD..0x5DB12B` 连续存放的常量串（自带冒号与空格）：`作者: `、`版本: `、
`简要说明: `、`构件类型: `、`物理端口数量: `，其后逐端口输出 `物理端口 ID:`（该标签无尾随空格）、`端口数据类型: `、
`端口类型: `、`缺省端口数量: `、`最大端口数量: `；数据类型按原始整数输出，缺省数量在最大数量之前，
每行以换行结束、端口数量行后多一个空行。这些串查不到直接 xref，是因为它们经 `0x5DAFF8`（构件类型 4 项：普通构件/
数据源构件/通用显示构件/探头构件）与 `0x5DB008`（端口类型 4 项：数据流输入/数据流输出/状态输入/状态输出）
两张 4 项 PChar 表间接取用 —— 这也是「构件类型共 4 种」的证据。信息内容本身来自实例信息块（见「构件 ABI」），
现版按相同顺序/标点拼装。原版界面上**没有**端口编辑控件，也**没有**
构件类型下拉（此前自创的那两套已删除）。

浏览（`BrowseBtnClick`）是纯探测：`LoadLibrary`(`0x5D1520`) → `GetProcAddress`(`0x5D146C`) 取
`RegComNum`/`RegComInfo`/`CreateInstance` → `RegComInfo(1..n)`（1 基，两个 ANSI 固定缓冲区：注册名 `0x6C` 字节、
显示名 `0x64` 字节）→ `FreeLibrary`(`0x5D13EE`)，不登记到宿主。失败按原口径弹「错误」标题 +「不是局级系统构件！」
（载入失败或缺导出，`MessageBox@0x5D1CAA`）或「没有可用构件！」；命中多个构件时弹
`ComSelectForm: TComSelectForm`（`TPF0 @ 0x21455C`，`BorderStyle = bsDialog`、`Position = poScreenCenter`、
308×211：`Label1` 15,20（`FormCreate` 写「找到」+ N +「个构件」，常量 `0x5D8B30`/`0x5D8B35`）、
`ListBox1` 13,39,196,157（`ItemHeight = 16`、`lbOwnerDrawFixed`、`OnDblClick = OKBtnClick`）、
`OKBtn`「确定」218,39,75,23（默认按钮）、`Button2`「取消」218,67,75,23）。`OKBtnClick` 先按需实例化并读信息块，
校验失败弹「错误」框且不关窗（多条提示直接拼接、无分隔）：创建失败「构件创建失败！」；块长/端口数越界
「构件注册信息不正确！」；数据流输入端口原始数据类型不在 `0..0x0D` 或 `0xFF`
「不支持该构件的数据流输入端口数据类型！\n支持的数据类型为(LSAS_TYPE_BIT~LSAS_TYPE_64F,LSAS_TYPE_VOID)」；
该端口最大数量 ≠ 1「不支持数据流输入端口数目可变！」；多于一个数据流输入端口「不支持多个数据流输入端口！」。
成功时把唯一数据流输入端口 ID 与「是否存在输出端口」写回 `+0x2E4/+0x2F4`，并把 `+0x2F4` 的
`comType == 2` 记到私有标志 `+0x310`。非 `.dll` 文件走另一分支：名称取
文件名、信息清空、直接放行「添加」。

添加（`AddBtnClick`）把浏览选中的文件复制到 `<exe>\com\` 后再注册（现版复制失败报
「复制构件文件失败！（错误码 N）」），成功后往事件表写一行，该行文本为 `'Com'`；删除（`DelBtnClick`）只反注册、
**不删磁盘文件**；修改（`EditBtnClick`）提交编辑后的构件名称；退出 = `mrCancel`。

构件记录是固定 `0x19C` 字节：`+0x000` 文件名、`+0x100` 完整路径、`+0x164` 事件文本（`'Com'`）、`+0x174` 构件名称、
`+0x194` 数据源标志（= `comType == 2`，由 `0x40D6B0` 消费）、`+0x198` 所属指针。

现版落点：`ComponentHost::PeekAbi()`（转发到匿名的 `ProbeAbi()`：探测不登记，32 位在 `LoadLibrary` 前按 PE
Machine 字段拒绝）、`ComponentHost::RegisterDllCopied()`、`ComponentHost::QueryInstanceInfo()`、
`src/dlg/DlgComponent.{h,cpp}` 与 `src/dlg/DlgComSelect.{h,cpp}` 按上表几何/文案/状态机实现，
`MainWindow::DoComponentManage()` 经 `ComponentRegistered` 信号把注册结果写进事件面板。
名称/作者/版本/简要说明/构件类型/端口全部运行期取自实例信息块，不再读 DLL 版本资源（`version.lib` 已移除）。
**刻意差异**：「修改」只改名、不重新复制 DLL —— 现版宿主按路径索引已载入模块，改指向会让 `CreateInstance` 失效；
`+0x310`/`+0x194` 两个标志现版以 `InstanceInfo::comType` 表达，不单独持久化。

顺带纠正旧误标：`ANALYSIS.md` 曾把 `TAgentForm` 当作构件管理、把 `TComCustomForm`/`TComSelectForm` 当作
「串口（COM）自定义与选择」。实际上 `AgentForm: TAgentForm`（`TPF0 @ 0x213DB0`，`AgentUnit`，359×235）
是**不含任何子控件的空窗体**，而带 `ComListBox/ComNameEdit/ComFileEdit/ComInfoMemo` + 添加/删除/修改的正是
`TComCustomForm`；「串口」二字在 `BesCom.dll` 与 `bes.exe` 中出现 0 次，原版「定制」菜单里也没有「串口管理」项，
两份对照表的该行已删除。

## 主菜单结构（DFM）

原版主菜单是 DevExpress `TdxBarManager`（`dxMainMenu`），DFM 的 `ItemLinks` 顺序与按钮 Caption 就是菜单结构；
`MainWindow::CreateMenuBar()` 按此复刻：

| 顶层 | 条目 |
|---|---|
| 文件(&F) | 新建/打开.../保存/另存为.../数据输出.../打印.../关闭/全部关闭/最近打开文件/退出 |
| 编辑(&E) | 进行编辑/剪切/复制/粘贴/撤销/重做/连续·块·行·列选取/选取参数(读入.../存盘.../清除当前选择/清除全部选择)/独特码查找.../帧长查找... |
| 查看(&V) | 联动/设为起点/标尺/取反/倒序/参数/不等长帧查看.../XTAB/YTAB/TAB跟随/纵轴显示(起始比特/帧长/帧号)/放大/缩小/恢复/界面风格(传统风格/VS.NET风格/Office2003) |
| 处理(&P) | 原版 `ItemLinks` 为空；本版承载 不等长帧处理/定位.../退出查找状态 |
| 分析(&A) | 原版仅「迭代」；本版另含显示模式与帧过滤（格式栏也有） |
| 识别(&R) | 原版 `ItemLinks` 为空；本版三项用于打开 `TProcessForm` |
| 定制(&C) | 管理...(Hint 管理)/快速执行... |
| 窗口(&W) | 分页/横排/竖排/分组横排/分组竖排/标题/窗口分组/事件/工具栏；末尾为本版 Tab 补充项 |
| 帮助(&H) | 关于... |

- `DataFormatBtn` 的 Caption 是「倒序」而非「数据格式」，与 `DataNotBtn`「取反」并列；参数入口是
  `ParamBtn`「参数」。旧版自创的「数据格式」「参数设置（定制）」两项已删除。
- 「串口管理」在原版不存在（`BesCom.dll`/`bes.exe` 中「串口」0 次），`CMD_CUSTOM_COM` 已删除，
  定制菜单只剩两项。`DoComSelect()` 随之删除（构件选择仍经 TComCustomForm/浏览与 TProcessForm 使用）。
- 查看菜单在 DFM 中还内嵌起点/帧长/步进/显示四个栏控件（`FbEdit/FlEdit/StepEdit/ViewModeComboBox`，
  ShowCaption=True，caption 为 起点/帧长/步进/显示），与格式栏是同一组 `ItemLinks`。Qt 不能把一个
  widget 同时放进 QToolBar 和 QMenu，故用 `QWidgetAction` 镜像控件：`menuEdStart/menuEdFrameLen/
  menuEdStep/menuCboViewMode`，`OnMenu*Edited()` 先回写格式栏再走原槽，`UpdateMenuState()` 双向同步。
- 顶层的「识别」助记符按 DFM 是 `(&R)`（不是 `(&I)`）；黄金基线因此变化一次。
- 快捷键取自 `dxBarManager1` 二进制流的 `ShortCut` 记录（`scCtrl = 0x4000`、`scShift = 0x2000`，
  低字节 ASCII）：新建/打开/保存/打印 `Ctrl+N/O/S/P`，剪切/复制/粘贴 `Ctrl+X/C/V`，
  撤销 `Ctrl+Z`、重做 `Shift+Ctrl+Z`，联动 `Ctrl+A`，设为起点 `Ctrl+B`，取反裸键 `N`。
  XTab/YTAB 的原版 `Tab`/`Shift+Tab` 不接：Qt 把它当焦点遍历键，窗口级 QAction 会破坏
  对话框与表格导航。`F5/F6/F7/Del` 是本版扩展，DFM 无对应记录。

## 视图右键菜单（DFM 隐藏栏「快捷操作」）

`TMDIChild.FormMouseUp@0x405AE0` 只处理右键（`button == 1`）：在 `dxBarManager1` 中按名
`快捷操作`（VA `0x5d3512`）取到隐藏工具栏，把 `ItemLinks` 拷进 `MainForm.PopupMenu`（`+0x378`）
再于鼠标处弹出；之后照常把消息转发给根面板 `[+0x1e4].vtbl+0x60`
（左键按下 `FormMouseDown@0x405A8C` 对应 `vtbl+0x64`）。`BesView::CreateContextMenu()` 复刻该栏：

联动 / 设为起点 / — / 连续·块·行·列选取 / 选取参数(读入…/存盘…/—/清除当前选取/清除全部选取) /
— / 数据输出… / 另存为… / — / 帧长查找… / 独特码查找… / 退出查找状态 /
— / 切换书签 / 下一标签 / 上一标签 / 清除标签 / — / 定位…

书签四项共用 `BookMarkBtnClick@0x40D250`，按 sender 分派：`+0x37c` 切换 → 书签接口 `vtbl+0x3c`、
`+0x380` 下一 → `+0x40`、`+0x384` 上一 → `+0x44`、其余（`+0x388`）清除 → `+0x48`。
「清除标签」对应新增的 `CMD_BOOKMARK_CLEAR`（40059）。

## 工具栏定制（TdxBarCustomizingForm）

主窗体 DFM 的 `dxBarManager1: TdxBarManager` 管理 4 条栏（主菜单/主工具栏/格式栏/快捷操作），
`CanCustomize` 未关闭（DevExpress 默认 True）。资源中保留标准定制窗体：类名短串
`TdxBarCustomizingForm` @VA `0x51fa27`、VMT `0x51ede4`；DFM `TPF0 @ 0x214800` 的 Caption 为
`Customize`，页签 ` Toolbars `/` Commands `/` Options `/` All Commands `/` Groups `，底部
`Close`/`&New...`/`R&ename...`/`&Delete`/`&Reset...`。DevExpress 的入口是工具栏右键菜单与
双击停靠站点，本版两者都有。

数据模型：每条栏一份命令 ID 有序列表（`MainWindow::m_barItems`），`0` = 分隔符，
`-1` = 格式栏纵轴显示下拉框占位；默认值逐项来自 DFM `ItemLinks`（BeginGroup 即分隔符）。
`RebuildToolBar()` 只摘除布局管理的命令动作和动态分隔符；格式栏的标签/数值框/显示下拉/
YDisp 下拉是固定 `QWidgetAction`，始终保留——占位符之前的分组插在 YDisp 之前，之后的分组
追加到栏尾，所以定制不会打乱固定控件。右键菜单由 `BuildToolBarContextMenu()` 生成（三条栏
勾选 + 「自定义...」），双击只认栏空白处，按钮上的双击仍执行原命令。

`DlgCustomize`（`src/dlg/`）对应标准窗体：Toolbars 页三条栏的显示勾选（立即 `SetBarVisible`）；
Commands 页栏下拉 + 当前按钮/可用命令双列表，添加/删除/上移/下移/Reset.../Close，
YDisp 占位不可选、不可移动。持久化键：`view/barMain|barFormat|barQuick`（逗号分隔 ID）与
`view/tbMain|tbFormat|tbQuick`。原「显示/隐藏全部」占位命令 `CMD_VIEW_TOOLBARS` 已删除——
原版窗口菜单里的「工具栏」是 `dxBarToolbarsListItem1` 子菜单，不是切换命令。

## 窗口分组拖放（等价于停靠站点拖动）

原版的数据窗口是挂在组树下的停靠站点：组项 `[item+0xC]` 指向承载控件
（`[form+0x2F4]`(GroupTreeView)`.+0x254`，联动收集器 `0x407AF8` 也按该字段定位），
分组横排/竖排按组项重插站点（`0x408CB4`/`0x408E2C`）。因此「把窗口拖进分组」在原版是
停靠系统自身的拖动，组树不接受拖放。Qt 版没有真实停靠图层，等价交互落在 `GroupPane`：

- 自定义 MIME `application/x-bes-window-tab`，内容为标签索引的十进制字节；
  `mimeData()` 只允许单个窗口项（分组节点 `UserRole = -1`，返回 nullptr）。
- `dropEvent()` 用 `itemAt(pos)` → `GroupOf()` 解析落点组：落在子项上取其父组，
  落到「数据窗口」即移出全部自定义组。成员变更经 `QTimer::singleShot(0, …)` 延迟，
  避免 Qt 还在拖放事务里持有 item 指针时 `SyncGroupPane()` 清空模型。
- `MainWindow::MoveTabToGroup()` 保证单一归属（先从其它组移除），随后
  `SyncGroupPane()` + `ApplyArrangement()` 立即更新分组排布的可见集合。

测试注意：`QApplication::notify` 对 `QEvent::Drop` 只转发给
`QDragManager::currentTarget()`；合成拖动必须先发送并被接受 `QDragEnterEvent`，
单独 `sendEvent` 一个 `QDropEvent` 会被静默丢弃。

## 状态栏（dxStatusBar1 / CursorInfoLabel）

DFM（`tools/forms.dfm.txt` 1187 起）：`dxStatusBar1` 680×20、`stpsFlat`，三面板
**380（文本）/ 400（容器，`Fixed = False` 随窗口伸展）/ 30（`taCenter`）**；容器内
`CursorInfoLabel`（`alClient`、`Layout = tlCenter`、`Transparent`）。Qt 版对应
`statusFile`（固定 380）/ `statusCursor`（最小 400 + 伸展）/ `statusInsert`（固定 30）。

各视图共用这一个标签：构造时把 `[owner+0x1e8]` 存进视图 `+0x20`（`0x47C870`），
光标/选区更新都走 `[+0x20].vtbl+0x20` 设置 Caption。文本（反汇编 + 内嵌 GBK 串）：

| 状态 | 文本 |
|---|---|
| 文件面板 | `文件: <路径>` |
| 比特视图标记 | `标记: 文件第<起点>比特 第<光标>比特 <行>行 <列>列`（`0x442D84`；默认串 `标记: 文件第0比特 第0比特 0行 0列`） |
| 比特视图加距离 | 同上 + ` 距离[...]`（`0x443DE0`；原版距离串以逗号分隔多个数，含义未解码） |
| 字节视图标记 | `标记: 第<行>行 <列>列`（`0x47FA88`/`0x4803CC`；拖动更新 `0x480630` 另加 ` 距离[...]`） |
| 连续选取（比特 / 字节） | `选取了 %d 比特` / `选取了 %d 字节` |
| 块选取 | `选取了 %d行×%d列` |
| 行选取（比特 / 字节） | `选取了 %d 行` / `选取了 %d行`（字节视图无空格） |
| 列选取 | `选取了 %d 列` |
| 插入指示 | `插入` / `改写` |

`UpdateStatusText()` 以 `UnitOf()` 判定单位（Hex/字符/位图/十六进制字符 = 8，其余 = 1）；
选区非空时显示选取统计，清空后回到标记文本。字节视图的「距离」后缀只在部分原版更新
函数中出现，本版统一在光标离开起点时追加。

## 打印（原版为占位）

`TMainForm` 方法表（VMT `0x5D5710`，`vmtMethodTable = [VMT-0x34] = 0x5D5FF9`，`u16`
数量 58，条目 `{u16, u32 地址, ShortString 名称}`）给出 `PrintBtnClick = 0x40D9BC`；
函数体只有 `MessageBoxA(form.Handle, "未完待续...", "提示", MB_ICONINFORMATION)`
（文本 `0x5D3D7A`、标题 `0x5D3C2D`，GBK）。条目配对由五个同级方法交叉验证
（`ComManageBtnClick→TComCustomForm`、`SearchStringBtnClick→TSearchStringForm`、
`SearchFrameBtnClick→TSearchFlForm`、`NetDataBtnClick→TNetDataForm`、
`FilterBtnClick→TFilterForm`）。`TPrintForm` 的 DFM 还在 `.rsrc`
（`TPF0 @ 0x22B5A0`，305×216），但没有 `.data` 类名短串/VMT，也没有创建它的代码；
导入表无 `StartDoc/StartPage/EndPage`，说明原版从未链接打印代码。

因此 Qt 版的 `DlgPrint` + `MainWindow::DoPrint()`（十六进制表、自动分页、毫米边距按
DPI 换算、页面范围、打印机/纸张设置）是**本版本扩展**；对话框的字段集合沿用该废弃
DFM，但不存在「原版打印样张」可用于像素/行为校准。

## 与原版对应/差异

| 原版 | Qt 版 |
|---|---|
| MDI 子窗口 | ViewArea（`QTabBar` + `QSplitter`）多视图 |
| DevExpress 停靠 | QDockWidget |
| 分页/横排/竖排/分组横排/分组竖排（`[form+0x498]` 全局状态） | ✅ `MainWindow::SetViewLayout` → `ViewArea` 重排全部视图，分组排布只同屏当前组（见「窗口排布」） |
| 4 条 dxBar | 主工具栏 + 格式栏 + 快捷操作（2 行） |
| dxStatusBar1（380/400/30 三面板 + CursorInfoLabel） | ✅ `statusFile`/`statusCursor`/`statusInsert`，标记/选取文本按视图单位区分（见「状态栏」） |
| 视图右键弹菜单（隐藏栏「快捷操作」的 ItemLinks） | ✅ `BesView::CreateContextMenu()`，项序/分隔/子菜单一致（见「视图右键菜单」） |
| `LockScrollBtn` 联动浏览（同一停靠容器内的其它视图） | ✅ 同一标签组内的其它 Tab，按移动量同步（见「联动浏览」） |
| TParamSetForm 4 页（独立启动隐藏构件选项页） | ✅ 数据格式/显示参数/颜色设置可见，构件选项按 DFM 构建但隐藏 + 窗口列表 |
| TFilterForm | ✅ 帧长/数值/CRC 三页 + 条件列表（多条件“与”） |
| TComCustomForm（定制构件管理） | ✅ `DlgComponent`：468×312、列表 + 名称/文件/信息 + 浏览/添加/删除/修改/退出（见「定制构件管理」） |
| TProcessForm（数据流输入/格式输入/数据输出共用一个窗口） | ✅ `DlgProcess`：行集/按钮状态机/校验/优先级按反汇编复刻；✅ `bes::FlowEngine` 把选区按块推入数据流输入端口 |
| TProgressForm | ✅ `DlgProgress`：302×86、进度 0..256、取消 = FormClose |
| TComSelectForm | ✅ `DlgComSelect`：308×211、「找到N个构件」+ 列表(13,39,196,157) + 确定/取消，双击 = 确定；按原版校验实例信息块（创建失败/块越界/数据类型/端口数量）并带回类型、端口数、数据流输入端口 ID 与输出端口存在性；也用于浏览探测到的临时构件列表 |
| TAboutBox | ✅ 332×256 布局/文案（徽标简化绘制） |
| TfrmAddGroupItems | ✅ 分组项对话框 |
| 编辑/撤销/粘贴 | ✅ 按 `EditBtnClick`/`SaveModified`/`editIface` 实现（见「编辑接口」） |
| 32 位构件 | 不支持 |

## 关键坑

1. `menuBar()->setNativeMenuBar(false)`：RDP 下原生菜单不渲染。
2. `QStatusBar::showMessage` 会遮住永久 QLabel，改用 `QLabel`。
3. `QMdiArea/ViewArea` 子窗口是 View 不是 Doc；`ActiveDoc` 经 `view->Doc()`。
4. 工具栏两行布局：`insertToolBarBreak(formatBar)`。
5. `PrintWindow` 抓不到 Qt 内容；截图用 `grab()`。
6. 核心层 `COLORREF→uint32_t (0x00BBGGRR)`，windows.h 用 `NOMINMAX`。
7. 32 位 DLL 在 64 位进程 `LoadLibrary` 返回 193；注册前先探测 Machine 字段并明确拒绝。
8. `RegComInfo` 的 idx 是 1-based，且写 ANSI 固定长度缓冲区。
9. 原版 `CreateInstance` 是 4 参数 stdcall，不能按 `void**` 单参调用。
10. Hex 每字节必须画 2 个 nibble；旧实现只画 1 个，会“丢一半数据”。
11. `OpenEditable` 必须以 `FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE` 打开：映射句柄自身持有 `GENERIC_READ|GENERIC_WRITE`，只放行读共享会在同一文件再次打开时报 `ERROR_SHARING_VIOLATION(32)`。
12. offscreen 平台里 `QMessageBox::information` 会在 `qt_getWindowsSystemMenu` 崩溃（退出码 `-1073741819`）；`run-tests.bat` 不能用 `if errorlevel 1` 判断，必须 `if not "%ERRORLEVEL%"=="0"`，测试路径也要避免弹出模态框。
13. **不要对构建目录跑 `windeployqt`**：`<exe 目录>\Qt5Core.dll` 一旦存在，Qt 就把 exe 目录当成「可搬迁安装」的 prefix，插件搜索路径退化为 `<exe 目录>\platforms`，不再看 `D:/local/Qt5/.../plugins`。`windeployqt` 只拷 `qwindows.dll`，于是 `test_ui.exe`（`QT_QPA_PLATFORM=offscreen`）在 `QApplication` 构造时弹出**原生** MessageBox「no Qt platform plugin could be initialized」并永久阻塞（这条路径不是 `qFatal`，既不输出也不返回错误码；进程 CPU 归零、`MainWindowTitle` 等于程序名即是它）。诊断：`QT_DEBUG_PLUGINS=1 QT_FORCE_STDERR_LOGGING=1`（`QT_LOGGING_TO_CONSOLE` 已废弃）能看到它只检查了 `<exe>\platforms`。要么不部署，要么把 `qoffscreen.dll`/`qminimal.dll`/`qdirect2d.dll` 一并拷进 `platforms\`。

14. **Delphi 常量串可能没有直接 xref**：渲染/文本拼装常通过初始化 PChar 表间接取用（例：`ComInfoText` 的标签池 `0x5DB0BD..0x5DB12B`，成员地址存在 `0x5DAFF8`/`0x5DB008`）。按字符串地址查不到引用并不能推断它未被使用，要再查一遍指向它的 dword。
15. **本镜像的 VMT 偏移要自己校验**：类名 `-44`、实例大小 `-40`、父类 `-36`、字段表 `-56`、方法表 `-52`、自指 `-76`；先确认 `[VMT-76] == VMT` 再读，否则字段表/方法表会读成 0（曾据此误判 `TAgentForm` “无 RTTI”）。
