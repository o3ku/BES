# BES 原版 vs 64 位版 — 功能/操作对比

> 原版参考：`BesCom.dll`（32 位，临时运行，仅用于对比；产品不再支持 32 位）
> 当前版本：`build/qt-msvc-x64-release/bes.exe`
> 对比时间：本轮实现完成后
> 测试：核心 172 断言 + UI 339 断言

## 0. 对比方式说明

| 标记 | 含义 |
|---|---|
| **R** | 实际运行原版与 64 位版，观察界面/状态/输出 |
| **D** | 原版 DFM 窗体与当前对话框逐控件对照 |
| **S** | 源码/自动化测试对照 |

“操作对比完成”指：**每个菜单项/功能都有明确结论**，不等于每一项都逐像素一致。
本轮实际运行对比覆盖：打开/拖放、默认显示、比特显示、Hex 显示、显示模式切换、工具栏/状态栏布局。
其余功能通过 DFM + 当前源码/测试逐项对照。

## 1. 文件 / 编辑

| 功能 | 原版 | 64 位版 | 结论 | 方式 |
|---|---|---|---|---|
| 新建 | 文件→新建 | 打开文件对话框 | ✅ 与 legacy_mfc 一致（新建=打开） | D/S |
| 打开 | 文件→打开 / 拖放 | 文件→打开 / 拖放 / 命令行 | ✅ 可用；拖放均支持 | R |
| 保存 | 编辑态下按 500000 字节分块原位回写；只读文件报「文件不可写！」 | `FileMap::Flush`（映射）/`BinaryEditor::Save`（内存副本，同样分块） | ✅ 只读提示与分块尺寸一致 | D/S |
| 另存为 | 输出到文件 | 按选区/全量输出 | ✅ 基本对齐 | D/S |
| 打印 | 占位：`PrintBtnClick@0x40D9BC` 仅弹「提示：未完待续...」，未链接打印代码 | 整文档打印；左右/上下边距（毫米，默认 10）、页面范围（空=全部）、纸张标签与打印设置 | ➕ 本版扩展（原版无打印功能，无样张可校准；见 §6.33） | D |
| 最近文件 | MRU 列表 | QSettings 8 条（MaxItemCount=8） | ✅ | S |
| 关闭/全部关闭/退出 | 有；未保存询问只发生在退出编辑时 | 有；同样只在退出编辑时询问 | ✅ 关闭路径不询问（与 `EditBtnClick` 一致） | D/S |
| 进行编辑 | `EditBtn`（bsChecked，标题/提示「进行编辑」，编辑菜单首项）→ `EditBtnClick@0x40B724` | 快捷工具栏文字按钮 + 编辑菜单首项「进行编辑」；进入无前置校验，退出按脏标志弹「文件内容已改变，保存吗？」(Yes/No/Cancel)；进入编辑时格式栏「显示」下拉框禁用，退出恢复 | ✅ 一致（禁用的控件即 `[form+0x45c]` = `ViewModeComboBox`，见 §6.23） | D/S |
| 剪切/复制 | `CutBtnClick@0x40B8DC`/`CopyBtnClick@0x40B8FC` → `editIface` +0x28/+0x2C | 连续选取整字节进剪贴板；剪切另加删除（需编辑态） | ✅ 接口非空壳，已按字节实现 | D/S |
| 粘贴 | `PasteBtnClick@0x40B91C` → `editIface` +0x30 | 按状态栏插入/改写决定后移或覆盖（需编辑态） | ✅ | D/S |
| 撤销/重做 | `UndoBtnClick@0x40B89C`/`RedoBtnClick@0x40B8BC` → +0x34/+0x38；Shift+Ctrl+Z | 逐操作撤销栈；Ctrl+Z / Shift+Ctrl+Z | ✅ 栈空时按钮禁用 | D/S |
| 连续/块/行/列选取 | 4 种；按钮 Tag 0..3，初始 -1 | 4 种；二维块；拖动累积、松开提交、同按钮再点清空退出；单击已选行/列再点即取消该行/列；Shift+导航键扩展、拖出视口自动滚动 | ✅ 鼠标/模式按钮/键盘扩展均按反汇编对齐（见 §6.38、§6.39）；单击反选为本版补充手势（原版只靠模式按钮/清除命令，见 §6.43） | R/D/S |
| 选取参数读入/存盘 | 有（.slp）：3 字节 `slp` + 4 段（Int32 数量 + 24 字节记录） | 同格式读写；标题/过滤/默认扩展名 .slp | ✅ 布局与段序（连续/块/行/列）均按反汇编 + 按钮 Tag 复刻 | D/S |
| 清除当前/全部选取 | 有 | 有 | ✅ | S |

## 2. 查看 / 布局

| 功能 | 原版 | 64 位版 | 结论 | 方式 |
|---|---|---|---|---|
| 放大/缩小/恢复 | 工具栏按钮 | 工具栏/菜单/Ctrl+滚轮 | ✅ | R/S |
| 界面风格 | 传统/VS.NET/Office | 三种 QSS，互斥并持久化 | ⚠️ 视觉近似 | D/S |
| 标尺 | 上/下/左标尺 | 上/下标尺 + 左侧行号标尺（分页为帧号，横排为数据行行号）；刻度数字按实际字宽避让，字节模式不再叠字（见 6.43） | ✅ | R/S |
| 横/纵分割线 | XTAB/YTAB | XTAB/YTAB | ✅ | S |
| 分页 | 有（`[form+0x498]=0`，一屏一个视图） | `ViewArea` 只显示当前标签（见 6.28） | ✅ | D/S |
| 横排/竖排 | 有（`[form+0x498]=1/2`，全部站点重新插入根面板） | 全部视图并排/堆叠（`QSplitter` 换向） | ✅ | D/S |
| 分组横排/竖排 | 有（`[form+0x498]=3/4`，组内子项按 Left+Right / Left+Bottom 链式插入） | 只同屏当前组的视图；边界由分隔条呈现，排布不改 XTAB/YTAB（见 6.36） | ✅ | D/S |
| 标题 | 有（原版默认选中） | 独立开关，默认选中，可与各布局叠加 | ✅ | R/S |
| 窗口分组 | 停靠面板/拖动 | 树形分组：窗口项可直接拖到分组节点（含子项）归组，拖回「数据窗口」移出全部自定义组；仍有右键 Tab 加入/新建分组 | ✅ 拖放交互等价，见 42 | D/S |
| 工具栏显隐/定制 | dxBar 定制（`TdxBarCustomizingForm`：Toolbars/Commands 页） | 每条工具栏独立显隐；右键或双击栏空白处打开自定义对话框，可增删/上移下移按钮并持久化，格式栏固定控件不被破坏 | ✅ 按 DFM 类名/窗体文本与 DevExpress 默认入口复刻，见 41 | D/S |
| 联动浏览 | 有 | 垂直+水平按「同一移动量」位移同容器其它视图 | ✅ 范围/语义按 `0x407B4C` 复刻，见 25 | S |
| 竖轴 YFN/YFL/YFB | 有 | 有 | ✅ | R/S |
| 默认帧长/显示 | 打开后 帧长 64、比特字符 | 已改为 64、比特字符 | ✅ | R |
| BitChar 0/1 | `.` / `1`，紫色 | `.` / `1`，紫色 | ✅ | R |
| Hex 每字节 | 2 nibble + 空格，紫色 | 2 nibble + 空格，紫色 | ✅ | R |
| Hex 帧长 | 显示 8（字节单位） | 已改为显示 8（字节单位） | ✅ | R |
| 标尺刻度 | BitChar/Hex 刻度密度不同 | 按 8 比特边界标注并画刻度：比特位图 8 格一刻，字节模式每格一刻，随模式/缩放变化 | ✅ | R/S |
| 状态栏 | 标记/文件比特/字节/行列/距离/插入 | 三面板 380/400(伸展)/30；文件面板「文件: 路径」；比特视图「标记: 文件第…比特 第…比特 …行 …列」+ 距离，字节视图「标记: 第…行 …列」+ 距离；有选区改「选取了 …」 | ✅ 面板几何与文本按 DFM/反汇编 | D/S |

## 3. 处理 / 分析

| 功能 | 原版 | 64 位版 | 结论 | 方式 |
|---|---|---|---|---|
| 数据格式/参数 | TParamSetForm | 数据格式=参数设置；4 页 + 窗口列表（独立启动时「构件选项」页隐藏） | ✅ 逐窗编辑格式/显示参数；“从端口”无状态端口时禁用；应用不关闭 | D |
| 迭代/单步 | 有 | 单步=模式开关；迭代=空占位 | ⚠️ 原版 `DiedaiBtnClick` 反汇编为 `ret`，本版一致不推进起点 | S |
| 不等长帧查看 | NetDataForm/查看 | DlgVarFrame 帧表 | ✅ 帧标记编辑/切分/检测/读入 .bnt | D |
| 不等长帧处理 | 前导码/预置/格式/误码/去四相/.bnt/中止 | 同左 + 帧表读入/写出 | ✅ 控件与处理流程对齐；窗体布局不同 | D |
| 定位 | 4 种方式 + Apply | 4 种方式 + Apply（不关闭） | ✅ | D |
| 独特码查找 | 格式/码/误码/去模糊/进度 | 同左 + 命中列表 | ✅ 增强 | D/S |
| 帧长查找 | 范围 3 项 + 检测行数(30) + 最少同步列数(2) | 同左：候选帧长递增 + 全0/1列统计，命中后立即应用 | ✅ | D/S |
| 退出查找状态 | 有 | 清除过滤/可见帧 | ✅ | S |
| 二进制/十六进制/文本/位图 | 有；格式栏下拉仅 5 项，位图按钮不切换模式（原版 mode 7 为休眠字节网格，见 §6.34） | 有；下拉 5 项与原版一致，位图按钮为扩展入口（灰度 Pixel） | ✅ 扩展已标注 | R/S |
| 十六进制字符对应 | 有 | 有 | ✅ | S |
| 帧过滤 | 三页（帧长/数值/CRC）+ 条件列表 | 同左 + 条件添加/更改/删除/清空；多条件相“与”；数值低字节在前 | ✅ | D/S |

## 4. 识别 / 定制 / 窗口 / 帮助

| 功能 | 原版 | 64 位版 | 结论 | 方式 |
|---|---|---|---|---|
| 数据流输入/格式输入/输出 | TProcessForm（一个窗口，三类行） | `MainWindow::DoProcessFlow` → 构件选择 + `DlgProcess` + `bes::FlowEngine`；行集/按钮状态机/校验/优先级按反汇编复刻，选区按块推入数据流输入端口 | ✅ 界面、状态机与数据推送等价（原版直接调构件 vtable，现版经端口接缝） | D/S |
| 定制构件管理 | TComCustomForm：构件列表 + 构件名称/构件程序文件/构件信息 + 浏览… + 添加/删除/修改/退出；浏览只探测 ABI（多构件弹构件选择），添加把文件复制进 `com\` 后登记，删除只反注册不删文件，32 位 DLL | 同布局/同状态机/同文案（468×312，见 `FEATURE_COMPARISON.md` 30/31）；仅 64 位；端口按原版只读展示，运行期取自实例信息块；`ShowModal` 可用 | ⚠️ 32 位不支持（按需求）；数据源标志以 `comType` 表达 | D/R |
| 参数设置（查看→参数，原 `ParamBtn`） | 4 页 + 窗口列表（状态基为 nil 时隐藏「构件选项」页） | 同原版：数据格式/显示参数/颜色设置 3 页可见，「构件选项」页按 DFM 构建但隐藏；不再出现在定制菜单（见 30） | ✅ “从端口”语义按反汇编还原（独立启动无状态端口，禁用）；数值裁剪/颜色方案已对齐 | D |
| 快速执行 | 「选择构件」+ 过滤「局级系统构件(*.dll)」→ 临时载入 DLL 并校验 `RegComNum/RegComInfo/CreateInstance` → 构件列表 → 对所选构件 `ShowModal` | 同原版：临时 `ComponentHost`（不写持久注册表），缺导出/载入失败一律报「不是局级系统构件！」，无构件报「没有可用构件！」 | ✅ 等价（构件实例数据管线仍未实现） | D |
| 新建窗口/多视图 | MDI | Tab 多视图共享文档 | ⚠️ 结构不同 | D/S |
| 关于 | AboutBox 资源 | DlgAbout：332×256、徽标+版本/接口/作者/单位 | ✅ 布局与文案对齐；徽标为简化绘制 | D |
| 事件面板 | EventPanel | EventPane | ✅ | S |
| 窗口分组项 | TfrmAddGroupItems | DlgGroupItems | ✅ | D |
| 主菜单分组 | 9 个顶层菜单，条目见 `TdxBarManager` DFM | 顶层/标题/条目分组已对齐；定制仅「管理...」「快速执行...」，参数在查看、排布在窗口（见 30） | ✅ 功能入口均在；原版为空的处理/识别菜单保留本版入口 | D |

## 5. 对话框控件对照（DFM）

| 原版窗体 | 当前 | 对照结果 |
|---|---|---|
| `TParamSetForm` | `DlgParamSet` | 数据格式/显示参数/颜色设置/构件选项 4 页 + 左侧窗口列表（状态基为 nil 时隐藏构件选项页，独立启动一致）；从端口/手工单选（无状态端口禁用从端口）；确定/取消/应用（应用不关闭）；颜色 9 项 + 默认方案/方案一；缓存/块裁剪、临时目录可写检测与可用空间；与 DFM 控件顺序一致 |
| `TFilterForm` | `DlgFilter` | 三页（帧长/数值/CRC）+ 条件列表；添加/更改/删除/清空；“应用”不关闭并立即作用主视图；数值按从低字节到高字节输入；CRC 校验帧尾最后 2 字节 |
| `TSearchStringForm` | `DlgSearchUnique` | 格式/码/误码/去模糊/进度；当前增加命中列表 |
| `TSearchFlForm` | `DlgSearchFl` | 范围 3 项 + 检测行数/最少同步列数；结果“帧长N，共找到M个全0/1列”，找到即应用 |
| `TOutSetForm` | `DlgOutputSet` | 左侧端口树 + 右侧 32 个端口单选（3 列）；树/单选双向联动，保留输出内容复选框 |
| `TPositionForm` | `DlgPosition` | 4 种定位方式 + 书签 + 应用（不关闭） |
| `TPrintForm` | `DlgPrint` | 原版窗体仅存在于 DFM（`TPF0 @ 0x22B5A0`，305×216），类未实例化；`PrintBtnClick` 只弹「未完待续...」。本版 `DlgPrint` 借用其控件集合（左右/上下边距、页面范围、纸张标签、打印设置）并补充毫米/DPI 换算，属本版本扩展 |
| `TComCustomForm` | `DlgComponent` | ✅ 468×312、`bsSingle` + 仅系统菜单/关闭；`ComListBox`(11,13,136,285，`ItemHeight=16`) + `GroupBox「构件属性」`(153,11,308,257) 内 `ComNameEdit`(8,39,216,20，`MaxLength=32`，唯一可编辑)、`ComFileEdit`(8,83,266,20，只读，显示「文件名: 注册名」)、`BrowseBtn「…」`(275,83,26,21)、`ComInfoMemo`(8,125,289,124，只读，运行期从实例信息块取 `作者/版本/简要说明/构件类型/物理端口数量` + 每端口 5 行)；底部 添加(151,275)/删除(229,275)/修改(308,275) 初始禁用、退出(386,275)=`mrCancel`；`FormCreate` 选中首项；无端口表与类型下拉（此前自创，已删） |
| `TComSelectForm` | `DlgComSelect` | ✅ `bsDialog`、308×211、`poScreenCenter`；`ListBox1`(13,39,196,157，`ItemHeight=16`) + `确定`(218,39,75,23，默认) + `取消`(218,67,75,23)；双击 = 确定；既服务已注册宿主，也服务浏览探测到的临时构件列表 |
| `TNetDataForm` | `DlgVarFrame` | 原版文件/预置/格式/进度；当前帧标记/前导码检测/.bnt 读入写出 |
| `TProcessForm` | `DlgProcess` | 434×448；开始/暂停/停止/复位/退出（48×22，暂停为可反复按下的开关）；优先级 5 项默认「正常」＝0；完成后退出/自动打开输出文件默认勾选；输入选择/反选/选取参数/数据类型(16 项，末项 255)/输出标志(EOI/EOF)/输出文件(文件: temp.dat｜不输出)/自定义参数/自定义显示(「点击按钮打开...」)；InfoLabel + 进度条 0..256；空闲仅「开始」可用，运行禁开始启暂停/停止，退出四键全禁并置退出请求；校验「选取参数不能为空！」「: 文件名不能为空！」 |
| `TProgressForm` | `DlgProgress` | 客户区 302×86、信息标签 (9,22,201,13)、进度条 (8,48,202,17) 固定 0..256、取消 (217,47,75,23) 为默认按钮、桌面居中、关窗等同取消 |
| `TfrmAddGroupItems` | `DlgGroupItems` | 分组名 + 窗口勾选；✅ |
| `TAboutBox` | `DlgAbout` | 332×256、标题“关于BES”、产品名/版本/接口版本/作者/单位、底部分隔线、确定按钮；徽标为简化静态绘制 |
| `TMainForm` / `TMDIChild` | `MainWindow` / `BesView` | Tab + Dock 代替 MDI；工具栏/状态栏有差异 |

## 6. 本轮根据对比完成的修正

1. 打开默认值：`帧长=64`、`等长帧比特字符`（与原版一致）。
2. BitChar：`0 → '.'`、`1 → '1'`；正文默认紫色。
3. Hex：每字节 `2 nibble + 空格`；每行 8 字节。
4. Hex 帧长按**字节单位**显示（64 比特显示为 8），修改时自动 ×8。
5. Hex 标尺改为按位偏移（0、8、16…），不再给空列编号。
6. 添加 `--view/--zoom/--screenshot` 命令行，用于固定状态截图对比。
7. 黄金截图基线更新为当前默认显示。
8. 增加下标尺；状态栏按原版三面板与标记/选取文本复刻（见 6.32）；最近文件上限 8 条。
9. 不等长帧支持 `.bnt` 帧表读入；构件支持 `ShowModal` 调用；输出端口使用真实端口号，并按原版改为端口树 + 32 端口单选。
10. 打印对话框按已废弃的 `TPrintForm` DFM 控件集合改为左右/上下两个边距输入（毫米，默认 10）和单个页面范围输入；边距按打印机 DPI 换算，修正此前按设备像素直接相加的问题。（原版打印未实现，见 33）
11. 参数设置按 `TParamSetForm` 重写：4 页 + 窗口列表逐窗编辑，独立启动隐藏「构件选项」页（原版状态基为 nil 时 `ComOptionPage.TabVisible=False`）；“从端口/手工”单选（窗口端口号非 0 才启用，原版按端口记忆）；确定/取消/应用（应用不关闭、确定仅在有修改时应用）；颜色默认方案/方案一；缓存 0→1MB、非法→64、上限 1024，块 32..min(缓存×1024,4096)、非法→640，帧长上限 90000000、步进≥1、TAB≥2。
12. 标题按原版改为独立开关并默认选中（`TitleBtn.Down`），不再作为互斥布局；与其他布局叠加显示帧号/帧长/起点；黄金基线随之更新。
13. 关于对话框按 `TAboutBox` 复刻（332×256、产品名/版本/接口版本/作者/单位、确定按钮；徽标为简化绘制）；选取参数对话框标题改为“选择参数文件”，过滤与默认扩展名改为 `.slp`。
14. 格式栏起点/帧长/步进补上下箭头，等效原版 `TdxBarSpinEdit`：按当前显示单位 ±1、0 处钳制；文本仍支持 64 位十进制输入；黄金基线更新。
15. 状态栏右侧补原版 30px「插入/改写」指示（对应 DLL 中 `插入`/`改写` 字符串）：默认插入，Insert 键切换；自本轮编辑功能落地后，该指示决定粘贴是后移还是覆盖（见 22）；黄金基线更新。
16. 标尺补齐：横排/分组横排也保留左侧行号标尺（YFN=行号、YFL=行宽、YFB=行首比特）；上/下标尺在 8 比特边界画短刻度，比特位图 8 格一刻、字节模式每格一刻，随模式/缩放变化；黄金基线更新。
17. 格式栏按钮改为原版的“图标在上、文字在下”：工具栏文字去掉菜单助记符与省略号；参数按钮标题为“参数”（打开参数设置），放大/缩小/恢复按原版顺序；`test_ui` 链接图标资源后校验；黄金基线更新。
18. 倒序与参数设置“高位在前(0-7)”合并为同一选项（原版 `DataFormatBtn` 提示即“选中为高位在前（0-7）未选为低位在前（7-0）”，`ReverseCheckBox` 同义）：工具栏按钮呈选中态并与复选框双向同步；未选中时仅在**每字节内**反转比特（比特模式按 7-0 取位、不跨字节），取反在其后应用；黄金基线更新。
19. 单步与迭代分离：原版工具栏 `StepBtn` 为“单步”、提示“切换单步和连续状态 / 只对数据流输入有效”、`bsChecked`，与另一个“迭代”按钮（`DiedaiBtn`）不是同一命令。现工具栏「单步」可勾选，只切换文档单步模式（无数据流时不移动视图）；“处理”菜单的「迭代」按原版 `DiedaiBtnClick`（反汇编为单个 `ret`）作为空占位，不推进起点；工具栏不再显示“迭代”。
20. 快速执行按原版实现（此前被错接到“刷新视图”）：原版 `FastExecuteBtnClick` 先弹出标题「选择构件」、过滤「局级系统构件(*.dll)」的文件对话框，`LoadLibrary` 后取 `RegComNum`/`RegComInfo`/`CreateInstance` 三个导出，任一取不到即弹「错误」+「不是局级系统构件！」，`RegComNum` 返回 0 弹「没有可用构件！」，否则用 `RegComInfo(1..n)` 建临时构件列表并弹选择框，对所选构件调用 `ShowModal(当前文件)`。现改为同流程（临时 `ComponentHost`，不写入持久构件注册表），载入失败与缺导出统一报「不是局级系统构件！」，与原版一致。
21. `.slp` 选取参数文件改为原版二进制格式：反汇编 `BesCom.dll` 的 `Load@0x440624`/`Save@0x4407AC` 确认文件以裸 3 字节 ASCII `slp` 开头（无前导长度、无版本号），随后固定 4 段，每段 `Int32 记录数` + 记录数 × 24 字节小端记录 `{Int64 p1; Int32 c1; Int64 p2; Int32 c2}`；魔数不符报「不是选取参数文件！」。`SelectionSet::SaveSlp/LoadSlp` 落地该布局（当时段序按 `数×数` 显示串推断为 行/块/连续/列，见 24 已按证据纠正；书签标记不写入），Qt 侧“读入/存盘选取参数”走二进制，`legacy_mfc` 保留原文本格式。
22. 编辑功能落地（此前剪切/复制/粘贴/撤销全为「内存不足」占位）：按 `EditBtnClick@0x40B724`、`SaveModified@0x40B93C`、`UndoBtnClick/RedoBtnClick/CutBtnClick/CopyBtnClick/PasteBtnClick` 与 `editIface` vtbl（+0x24 编辑态、+0x28/+0x2C/+0x30 剪三操作、+0x34/+0x38 撤销重做、脏标志 `[engine+0x1a4]`、插入标志 `[engine+0x1c]`）实现——快捷工具栏新增无图标文字按钮「进行编辑」（原版 `bsChecked`，进入即把文件换成可增长内存副本，退出时若脏则弹「文件内容已改变，保存吗？」Yes/No/Cancel）；保存按原版 500000 字节分块原位回写，只读文件报「文件不可写！」；剪切/复制/粘贴按连续选取整字节与光标字节位置工作，粘贴方式由状态栏「插入/改写」（Insert 切换）决定；撤销/重做逐操作回滚，重做快捷键按 DFM 改为 Shift+Ctrl+Z，栈空时按钮禁用。未复刻的两点：进入编辑时禁用的控件（`[form+0x45c]`）成员当时未知，见 23 已补齐；`EditKeyPress@0x40FFF0` 只是 TEdit 的十六进制输入过滤，无证据支持在数据区内直接键入改字节，故未发明该行为。
23. 编辑态禁用的控件补齐：用 TMainForm RTTI 字段表（VMT `-44` 类名、`-56` 字段表；字段表头 `{Word 数量=112, Int32 属性指针}`，其后 112 条 `{Int32 Offset; Int16 类型索引; Byte 名字长度; 名字}`，正好解析到方法表 `0x5D5FF9` 收尾）把 `[form+0x45c]` 定位到 **`ViewModeComboBox`**——格式栏上的 `TdxBarCombo`，DFM 里 `Caption = "显示"`、`Hint = "显示模式"`、设计期 `Enabled = False`、`OnChange = DrawContextChanged`、栏内 `UserWidth = 114`。`EditBtnClick@0x40B724` 在进入编辑时对其调用布尔 Enabled 设置器 `0x509AA4`（`dl=0`），每条成功退出路径再 `dl=1`；退出被拒（后台任务运行中报「任务运行时不能退出编辑！」，或保存失败）时改为按回 `EditBtn`（`0x50AA0C`）并保持禁用。`0x40A3D8` 的状态回填里同一控件也从记录 `[esi+0x18]` 再断言一次，所以现版把 Enabled 统一收敛在 `MainWindow::UpdateMenuState()`：`m_cboViewMode->setEnabled(has && !editing)`（无文档时同原版设计期为禁用）。同场解码 `SaveModified@0x40B93C` 的三条失败提示——「文件不可写！」「后台任务运行时不能保存文件！」「目标磁盘空间不够！」，后两条在现版路径上不可达（无后台任务、无短写判定），仅记录。
24. `.slp` 段序纠正 + 未决字段落定（改动 21 的推断）：`Load@0x440624`/`Save@0x4407AC` 本身与选区语义无关——对象的 `+4/+8/+0xC/+10` 是四个 `TList`，按字段地址顺序写「Int32 记录数 + 记录数 × 24 字节」，段归属必须由消费方确定。证据链两段：① `0x444924` 投递记录时按 `[[self+0x18]+0]`（选取模式）分支，模式 0 → `[self+4]`（并 `Sort`，比较器 `0x4033AC`）、1 → `[self+8]`、2 → `[self+0xC]`、3 → `[self+0x10]`（模式 2/3 还会合并相邻区间、必要时拆分）；② 「模式」就是工具栏按钮的 `Tag`——`SelectBtnClick@0x40AC88` 读 `[Sender+0x83]`（Down）后把 `[Sender+0xC]`（Tag，未按下传 `-1`）写入同一模式字段，而 DFM 中 `ContinueSelectBtn` 无 Tag（⇒0）、`RectSelectBtn Tag=1`、`RowSelectBtn Tag=2`、`ColSelectBtn Tag=3`，四者共用 `OnClick=SelectBtnClick`/`GroupIndex=2`/`bsChecked`/`AllowAllUp`。故**段序 = 连续 / 块 / 行 / 列**（原实现把段 0 与段 2 写反）。顺带落定 21 留下的“语义未知”字段：块选取对两组「行+列」各取 min/max 归一化；行选取只用 Int64 半边（min@+0、max@+0xC），两个 Int32 恒 0；列选取只用 Int32 半边（min@+8、max@+0x14），两个 Int64 恒 0（`[self+0x68]=-1` 时按最后一列收尾）；连续段逐字拷贝 p1/c1/p2/c2，其两个 Int32 是随选区携带的待定参数，本模型写 0。原版行/列列表存区间、本模型每行一条 `row1=row2`，读侧统一按 min..max 展开，故与原程序双向可互换。`SaveSlp/LoadSlp` 与 `Selection.h` 注释按此改写，`test_core` 的 `.slp` 断言改为校验新段序（段计数偏移 3/31/59/111 = 1/1/2/1、连续记录 p1@7=100 与 p2@19=200、行记录 p1@63=7）——核心 151 断言全过、`GOLDEN OK`。
25. 联动浏览的范围与语义补齐：开关是 `LockScrollBtn`（TMainForm 字段表 `+0x3C8`，DFM `Caption`/`Hint` 均为「联动浏览」、`Style = bsChecked`、快捷键约 `Ctrl+A`，**没有 OnClick**——`bsChecked` 自己翻 `Down` 态，状态读 `[obj+0x83]`）。同步对象收集器 `TMainForm::0x407B4C(当前视图, outList)`：先 `outList.Clear()`，若 `[[form+0x3C8]+0x83] = 0` 直接返回；否则取 `当前视图.[+0x24].vtbl[+0x108]()`（即视图所属的停靠站点），要求它是 `TdxSideContainerDockSite`，再遍历其 items 把 `item.[+0xC]`（视图本身）加入列表，跳过 nil 与触发者——**联动范围 = 同一停靠容器内的其它视图，不含自己，也不跨容器**。传播量是「位移」而不是「绝对位置」：三个触发点（滚轮 `0x405C34`，行数 = `-WheelDelta div 120`；滚动条通知 `0x4061C6`，按 `TScrollCode` 0..5 换算，源视图基准取 `rec+0x160`、其它视图基准取 `rec+0x15C`；`0x4064A9` 的 ±1/3/5 跳转表）都对列表里每个视图执行「自己的位置 + 同一移动量」，写回走 `rec.[+0x2E0].[+0x1E4].vtbl[+8]`（`0x5C90B0` Int64 加、`0x56EF8C` Int64 存）；辅助遍历 `0x407AF8` 则是在 `[form+0x2F4]`(GroupTreeView)`. [+0x254]` 里按 `item.[+0xC]` 比对，说明分组树只用于定位视图、不扩大同步范围。现版落点：单个 `m_tabs` 就是中心控件（`GroupPane` 只是导航树），故「同一停靠容器」等价于「同一标签组内的其它 Tab」；`BesView::OnVScroll/OnHScroll` 改为记录前值并把差值交给新增的 `BesView::LinkShift(dRow, dCol)`，`LinkShift` 在 `MainWindow::LinkBrowse()` 为真时遍历 `centralWidget()` 的 `QTabWidget`、跳过自己，把增量加到每个视图自己的 `ScrollRow()/HOffset()`（钳到 ≥0）并用 `SetScrollRowQuiet/SetHOffsetQuiet` 静默写入；原先「广播绝对位置」的 `SetScrollPosQuiet` 删除。`test_ui` +10 条断言（两视图、均有滚动范围、开启后其它视图按同一移动量位移且不跳到源视图绝对行、关闭后不跟随、反向同样带动、开关恢复初值），UI 增至 211 断言；核心 151 断言、`GOLDEN OK`。
26. 构件数据流处理窗体补齐（§5 原记「`TProcessForm` 当前为简化版」）：`TProcessForm`（VMT `0x5D86A0`、实例 `0x3A4`、字段表 `0x5D8784` 28 项、方法表 `0x5D896F` 16 项）就是「识别 → 数据流输入 / 数据格式输入 / 数据流输出」三项共用的窗口——DFM 的行分类为 数据输入(`DataInRow`) / 数据输出(`DataOutRow`) / 构件参数(`ComParamRow`) / 构件自定义(`AdvanceRow`)，而「数据格式」正是其中的 `DataTypeRow`。新增 `DlgProcess` 按 DFM 复刻：434×448、`开始/暂停/停止/复位/退出`（48×22，除开始/退出外初始禁用）、`优先级` 下拉（默认 `ItemIndex = 3`「正常」，由 `FormCreate 0x413A8F` 写入，DFM 未带）、`完成后退出`/`自动打开输出文件`（DFM 均 `Checked`）、输入选择 5 项（`全部数据/连续选取/块选取/行选取/列选取`，字面量 `0x5D761F..0x5D7656`）、反选（值 `'1'/'0'`，`0x48F2F0` 写行 `+0xB0`）、选取参数（`0x4A2210` 写行 `+0x88`）、数据类型 16 项（Values `0..14` 加末项 `255`「未知数据类型」，默认 0）、输出标志 `EOI/EOF`、输出文件 `文件: temp.dat` / `不输出`、自定义参数/自定义显示两行按钮文案「点击按钮打开...」、底部 `InfoLabel` + 进度条 `Min 0 / Max 256`。行为按反汇编：`SelectModeRowChange 0x4163B0` ⇒ 反选仅 `全部数据/块选取` 可用且可用时清空勾选（原版另判 `[[ctx+0x74]+4] = 7`，该字段属于另一个上下文对象，与本 ABI 的 1 基构件类型 1..4 无关，现版不启用该分支；见 29），选取参数在 `全部数据` 时可编辑并填「全部数据」、否则填段名且仅当文本不等于「不接受手工输入」(`0x5D7696`，`0x5D0B60` 等值比较) 时才可用；`RunBtnClick` 尾部 `0x416198` ⇒ 禁「开始」、启「暂停/停止」、「复位」取决于 `[form+0x344]`，并置 `[ctx+0x19] = 1`；`PauseBtnClick 0x4161F8` 读按钮 `Down`（`+0x130`）后向构件发命令 3（暂停）/4（继续）；`StopBtnClick 0x416250` 禁暂停/停止/复位并清运行标志；`ResetBtnClick 0x41629C` 发命令 5；`CancelBtnClick 0x4162AC` 四按钮全禁 + `[ctx+0x19]=0` + `[ctx+0x18]=1`（退出请求），`FormClose` 先调它；空闲态由 `0x415650` 定义（仅「开始」可用、进度条 `Visible = False`）。校验按 `0x415C15..0x415C70`：选取模式非 0 且选取参数为空 ⇒ 红字 InfoLabel「选取参数不能为空！」(`0x5D7670`) 并中止，另有「: 文件名不能为空！」(`0x5D7683`)。`PriorityComboBoxChange 0x416590` 用 `0x5D7600` 的 `{-15,-2,-1,0,+1}` 表按 ItemIndex 取优先级，故 `DlgProcess::priorityValue()` 返回同一组值。`DlgProgress` 同时按 `TProgressForm` 收紧：客户区 302×86、`ProgressInfo` (9,22,201,13)、进度条 (8,48,202,17) 固定 `0..256`（原实现自定标度）、取消按钮 (217,47,75,23) 为默认按钮、`poDesktopCenter`、关窗等同取消。`test_ui` +37 条断言（空闲/运行/停止/复位/退出五态、进度 0..256 映射与封顶、输入选择启用规则、数据类型 255、输出端口与输出标志、校验文案、优先级默认值、进度窗几何），UI 增至 248 断言；核心 151 断言、`GOLDEN OK`。**仍缺**（已由 27 落地）：真正把数据推给构件的端口管线（`[form+0x360]` 引擎对象 + `[form+0x364]` 构件实例，见下条）。（此前 §2 表格只标注为「⚠️ 原版具体联动范围未知」）

27. 构件数据端口管线落地（补齐 26 的「仍缺」，新增 `src/core/FlowEngine.h/.cpp`）：原版 `TProcessForm` 自己不搬数据——`FormCreate@0x413844` 把主窗体传入的数据流上下文存进 `[form+0x360]`（`@0x410689` 交接，`@0x41067B` 里 `[ctx+0x8c] := 0x10000` 即块上限）、`[form+0x364]` 置 nil、`[form+0x344] := 1`（决定「复位」是否可用，故现版默认支持复位）、`[form+0x358] := 100`；`RunBtnClick` 尾部 `@0x4161D7` 只 `mov byte [eax+0x19],1` 置运行标志，`PauseBtnClick@0x4161F8` 走 `SuspendThread([ctx+0x14])`（`@0x416213`）+ 构件命令 3（`@0x416228`）/命令 4（`@0x41623C`）+ `ResumeThread`（`@0x416248`），`ResetBtnClick` 发命令 5（`@0x4162A7`），退出把 `[ctx+0x18]` 置 1，即「暂停/停止/退出」是对工作线程与构件的 signalling 操作。构件侧调用按 vtable 槽：命令 `+0x28`、优先级 `+0x30`（`@0x4165E0`）、自定义参数 `+0x18`（`@0x41714A`/`@0x4174A9`）、自定义显示 `+0x34`（`@0x41660E` 传 1/2、`@0x416133` 传 `101,0,0`）。但 `BesCom.dll` 只导出 4 个符号（名字串位于文件 `0x1D3325`/`0x1DA61D`，`.edata` 在 `0x206112`），上下文/记录引擎并不在其中，其记录的二进制布局无从验证，因此现版不虚构该 ABI，而是复用已确认的端口接缝：新增 `bes::IFlowPort`（数据流输入端口）由 `MainWindow::FlowInputFile` 以临时文件承载，与「输出到端口」的 `TempFilePort` 对称；`bes::FlowEngine` 接受 `FlowParams`（输入选择/反选/输出标志/数据类型/选取参数/优先级 + `[ctx+0x8c]` 块上限），把连续/块/行/列选区（含反选补集）用与 `OutputWriter::emit` 相同的比特→字节换算合并成区间、按块上限切分后逐块推入端口。线程模型改为单线程 + 块间回调：`DlgProcess::startRun()` 绑定泵（更新进度并 `processEvents`），暂停时在泵里等待恢复，故「暂停/停止/退出」仍能即时生效，`Pause/Resume/Stop/Reset/RequestExit` 与原版命令 3/4/5、`[ctx+0x18]` 一一对应；提示文案取原版的「数据长度为0！」、「没有可用的数据流输入端口！」（端口数量/类型/最大数量的前置校验已在构件选择时按原版拦截，见 29）与「端口输入失败！」。`MainWindow::DoProcessFlow` 负责绑定文档映射/选区/帧模型/每格比特与端口，窗口关闭后在事件面板回显「已输入 N 字节（端口文件 …）」。`test_core` +20 条断言（三类前置校验、全量推送、块上限 100 → 11 块且末块 24 字节、行/块/列/连续四类选区、反选补集 1016 字节、比特列取样、停止保留进度、暂停后继续不丢数据、命令 3/4/5、退出中断、端口写失败），核心增至 171 断言；`test_ui` +6 条（复位能力开/关两个方向、「开始」经引擎把 256 字节推入端口、「全部数据」映射为 `SelectMode::None`），UI 增至 254 断言；`GOLDEN OK`。仍只做到接口层的是构件自定义视图尺寸同步（命令 100，`com.vtbl[+0x34](com, 100, &rect, 16)`），它依赖构件实例的真实窗口句柄。

28. 窗口排布（分页/横排/竖排/分组横排/分组竖排）按原版补齐：此前把 `ViewLayout` 当作「当前文档的绘制方式」，反汇编确认它其实是**主窗体的全局状态**——`TMainForm` 字段 `[form+0x498]`。五个处理函数先写常量再遍历全部停靠站点：`PageBtnClick@0x408B20` 置 0（`xor edx,edx` → `mov [ebx+0x498],edx`，**不重新插入**，只做 `0x407DFC/0x408A78` + `0x5442B4/0x5573CC/0x407F38` 刷新，即恢复单页）；`HorizBtnClick@0x408B94` 置 1 后对每个站点执行 `0x544BC8(eax=[item+0xC] 控件, edx=[form+0x2E4] 根面板, cl=3, 索引 -1)`；`VertBtnClick@0x408C24` 同样循环但 `cl=2`；`PageHorizBtnClick@0x408CB4` 置 3，先用 `0x426BB0`（子项数）/`0x4265A0`（是否单项）分流，多子项时首项 `cl=1` 插进根面板、其后各项相对前一项 `cl=5` 依次插入；`PageVertBtnClick@0x408E2C` 置 4，把链上的 `cl=5` 换成 `cl=4`。站点集合取自 `[form+0x2F4]`(GroupTreeView)`.+0x254`（`0x426F54` 计数、`0x4273BC`/`0x4267D8` 取项/取子项），`cl` 即 Delphi `TAlign`（None0/Left1/Top2/Client3/Bottom4/Right5）。⇒ 排布是「全部视图」级别、全局只有一份，切文档/切页不改变它，原版也不借它改字节流。现版落点：新增 `src/app/ViewArea.h/.cpp` 取代中心控件 `QTabWidget`（`QTabBar` + `QSplitter`，手柄 4px、子视图不可折叠）——`Paged` 只让当前视图可见但把其它视图**留在 splitter 里**（移出会丢几何，联动浏览的位移基准会错），其余四种按 `shownViews` 设可见集合并切分隔方向（竖排/分组竖排 = 纵向）；`MainWindow::SetViewLayout(bes::ViewLayout)` 对应 `[form+0x498]`（新增全局成员 `m_viewLayout`），一次写入所有文档的 `params.layout` 再 `ApplyArrangement()`，分组排布的可见集合取当前标签所在组、无分组时回退为全部；`Actions.cpp` 五个命令收敛到该入口，`OnTabChanged/CloseTab` 重排；`BesView::LinkShift` 改遍历 `ViewArea`；新开/克隆视图经私有 `ApplyLayoutTo(doc)` 沿用当前全局排布（原版 `[form+0x498]` 只有一份，新窗口不会回到分页）。`bes::ViewLayout` 枚举值刻意与 `[form+0x498]` 一一对应（0..4）。**本版本扩展保留**：横排/分组横排时渲染器把行当作连续数据流（见 §7 表新增行）。`test_core` +1 条（编码 0..4 与中文模式名），核心增至 172 断言；`test_ui` +9 条（默认分页只有一个视图同屏、横排/竖排切换分隔方向与可见集合、分组横排/分组竖排只同屏当前组、排布切换把显示模式一并写进全部视图、回到分页后其余视图不再同屏），UI 增至 263 断言；`GOLDEN OK`（默认排布未变，基线不变）。同场修掉一个截图不稳定因素：进程没有前台焦点时 Qt 认为窗口 Inactive，样式按 `QStyle::State_Active` 改用调色板的 Inactive 组，菜单文字变成灰 `(160,160,160)`，同一画面会有两种哈希；`QApplication::setActiveWindow()` 与 `QPalette::setCurrentColorGroup()` 都压不住（前者被平台激活状态覆盖，后者被样式重新判定），截图模式改为把 Active 组整组复制进 Inactive 组。

29. 构件 ABI 细节与选择窗体校验定案（补齐 26 的端口前置校验与旧误读）：`RegComInfo` 两缓冲区角色明确——第 2 参数（栈上 `0x6C` 字节）是**注册名**（`CreateInstance` 的入参），第 3 参数（`0x64` 字节）是**显示名**；列表显示显示名，`ComFileEdit` 显示 `文件名: 注册名`（旧记「完整路径」有误）。实例信息经 vtable 槽 18（x64 `0x90`）先取长度再填充、槽 1 释放，内联布局：`+0x00` 作者[20]、`+0x14` 版本[10]、`+0x1E` 说明[50]、`+0x50` 构件类型、`+0x54` 端口数、`+0x58` 起每端口 `0x3C` 字节（`+0x28` ID、`+0x2C` 原始数据类型、`+0x30` 最大数量、`+0x34` 缺省数量、`+0x38` 端口类型）。`TComSelectForm.OKBtnClick` 校验（标题「错误」、失败不关窗、多条提示直接拼接）：创建失败「构件创建失败！」；块长/端口数越界「构件注册信息不正确！」；数据类型不在 `0..0x0D`/`0xFF`「不支持该构件的数据流输入端口数据类型！\n支持的数据类型为(LSAS_TYPE_BIT~LSAS_TYPE_64F,LSAS_TYPE_VOID)」；数据流输入端口最大数量 ≠ 1「不支持数据流输入端口数目可变！」；多于一个数据流输入端口「不支持多个数据流输入端口！」。成功时带回唯一数据流输入端口 ID 与输出端口存在性（原版写 `+0x2E4/+0x2F4`），记录 `+0x194`（窗体 `+0x310`）= `(comType == 2)` 数据源标志。选择窗 `Label1` 为「找到 N 个构件」（常量 `0x5D8B30`/`0x5D8B35`）。落点：`ComponentHost::QueryInstanceInfo()`、`DlgComSelect` 校验与 `DataInPortId/HasOutputPort/ComType/PortCount`、`DlgComponent` 的文件/信息显示；`plugins/portcfg` 持久化删除（只存 DLL 路径），`DlgProcess` 的 `comType == 7` 反选映射不再启用。`test_ui` 更新至 299 断言，核心 172 断言，`GOLDEN OK`。

30. 主菜单按原版 `TdxBarManager` 重建（删除不存在的命令）：DFM 主菜单 `dxMainMenu` 的顶层顺序为 文件(&F)/编辑(&E)/查看(&V)/处理(&P)/分析(&A)/识别(&R)/定制(&C)/窗口(&W)/帮助(&H)。逐条读取 `ItemLinks` 与按钮 Caption 后定案：编辑首项是 `EditBtn`「进行编辑」，选取读入/存盘/清除当前/清除全部收在 `dxBarSubItem18`「选取参数」子菜单，末尾是独特码查找/帧长查找；查看依次是联动浏览(`LockScrollBtn`)/设为起点/起点·帧长·步进·显示（栏控件）/不等长帧查看(`NetDataBtn`「不等长帧查看...」)/取反(`DataNotBtn`)/倒序(`DataFormatBtn`，`Hint` 即“选中为高位在前（0-7）未选为低位在前（7-0）”，并非“数据格式”)/参数(`ParamBtn`)/标尺(`StuffBtn`)/XTAB/YTAB/TAB跟随/纵轴显示/放大·缩小·恢复/界面风格(`dxBarSubItem10`，三项 Caption 为 传统风格/VS.NET风格/Office2003)；处理与识别的 `ItemLinks` 为空，分析只有「迭代」，定制只有「管理...」(`ComManageBtn`，Hint 管理) 与「快速执行...」，窗口 = 分页/横排/竖排/分组横排/分组竖排/标题/窗口分组/事件/工具栏列表，帮助只有「关于...」。据此删除 `CMD_CUSTOM_COM`「串口管理」（原版无此串、无此命令）、`CMD_CUSTOM_PARAM`（参数重复且位置错误）、`CMD_ANALYZE_FORMAT`「数据格式」（与倒序重复）以及与 XTAB/YTAB 重复的横/纵分割线命令；查看菜单补参数、标尺、设为起点、不等长帧查看，窗口菜单接管五种排布/标题/工具栏显隐，`DoComSelect()` 随串口命令删除。**保留的差异**：处理/识别在原版为空、分析仅「迭代」，但本版仍需菜单入口来打开不等长帧处理、定位、退出查找与 `TProcessForm`，故分别保留；查看内嵌的四个栏控件最初只出现在格式栏（已在 6.35 补齐）；窗口菜单末尾新增 Qt 多标签用的「新建窗口/上一个/下一个」。`test_ui` +6 条（定制仅两项、查看含参数等、窗口含五种排布、编辑含进行编辑/选取参数子菜单、识别 `(&R)`、无「串口管理」），UI 增至 305 断言，核心 172 断言；`GOLDEN OK`（基线仅因顶部「识别」助记符变化而更新）。

31. 视图右键菜单与「清除标签」补齐：`TMDIChild.FormMouseUp@0x405AE0` 只在 `button == 1`（右键）时，按名称 `快捷操作`（VA `0x5d3512`）在 `dxBarManager1` 里取出该**隐藏工具栏**，把 `ItemLinks` 复制到 `MainForm.PopupMenu`（`+0x378`）并就地弹出，之后仍把消息转发给根面板 `[+0x1e4].vtbl+0x60`（左键按下走 `vtbl+0x64`，`FormMouseDown@0x405A8C`）。DFM 里该栏的项序为：联动(`LockScrollBtn`) / 设为起点(`SetStartBtn`) / | / 连续选取 / 块选取 / 行选取 / 列选取 / `dxBarSubItem18`「选取参数」(读入… / 存盘… / | / 清除当前选取 / 清除全部选取) / | / 数据输出… / 另存为… / | / 帧长查找… / 独特码查找… / 退出查找状态 / | / 切换书签 / 下一标签 / 上一标签 / 清除标签 / | / 定位…。书签按钮共用 `BookMarkBtnClick@0x40D250`：`+0x37c`（切换）→ 书签接口 `vtbl+0x3c`、`+0x380`（下一）→ `+0x40`、`+0x384`（上一）→ `+0x44`、其余即 `+0x388`（清除）→ `+0x48`。据此新增 `CMD_BOOKMARK_CLEAR`（40059，Caption「清除标签」）与 `DoClearBookmarks()`，`BesView::CreateContextMenu()/contextMenuEvent()` 按上表构建菜单（含分隔符与子菜单位置）。**修正两处旧误**：`LockScrollBtn` 的 `Caption` 是「联动」、`Hint` 才是「联动浏览」（25、30 两条记为两者相同）；快捷工具栏的「清除标签」图标原先错接「清除当前选取」，现已改为清除书签。`test_ui` +7 条断言（清除标签文案、快捷栏按钮归属、书签置/清、菜单非空、完整项序、选取参数子菜单内容），UI 增至 312 断言，核心 172 断言；`GOLDEN OK`（截图未展开弹窗，仅「联动」文案变化）。

32. 状态栏按原版 `dxStatusBar1` 定案（本版旧实现是三个弹性标签 + 自创文本）：DFM 三面板为 380（文本）/ 400（容器，`Fixed = False` 随窗口伸展，内嵌 `CursorInfoLabel`：`alClient`、`tlCenter`、`Transparent`）/ 30（`taCenter`），`PaintStyle = stpsFlat`。反汇编确认 `CursorInfoLabel` 是各视图共同的文本目标：视图构造时把 `[owner+0x1e8]` 存进自身 `+0x20`（`0x47C870`），光标/选区更新都调 `[+0x20].vtbl+0x20` 写 Caption。文本：文件面板 `文件: <路径>`；比特视图 `标记: 文件第<起点>比特 第<光标>比特 <行>行 <列>列`（`0x442D84`），离开起点时追加 ` 距离[...]`（`0x443DE0`）；字节视图 `标记: 第<行>行 <列>列`（`0x47FA88`/`0x4803CC`），拖动类更新另有 ` 距离[...]`（`0x480630`）。选区非空时同一标签改显示选取统计：比特视图 `选取了 %d 比特` / `%d行×%d列` / `%d 行` / `%d 列`（`0x43E308`）；字节视图 `选取了 %d 字节` / `%d行` / `%d 列`（`0x47CB14`，行文案无空格）；清空选区后恢复标记文本。落点：`CreateStatusBar()` 重建三面板（`statusFile`/`statusCursor`/`statusInsert`），`UpdateStatusText()` 按 `UnitOf()` 生成文本。**保留差异**：字节视图的距离后缀只在三个同类更新函数之一出现，本版统一在光标离开起点时追加；比特视图 `0x443DE0` 的距离串以逗号分隔多个数值（连续追加 3 个逗号），含义未解码，本版只报单个距离；原版程序在本机无法启动（致命对话框 `Failed to set data for ''`），格式均出自 DFM/反汇编。`test_ui` +14 条断言（面板存在/宽度、文件面板文本、两种标记、距离、四种选取文案与空格差异、清空复原），UI 增至 326 断言，核心 172 断言；`GOLDEN OK`（黄金样例为比特视图，面板宽度与默认标记随之更新基线）。

33. 打印定案：原版是「未完待续...」占位，本版为扩展。`TMainForm` 方法表（VMT `0x5D5710`；`vmtMethodTable = [VMT-0x34] = 0x5D5FF9`；`u16` 数量 58；条目 `{u16, u32 地址, ShortString 名称}`）解析出 `PrintBtnClick@0x40D9BC`，函数体为 `MessageBoxA(form.Handle, "未完待续...", "提示", MB_ICONINFORMATION)` 后返回（文本 `0x5D3D7A`，标题 `0x5D3C2D`，GBK）。条目配对以五个同级方法交叉确认：`ComManageBtnClick`→`TComCustomForm`、`SearchStringBtnClick`→`TSearchStringForm`、`SearchFrameBtnClick`→`TSearchFlForm`、`NetDataBtnClick`→`TNetDataForm`、`FilterBtnClick`→`TFilterForm`（各自经 `[0x5E....]` 类引用取 VMT 名称）。`TPrintForm` 的 DFM（`TPF0 @ 0x22B5A0`，305×216，Edit1/2 默认 `10`）没有对应的 `.data` 类名短串/VMT，也没有创建它的代码；导入表无 `StartDoc/StartPage/EndPage`，说明打印代码从未链接。结论：原版无打印功能，本版 `DlgPrint`/`DoPrint()` 保留为文档化扩展（控件集合取自该废弃 DFM），不再以「原版打印样张」为校准目标。无代码改动，核心 172 / UI 326 断言与 `GOLDEN` 不变。

34. 位图定案：原版「位图」是休眠的 mode 7 字节网格视图，不是灰度像素图，本版保留 Pixel 渲染为扩展。
    - **下拉 5 项**：DFM `ViewModeComboBox`（`Enabled = False`、`ItemIndex = -1`、`OnChange = DrawContextChanged`）只有 比特位图 / 等长帧比特字符 / 等长帧十六进制 / 字符方式 / 十六进制字符对应。`DrawContextChanged@0x40A888` 在 `0x40A95E` 用 `[combo+0x100]`（ItemIndex）作为新 mode，仅当当前记录 mode 已是 7 时保持 7；参数对话框 `modeCombo` 同样 5 项。
    - **位图按钮不切模式**：DFM 的 `BMPBtn` 与 `BinBtn/HexBtn/TextBtn` 同为 `bsChecked`、`GroupIndex = 3`、`OnClick = DrawContextChanged`，但该函数从不读取 `BMPBtn`（`+0x488`）；Bin/Hex/Text/BMP 的按下态只在 `[view+4] == 7` 时写入记录 `+0x64`（`0x40AA4A`）。所以正常 UI 里按「位图」只是重绘当前下拉模式。
    - **唯一入口**：视图构造 `0x403D50` 从流里读 4 字节魔数 `0x1234ABCD`（写入点 `0x47C08D`），相等才把视图与记录 mode 置 7；mode 7 下下拉框被禁用（`0x509AA4` 传 `dl=0`）。
    - **绘制**（painter `0x47C870`）：按 `+0x64` 分 0 Bin / 1 Hex / 2 Text / 3 BMP。1 是十六进制文本网格（4 格一组、Courier New）；0/2 是 5 格一组、每 10 格标号的文本网格（Text 用楷体）；3（BMP）从 `0x47D374` 起画方格网格——格宽取缩放值（`+0x54`），50 格一组、每 50 格下划线、每 100 格标号，固定画笔/画刷颜色，循环内没有按字节值取色的代码；`0x47E414` 只是求行内最大格宽的辅助函数。结论：它是「每字节一个方格的网格」，无灰度映射。
    - **落点**：格式栏 `cboViewMode` 去掉了多余的「位图」项，恢复原版 5 项；`MainWindow` 用 `ComboIndexFromView`/`ViewFromComboIndex` 映射 combo 4 ↔ `ViewMode::HexChar`，`ViewMode::Pixel` 在 combo 中显示为 -1；「位图」按钮/菜单仍选中本版灰度 `Pixel` 作为文档化扩展（原版正常 UI 不可达，且原版在本机无法启动，无样张）。`test_ui` +5 断言（下拉 5 项、第 5 项文案、位图按钮切 Pixel、Pixel 不回显、combo 4↔HexChar），UI 331 / 核心 172；`GOLDEN OK`。

35. 查看菜单内嵌栏控件：DFM 的 `FbEdit/FlEdit/StepEdit` 是 `TdxBarSpinEdit`（Caption 起点/帧长/步进、ShowCaption=True、Enabled=False、`OnChange = DrawContextChanged`，MaxLength 11/8/8），`ViewModeComboBox` 是 `TdxBarCombo`（Caption 显示）；四者的 `ItemLinks` 同时挂在格式栏和 `dxMainMenu` 的查看子项，即原版两处是同一组控件。落点：把格式栏的 spin 行提取为 `MainWindow::CreateSpinRow()`，查看→设为起点之后用 `QWidgetAction` 插入 起点/帧长/步进/显示 四个镜像控件；`WireSignals()` 把菜单文本框接到 `OnMenu*Edited()`（先回写格式栏再走原槽），菜单下拉直接走 `OnViewModeCombo()`；`UpdateMenuState()` 同步两处文本/索引/帧长提示与「显示」可用性。Qt 不能把一个 widget 同时放进 QToolBar 与 QMenu，故用镜像而非同一实例。`test_ui` +6 断言（四个控件存在、起点镜像、菜单下拉 5 项、菜单改起点生效且格式栏跟随、菜单下拉 4↔十六进制字符对应并同步格式栏），UI 337 / 核心 172；`GOLDEN OK`（菜单未展开，截图不变）。

36. 分组排布不再伪造 XTAB/YTAB 边界线（补齐 §2 表「组内边界线为近似」）：此前 `ApplyLayoutTo()` 在组模式下强制 `showXTab/showYTab = true`，`BesView` 再把线宽加到 2px。反汇编 `PageHorizBtnClick@0x408CB4`（置 3）与 `PageVertBtnClick@0x408E2C`（置 4）只有四步：取根面板 `[form+0x2E4]` 客户区 → 遍历 `[form+0x2F4]`(GroupTreeView)`.+0x254` 的组项（`0x4273BC` 取项、`0x426BB0` 为真时跳过、`0x4265A0` 判定单/多子项）→ 用 `0x544BC8` 重插承载控件 `[item+0xC]`（单子项 `cl=1`；多子项首项 `cl=1`、其余相对前一项 `cl=5` / `cl=4`）→ 刷新 `0x5442B4/0x5573CC/0x407F38/0x54C208/0x54C1DC/0x54DB60`。没有视图参数写、没有 XTAB/YTAB Down 态写、也没有绘制调用，故组间边界由停靠站点/分隔条自身边框呈现。落点：`MainWindow::ApplyLayoutTo()` 只写 `params.layout`；`BesView` 的 XTAB/YTAB 只受用户开关控制（删除组模式强制与 2px 线宽）。`test_ui` 拆成两条断言（模式写入全部视图、排布不改 XTAB/YTAB 开关），UI 338 / 核心 172；`GOLDEN OK`。

37. 参数设置「从端口/构件选项」语义定案（补齐 11 的待定与 §4 的 ⚠）：反汇编确认主窗口 `+0x4A0` 是宿主状态基，由 `0x406DB8` 从 `[ebp+0xC]` 写入（`0x406E3C`，全模块唯一写点）；独立启动 `ShowModal@0x401ED0` 以 `0x401F08 push 0` 传入 nil。`ParamBtnClick@0x40CF80` 把该字段压栈给构造 `0x419688`，存入 `[form+0x3E8]`；`FormCreate@0x419E64` 判空后对 `ComOptionPage` 调 `0x42513C` 置 `TabVisible=False`。`WindowListChange@0x41A198` 用窗口 `+8` 端口号启用「从端口」，勾选值存 `[form+0x3EB+端口]`（同端口共享）；`EditChange@0x41A9A4` 在 `GroupBox2` 内统一启停。本版文件窗口端口号为 0，故「从端口」禁用、「构件选项」页隐藏，与原版独立启动一致；宿主通道（`0x4184BA`，消息码 `0x401`，可传非空状态基）未接入。`test_ui` 增隐藏页断言（UI 339）；核心 172；`GOLDEN OK`。

38. 连续/块/行/列选取的鼠标与模式按钮交互定案（补齐 §1 的 ⚠）：选取对象在子窗口 `+0x1E4`（模式 1 的 VMT `0x5DDBDC`），`+4/+8/+0xC/+0x10` 为连续/块/行/列四个 `TList`，`+0x54..+0x68` 是锚点/当前端点的比特与列，`+0x51` 拖动中、`+0x52` 自动滚动。流程：`MouseDown@0x443840` 仅左键、把锚点与当前端点设为命中比特；`MouseMove@0x443DE0` 只更新当前端点并刷新绘制，拖出客户区时自动滚动（`+0x52`），全程不写列表；`MouseUp@0x444924` 才把一段 24 字节记录追加到当前模式列表（连续 `TList.Add + Sort`，比较器 `0x4033AC` 按起点升序；块直接追加；行/列合并相邻区间）。⇒ **多次拖动默认累积**，鼠标处理内没有 `GetAsyncKeyState`，Ctrl 不参与。模式按钮 `SelectBtnClick@0x40AC88` 把 Tag（连续 0/块 1/行 2/列 3，未按下 -1）写入模式；`SetSelectMode@0x43FFBC` 切换时调 `0x4404CC(dl=0)` 只清上一模式列表，再次点击当前按钮则 `0x4404CC(dl=1)` 清全部四表并退出模式；DFM 四按钮 `bsChecked/GroupIndex=2/AllowAllUp` 均无 `Down=True` ⇒ 初始模式 -1。`[child+0x198]` 只由粘贴路径 `0x44135F` 置位（下一次按下先清选区），与累积语义无关。落点：`SelectionSet` 连续段改为按起点排序的多段 vector，Renderer/OutputWriter/FlowEngine/状态栏/`.slp` 全部按多段遍历；`BesView` 按下/移动只维护预览副本，松开 `CommitSelection` 追加记录并删除 Ctrl 分支；`BesDoc::SetSelectMode` 实现「切换只清上一模式 / 同模式再点清空退出」，四个命令改 checkable 并由 `UpdateMenuState` 同步，文档默认模式改回 None。`test_core` +13（多段排序/渲染/统计、行/列区间并集、ClearMode 只清指定列表、`.slp` 两段连续往返），核心 185；`test_ui` +12（按钮可勾选、累积两条、切换只清上一模式、再点清空退出、按下/移动不提交、松开提交、第二次拖动累积且 Ctrl 无效），UI 351；`GOLDEN OK`。

39. 选取的键盘扩展与拖出自动滚动（补齐 §7 的残留）：`KeyDown@0x445B0C` 先要求选取模式 ≠ -1；跳表把 VK_SHIFT(0x10) 映射到 `0x445CC9`（置 `+0x51=1`、当前端点 `+0x60/+0x64/+0x68` 置 -1），方向/PgUp/PgDn/Home/End 在 Shift 按下时移动当前端点——Up/Down 行 ±1、Left/Right 列 ±1、Home 行首、End 行尾、PgUp/PgDn ±`+0x40`（可视化行数），首次按键从锚点走一步；`KeyUp@0x4477DC` 收到 Shift 松开时 `call [vmt+0x60]`（= `MouseUp@0x444924`）提交，Esc 分支在模式 0/1 下遍历列表删除与锚点/端点匹配的记录。鼠标侧 `MouseMove@0x443DE0` 用 `PtInRect`（`0x443F5C`）判断鼠标是否已离开客户区，离开则置 `+0x52` 并启动定时器自动滚动。落点：`BesView::keyPressEvent/keyReleaseEvent` + `BeginKeyboardSelection/MoveKeyboardEndpoint/EndKeyboardSelection`——端点在「显示行 × 格列」坐标上移动，换算回比特后走同一份 `CommitSelection`（连续/块/行/列各自成记录），松开 Shift 提交、Esc 取消预览；拖出视口用 60ms `QTimer`（`UpdateAutoScroll/OnAutoScrollTick`）滚动行列并把鼠标点钳进视口重新命中，端点跟到屏幕边缘格。`test_ui` +13（Shift+Right/Down/End/Home 端点、松开提交 16..64、Esc 取消、块/行/列三种提交、无模式不动作、拖出自动滚动与松开提交），UI 364、核心 185；`GOLDEN OK`。

40. 无 Shift 方向键的光标移动（补齐 39 的残留）：`KeyDown@0x445B0C` 尾段 `0x44611D` 的门槛是 `state+0x18 == 0`、Shift 未按下、参数位为 0；随后按跳表移动光标 `+0x54/+0x58`（比特）与 `+0x5c`（列），并调 `0x4404CC(dl=0)` 只清当前模式列表。具体位移：Up/Down 行 ±1（保持列）、Left/Right 列 ±1 且行首再左折返到上一行末列、行尾再右折返到下一行首列、Home 行首、End 行尾、PgUp/PgDn ±可见行数（`+0x40`）。落点：`BesView::MoveCaret(dRow, dCol, absCol)` 在「显示行 × 格列」坐标上计算后写回 `BesDoc::SetCursorBit`，先 `ClearMode(当前模式)` 清列表，再 `EnsureBitVisible` 滚动到光标并重绘；无 Shift 的方向键不再驱动滚动条。`test_ui` +9 断言（Right/Down/End/Home 光标、带连续选区按方向键只清列表、左右折返、首格/末格钳制、PgDown 按页步进），UI 373、核心 185；`GOLDEN OK`。

41. 工具栏按钮级定制（`TdxBarManager` + `TdxBarCustomizingForm`）：主窗体 DFM 持有 `dxBarManager1: TdxBarManager`（主菜单/主工具栏/格式栏/快捷操作），`CanCustomize` 未被关闭（DevExpress 默认 True）；资源中保留标准定制窗体——类名短串 `TdxBarCustomizingForm` @VA `0x51fa27`、VMT `0x51ede4`，DFM `TPF0 @ 0x214800` Caption `Customize`，页签 ` Toolbars `/` Commands `/` Options `/` All Commands `/` Groups `，底部 `Close`/`&New...`/`R&ename...`/`&Delete`/`&Reset...`；官方入口是工具栏右键菜单与双击停靠站点。落点：`MainWindow` 每条栏保存命令 ID 有序列表（`m_barItems`，`0` = 分隔符，`-1` = 格式栏 YDisp 下拉占位；默认值逐项取自 DFM `ItemLinks`），`RebuildToolBar()` 只重建布局管理的命令动作和动态分隔符，格式栏固定控件（标签/数值框/显示下拉/YDisplay）原地保留，占位之前的分组插到其前、之后的分组追加到栏尾；右键工具栏（含按钮）弹三条栏勾选 +「自定义...」，双击栏空白处调 `DlgCustomize`（Toolbars 页勾选显隐；Commands 页栏选择、当前/可用双列表、添加/删除/上移/下移/Reset.../Close，YDisp 占位不可选）。布局与显隐写入 `view/barMain|barFormat|barQuick`、`view/tbMain|tbFormat|tbQuick`；原「显示/隐藏全部」占位命令 `CMD_VIEW_TOOLBARS` 删除（原版窗口菜单的「工具栏」是子菜单）。`test_ui` +25 断言，UI 398、核心 185；`GOLDEN OK`（格式栏补上「参数｜标尺」的 BeginGroup 分隔，基线更新）。

42. 窗口分组的拖放归组（补齐 §2 表「窗口分组 ⚠️ 非拖拽」）：原版把每个数据窗口作为停靠站点挂进组树（`[form+0x2F4]`(GroupTreeView)`.+0x254`，组项 `+0xC` 指向承载控件；联动收集器 `0x407AF8` 也按该字段定位视图，见 25），分组横排/竖排再按组项重新插入站点（见 36）——「把窗口拖进组」在原版是停靠系统的拖动，组树本身只读。本版没有真实停靠站点，等价交互落在分组树：`GroupPane` 设 `DragDrop` 模式，`mimeData()` 只允许单个窗口项（分组节点 `UserRole = -1` 不产生数据），自定义 MIME `application/x-bes-window-tab` 携带标签索引；`dropEvent()` 用 `itemAt(pos)` 经 `GroupOf()` 解析落点组（落在子项上即其父组），再以 `QTimer::singleShot(0, …)` 等拖放事务结束才发 `WindowDroppedOnGroup`——直接在 drop 里重建树会在 Qt 仍持有 item 指针时清空模型。`MainWindow::MoveTabToGroup()` 维持「一个窗口只属于一个自定义组」：先从其它组移除再加目标组，目标为「数据窗口」则移出全部自定义组；成员变化后 `SyncGroupPane()` + `ApplyArrangement()` 立刻刷新分组排布的可见集合。右键菜单「新建分组...」复用 `CreateGroup()`，新建时选中的窗口同样受该约束。`test_ui` +15 断言（拖动模式、MIME 往返与非法数据拒绝、新建空组、窗口移入、重复拖入去重、跨组移动、新建分组时勾选窗口会把该窗口移出原组、同组两窗同屏、移出后只剩组内成员、拖回「数据窗口」、空组回退全部视图、真实 `DragEnter`+`Drop` 事件路径），UI 415、核心 185；`GOLDEN OK`（分组树不参与黄金截图）。注意：`QApplication::notify` 只把 `Drop` 转发给 `QDragManager::currentTarget()`，合成拖动必须先让 `DragEnter` 被接受，测试因此按两段事件驱动。

43. 列刻度数字避让与行/列单击反选（补齐用户报告的「列上的数字刻度全挤到一起了」「行、列选取，选中后无法取消」）：上/下标尺原按固定 4 格宽绘制数字，字节/十六进制模式每格都是 8 比特边界，逐格写字导致压字；现改为 `QFontMetrics::horizontalAdvance` 实际字宽排版，数字与上一标签右边界 + 6px 相交则跳过（刻度线全部保留），任意格宽/缩放/模式不再重叠。行/列选中无法取消的根因是缺少「再点一下」这一手势：原版 `MouseUp@0x444924` 行/列分支只做区间并集合并，`KeyDown@0x445B0C` 的 Esc 跳到 `0x44611D` 空操作，取消只能再点当前模式按钮（`AllowAllUp`）或「清除当前/全部选取」——这些路径本版均已具备。补上 `SelectionSet::ToggleRow/ToggleCol`（有序插入/删除、保持模式），`BesView::CommitSelection` 在行/列起止帧（列）相同即 toggle，拖动跨多行/列仍走 `AddRowRange/AddColRange` 累积。`test_core` +4、`test_ui` +4，核心 189 / UI 426；`GOLDEN OK`（黄金截图为 8 比特宽格，标签间距本就足够，基线不变）。

44. 快捷键与原版 DFM 对齐（交互巡检）：逐条解码 `dxBarManager1` 二进制流里的 `ShortCut` 记录（`scCtrl = 0x4000`、`scShift = 0x2000`，低字节 ASCII）。原已一致：新建 `Ctrl+N`、打开 `Ctrl+O`、保存 `Ctrl+S`、打印 `Ctrl+P`、剪切/复制/粘贴 `Ctrl+X/C/V`、撤销 `Ctrl+Z`、重做 `Shift+Ctrl+Z`；本轮补上联动 `Ctrl+A`、设为起点 `Ctrl+B`、取反裸键 `N`。故意不接 XTab/YTAB 的 `Tab`/`Shift+Tab`：Qt 里它们是焦点遍历键，作为窗口级 QAction 快捷键会破坏对话框/表格导航，原版由 DevExpress 工具栏按钮自行处理按键，无法用 QAction 等价表达。`F5` 刷新、`F6` 帧长查找、`F7` 独特码查找、`Del` 清除当前选取在 DFM 无记录，保留为文档化扩展。`test_ui` +5 断言（逐项比对 `QKeySequence`），UI 431。

45. 构件命令 100 分派层（补齐 6.27 的「仅接口层」）：真实调用点在 `0x4154D3`，属 `TProcessForm.SetComponent`（函数 `0x414378`，入口把构件实例写入 `[form+0x364]`）。它先经 `[form+0x334]`（`ContainerPanel`）的 `vtbl+0x40` 取客户区到 `[ebp-0x23C]` 的 16 字节 `TRect`，再 `push 0x10; push &rect; push 0x64; push com; call [vtbl+0x34]`，即 `int handler(void* self, int cmd, void* arg, int argSize)`。返回 `-100` 表示构件不提供自定义视图，宿主在 `0x4155F8` 对 `ContainerPanel` 调 `SetVisible(False)`（`0x59BF38`）；其余返回值隐藏默认窗格 `[form+0x2F0]/[form+0x2F4]`，并用调用前后的矩形比较结果重排窗体（`0x59B834/0x59B858/0x59B878`）。`CreateInstance` 第 4 参数确认是 `ContainerPanel.Handle`（`0x4180C0..D0`，`TWinControl.Handle` 在 `+0x14C`），第 2 参数是临时上下文记录（`0xAC` 字节），第 1 参数是注册名 `[form+0x2EC]`。落点（只做可验证部分）：`bes::ViewRect`（4×int32，16 字节）、`kCmdSetViewRect = 100`、`kCmdNoCustomView = -100`；`ComponentHost::SendCommand()` 经 vtable 索引 13（x64 `0x68`）分派、`__try/__except` 保护；`SetViewRect()` 按原版口径给出 `hasView`；`DestroyInstance()` 补调槽 1 释放，析构函数在 `FreeLibrary` 前释放存活实例。`test_plugin` 的命令槽记录命令号/长度/矩形/次数并被 `TestPluginStats` 导出；`test_ui` +9 断言（槽 13 分派、16 字节矩形原样送达、未知命令 -100、`SetViewRect` 两方向、调用计数、销毁释放），UI 440。**仍未接**：`DlgProcess` 没有原版的 `TPanel ContainerPanel`（DFM：`Align = alBottom`、`OnResize = ContainerPanelResize`），窗口托管需要把容器 HWND 交给真实构件，仓库内没有可验证的 64 位构件，故不虚构。核心 189 / UI 440；`GOLDEN OK`。

46. 不等长帧查看「确定后无变化」（用户报告）：`BesDoc::OpenFile` 按原版默认把帧长设为 64 比特（见 1），而 `DlgVarFrame` 构造时又用当前帧长自动铺满帧表，因此「确定」应用的是一张与当前等长视图完全相同的 64 比特表——真机截图逐字节相同；且再次打开对话框会用这张等长表覆盖已生效的不等长帧。落点：构造时不再自动铺满，改为空表并提示「按帧长切分或输入前导码扫描」；已有不等长帧时回填 `VariableFrameSet`（起点/默认帧长取首帧），取消保持原状；`应用/确定` 仅在帧表非空时可用；`Reload()` 发 `FramesChanged()`，`MainWindow::DoVarFrame` 在查看模式下实时预览（切分/编辑/扫描即刷新编辑器并清过滤），取消时恢复打开前的帧表、帧模型与过滤状态，确定时走同一应用路径并记录事件；扫描与确定都同时刷新文档与窗口。`test_ui` +12 断言（128 比特切分后行数=帧数、视图可见重绘、重开回填、取消保留、扫描实时预览、取消回滚、确定保留）；核心 189 / UI 452；`GOLDEN OK`。

47. 同类问题巡检：帧过滤「确定后失效」与「不等长帧下过滤无变化」（承 46 的缺陷类别，逐个对话框排查回填/刷新）：排查中「视图未刷新」一类不成立——`MainWindow::UpdateAllViews()` 已遍历所有文档的全部 Tab，`BesDoc::UpdateAllViews()` 只是冗余；确定发现两处，都在帧过滤上。① `DlgFilter` 从不读回已生效的过滤，条件列表每次打开都是空的，用户「确定」后再打开哪怕只改一个判定，返回的空过滤都会把 `st.filter/filterActive/visibleFrames` 清零——表现为「过滤点了确定就没了」。现构造接收 `const FrameFilter*`，回填条件到列表（文案与「添加」共用 `DescribeCondition()`）、`LoadRow(0)` 恢复该行、置 `m_needRun`，再次确定保持条件并重算命中；回填必须在 `OnDataTypeChanged()` 之后，否则判定下拉被复位。② 不等长帧生效时过滤按等长帧扫描（`start + i*frameLen`），得到的帧号与帧表下标不符，「应用」后画面不变。现 `SearchEngine` 增加 `VariableFrameSet` 重载，与等长版共用 `FrameMatches()`（帧长条件比较每帧自身长度、数值条件按帧起点、CRC 与帧尾 2 字节对拍），`DlgFilter` 接收 `const VariableFrameSet*` 并在不等长时走该重载，`Renderer` 新增 `FrameOfRow()` 让 `Compute/Layout/HitTest/YAxisText/RowStartBit/FrameLenOfRow` 的不等长分支统一按可见帧列表映射（此前仅因 `row == frameIdx` 才看起来正确），`MainWindow::DoFilter` 跳转首帧改用帧表 `startBit`。其余对话框（`DlgParamSet`/`DlgSearchFl`/`DlgPosition`/`DlgPrint`/`DlgOutputSet`/`DlgGroupItems`/`DlgSearchUnique`/`DlgComponent`/`DlgComSelect`/`DlgProcess`/`DlgCustomize`/`DlgAbout`）都已正确回填或本就无状态，未改动。`test_core` +7、`test_ui` +4；核心 196 / UI 456；`GOLDEN OK`。


## 7. 仍未对齐的差异（已确认，不属于未对比）

| 差异 | 影响 | 处理建议 |
|---|---|---|
| 构件仅 64 位 | 原版 32 位构件不可用 | 按需求接受 |
| 数据区内直接键入改字节 | 原版只有 TEdit 的十六进制过滤证据 | 无证据，不发明；改用剪贴板 + 撤销/重做 |
| MDI vs Tab | 窗口组织方式不同；原版是 MDI 子窗体 + 停靠站点（组树按站点定位），现版是单一标签组 + 分组树（组内拖放已等价落地，见 6.42）；原版联动只覆盖「同一停靠容器」，现版等同「其它所有 Tab」 | 可保留现代化 Tab；五种窗口排布已按 `[form+0x498]` 落地（见 6.28） |
| 宿主嵌入通道 | 原版 `0x4184BA`（消息码 `0x401`）可由宿主传入非空状态基创建主窗口，此时「构件选项」页显示、各端口独立记忆「从端口」 | 本版无宿主通道；独立启动行为已一致（见 6.37） | 不实现宿主嵌入 |
| 排布不改字节流 | 原版五种排布只重排窗口，视图内容仍是各自的分页帧网格；现版 横排/分组横排 额外把行当作连续数据流绘制 | 记为本版本扩展；若要逐行为一致可改为「仅重排窗口」 |
| 处理/识别菜单本应空白 | 原版 `dxBarSubItem4/6` 的 `ItemLinks` 为空（分析仅「迭代」）；现版用它们承载不等长帧处理/定位/退出查找与 `TProcessForm` 入口 | 保留入口；原版这些功能只通过工具栏按钮或构件对话框到达（见 6.30） |
| 查看菜单的栏控件 | 原版把起点/帧长/步进/显示四个 `TdxBar*` 控件内嵌在查看菜单里（同一组 `ItemLinks` 与格式栏共用） | 已用 `QWidgetAction` 镜像控件补齐（见 6.35）；Qt 无法让一个 widget 同时在工具栏与菜单，故是等价镜像而非同一实例 |
| 打印 | 原版 `PrintBtnClick` 只弹「未完待续...」，`TPrintForm` DFM 未实例化，无打印代码 | 本版提供实际打印 | 记为本版本扩展（见 6.33）；不追求与原版一致 |
| 位图 | 原版下拉框仅 5 项；「位图」按钮不切模式；mode 7 是只能由魔数 `0x1234ABCD` 流恢复的字节网格视图（子格式 Bin/Hex/Text/BMP），正常 UI 不可达 | 下拉框恢复 5 项；位图按钮/菜单选中灰度 Pixel 视图 | 记为本版本扩展（见 6.34）；原版不能启动，无样张可校准 |
| 构件端口按二进制 ABI 直连 | 原版由引擎对象 `[form+0x360]`（`+0x14` 句柄、`+0x18` 退出、`+0x19` 运行）在工作线程里直接调构件 `[form+0x364]` 的 vtable；现版 `bes::FlowEngine` 单线程、选区经 `IFlowPort` 落到临时文件端口 | 数据流管线已等价落地（见 6.27）；命令 100 分派层已落地（见 6.45）；`BesCom.dll` 只导出 4 个符号，记录引擎的二进制布局不可验证，不虚构 ABI；构件窗口托管（`ContainerPanel` + `CreateInstance` 第 4 参数 HWND）因无可验证的 64 位构件仍未接 |

## 8. 验证

```bat
run-tests.bat
=> 核心 196 断言 ALL PASSED
=> UI  456 断言 UI ALL PASSED

tools\golden.bat
=> GOLDEN OK
```

64 位运行：

```text
build\qt-msvc-x64-release\bes.exe
```

结论：**功能矩阵已全部给出对比结论；打开/默认显示/BitChar/Hex 已实际运行对齐，其余差异均为已确认的行为/布局差异，不是未对比项。**
