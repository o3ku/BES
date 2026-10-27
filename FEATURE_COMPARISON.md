# BES 功能对照与完成度

> 基准：`legacy_mfc/`（MFC 重写版）+ `tools/forms.dfm.txt`（BesCom.dll 原始窗体）
> 当前实现：`src/`（Qt5 版）
> 测试：`run-tests.bat`（核心 189 断言 + 界面 426 断言）
> 操作/界面逐项对比：`docs/OPERATION_COMPARISON.md`

## 总览

| 维度 | 完成度 | 说明 |
|---|---:|---|
| 命令/菜单/工具栏骨架 | ~100% | 75 个命令 ID 全部接入；主菜单按 DFM `TdxBarManager` 对齐（见 32）；界面风格互斥；每条工具栏独立显隐，右键/双击打开按钮级定制（增删/排序，见 42），内容与显隐均持久化；格式栏数值框带 SpinEdit 箭头，按钮为图标+文字 |
| 数据/帧/选取引擎 | ~88% | 默认帧长 64/自动可选、二维块选取、不等长帧、过滤映射、选区序列化、字节编辑与撤销栈 |
| 显示/浏览交互 | ~80% | Hex 双 nibble、HexChar、水平/垂直滚动、缩放字体、XTAB/YTAB、书签/光标 |
| 搜索/过滤 | ~80% | 任意长度独特码、去四相模糊、同步列帧长查找（检测行数/最少同步列数）、过滤应用于视图 |
| 对话框/业务窗体 | ~86% | 13 个对话框/窗体；参数设置 4 页（独立启动可见 3 页，构件选项页按原版隐藏；窗口列表逐窗编辑）；关于对话框按原版；打印页范围 |
| 构件插件 | ~70% | 64 位构件 ABI 完整（`RegComInfo` 双缓冲角色、4 参数 `CreateInstance`、实例 vtable 信息块）；构件类型/端口/作者/版本在运行期从实例信息块读取；`ShowModal` 可打开构件界面；32 位构件明确拒绝；数据流管线经临时端口等价落地（不直连构件 vtable） |
| 文档/窗口组织 | ~80% | Tab 多视图、文档生命周期、窗口分组/分组项（组内拖放归组，见 43）、五种窗口排布（分页/横排/竖排/分组横排/分组竖排，全局状态）；仍非原版 MDI |
| 验证/回归 | ~75% | 核心+UI 冒烟+黄金截图；未取得原版运行基准（BesCom.dll 启动器本身有缺陷） |

**结论：作为“可用的 BES 工作副本”约 75%；作为“逐像素/逐行为 1:1 复刻”仍约 60%。**
主要剩余差距是 32 位构件不再支持、原版 MDI/停靠图层（分组树内拖放已等价落地，见 43）、以及无法运行原版导致的黄金行为缺失。

## 逐项对照

| 分类 | 功能 | 原版 | 当前 |
|---|---|---|---|
| 文件 | 新建/打开/保存/另存为 | 有 | ✅ 新建=打开（与 legacy_mfc 一致）；保存=Flush；另存为按选区/全量输出 |
| 文件 | 打印当前文档 | 占位（`PrintBtnClick@0x40D9BC` 仅弹「提示：未完待续...」；`TPrintForm` DFM 未被实例化） | ✅ 本版扩展：十六进制表、整文档打印、自动分页、边距/页范围；打印设置保留在对话框中（见 35） |
| 文件 | 最近打开文件 | 占位 | ✅ QSettings 持久化 8 条（与原版 MaxItemCount=8 一致） |
| 文件 | 关闭/全部关闭/退出 | 有 | ✅ 关闭时释放文档；退出编辑时才按脏标志询问保存（原版 `EditBtnClick` 无关闭询问） |
| 编辑 | 进行编辑（工具栏） | 有（`EditBtn`，bsChecked） | ✅ 文字按钮「进行编辑」，进入=换成可增长内存副本，退出按脏标志弹「文件内容已改变，保存吗？」(Yes/No/Cancel) |
| 编辑 | 剪切/复制/粘贴 | 有（`editIface` +0x28/+0x2C/+0x30） | ✅ 连续选取按整字节复制进剪贴板；剪切=复制+删除、粘贴按插入/改写模式写入；非编辑态下剪切/粘贴禁用（复制仍可用） |
| 编辑 | 撤销/重做 | 有（`editIface` +0x34/+0x38） | ✅ 逐操作撤销栈（改写/插入/删除各自可逆），栈空时按钮禁用；快捷键改为原版的 Ctrl+Z / Shift+Ctrl+Z |
| 编辑 | 连续/块/行/列选取 | 有 | ✅ 二维块、行/列范围；每次拖动独立累积、松开才提交、同按钮再点清空退出；单击已选行/列再点即取消该行/列（见 44）；Shift+导航键扩展与拖出自动滚动（见 40、41） |
| 编辑 | 选取参数读入/存盘 | 有（.slp） | ✅ 原版二进制格式（`slp` + 4 段 × 24 字节记录）；对话框标题/过滤/默认扩展名按原版 |
| 查看 | 放大/缩小/恢复 | 有 | ✅ 格尺寸+绘制字体均缩放；Ctrl+滚轮 |
| 查看 | 界面风格 | 有 | ✅ 传统/VS.NET/Office，QActionGroup 互斥并持久化 |
| 查看 | 标尺/横纵分割线 | 有 | ✅ 上/下标尺按数据列对齐并带 8 比特刻度，数字按实际字宽避让不重叠（见 44）；左侧行号标尺（横排为数据行）；XTAB/YTAB 与数据坐标对齐 |
| 查看 | 分组横排/竖排/标题 | 有 | ✅ 分组排布只同屏当前组的视图，组间边界由分隔条呈现，排布不改 XTAB/YTAB（见 38）；标题为独立开关（默认选中），显示帧号/帧长/起点，可与其他布局叠加 |
| 查看 | 窗口分组 | 有 | ✅ 树形分组、新建/删除/关闭组、双击激活；窗口项可直接拖入分组节点，拖回「数据窗口」移出，右键 Tab 加入仍可用（见 43） |
| 查看 | 工具栏/联动浏览 | 有 | ✅ 每条工具栏独立显隐；联动＝同一容器内其它视图，按「同一移动量」位移各自位置（原版 `0x407B4C`） |
| 查看 | 竖轴 YFN/YFL/YFB | 有 | ✅ |
| 处理 | 分页/横排/竖排 | 有 | ✅ 默认 64 比特帧长（可改自动）+长帧水平平移 |
| 查看 | 参数（`ParamBtn`） | 有，Caption「参数」 | ✅ 参数设置左侧窗口列表逐窗编辑格式与显示参数，可见 3 页（数据格式/显示参数/颜色设置）；「构件选项」页照原版 DFM 构建但隐藏（见 12）；“应用”不关闭。原版把它放在查看菜单（`DataFormatBtn` 实为「倒序」，见 32） |
| 分析 | 迭代(单步) | 有 | ⚠️ 原版 `DiedaiBtnClick` 为空过程（仅 `ret`），本版同样为空占位；工具栏「单步」切换单步/连续模式 |
| 处理 | 不等长帧查看/处理 | 有 | ✅ 帧标记表、切分、前导码检测、预置、二进制/十六进制/字符、误码、去四相、.bnt 输出/读入、中止/进度 |
| 处理 | 定位 | 有 | ✅ 绝对/相对/行列/书签，含“应用”不关闭 |
| 处理 | 独特码查找 | 有 | ✅ 任意长度、误码、去四相模糊、进度/取消、命中列表 |
| 处理 | 帧长查找 | 有 | ✅ 原版算法：候选帧长递增，检测行数内全0/1列达到最少同步列数即命中 |
| 处理 | 退出查找状态 | 有 | ✅ 清除过滤/可见帧 |
| 分析 | 二进制/十六进制/文本/位图/HexChar | 有 | ✅ Hex 每字节 2 位；HexChar 2 位+ASCII；菜单/工具栏互斥勾选；格式栏下拉与原版一致仅 5 项，位图为扩展入口 |
| 分析 | 帧过滤 | 有 | ✅ 按原版三页条件（帧长/数值/CRC）加入列表，多条件相“与”；数值按低字节在前输入；CRC 校验帧尾 |
| 识别 | 数据流/格式输入输出 | TProcessForm 一个窗口 | ✅ `DlgProcess` 复刻行集/按钮状态机/校验/优先级；✅ `bes::FlowEngine` 把选区按块推入构件数据流输入端口，暂停/停止/复位/退出对应命令 3/4/5 与退出请求 |
| 定制 | 构件管理 | 有 | ✅ 按 `TComCustomForm` 复刻：构件列表 + 构件名称（可改，≤32 字）/构件程序文件（只读「文件名: 注册名」）/构件信息（只读多行，运行期取自实例信息块）+ 浏览… + 添加/删除/修改/退出；浏览探测构件 ABI（不登记）→ 多构件时弹构件选择（「找到N个构件」+ 原版端口校验）→ 添加时复制进 `com\` 再注册；删除只反注册不删文件；仅 64 位构件 |
| 查看 | 参数设置 | 有（`ParamBtn`「参数」） | ✅ 入口为 查看→参数 / 格式栏；颜色 9 项（默认方案/方案一）；数值裁剪按原版；「构件选项」的缓存/块/临时目录照原版构建但随该页隐藏 |
| 定制 | 快速执行 | 有 | ✅ 「选择构件」载入临时 DLL → 校验 `RegComNum/RegComInfo/CreateInstance` → 构件列表 → `ShowModal`；缺导出报「不是局级系统构件！」 |
| 定制 | 数据输出端口 | 有 | ✅ 输出设置端口树 + 32 端口单选；无构件端口时回退通用 1..32；写 `%TEMP%/bes_port_N.dat` 并在事件面板提示 |
| 窗口 | 新建窗口/多视图 | 有 | ✅ 同一文档多 tab，共享文档 |
| 窗口 | 分页/横排/竖排/分组横排/分组竖排 | 有（`[form+0x498]` 全局） | ✅ `ViewArea`（标签条 + 分隔窗口）按全局排布重排全部视图；分组排布只同屏当前组 |
| 帮助 | 关于 | 有 | ✅ 按 TAboutBox 尺寸/文案复刻（徽标为简化绘制） |
| 菜单 | 主菜单分组与标题 | 9 个顶层菜单；分组见 `TdxBarManager` DFM | ✅ 顶层、标题与条目分组已按 DFM 对齐（见 32）：定制仅「管理...」「快速执行...」，参数在查看、排布在窗口；原版为空/仅迭代的处理/分析/识别菜单保留本版命令入口并注明 |

## 本轮补齐/修复（P0/P1/P2）

### P0
1. **默认浏览可用**：打开文件默认 `帧长=64`、`比特字符`（与原版一致）；`帧长=0` 表示自动（一屏宽一帧）。
2. **Hex/HexChar 不再丢数据**：每字节显示高/低两位十六进制；HexChar 追加 ASCII。
3. **水平滚动打通**：`horizontalScrollBar` 已连接，`Layout/HitTest` 支持 `hCellOffset`；TAB 跟随吸附。
4. **构件管理崩溃修复**：端口记录不再空指针（该「端口属性表」界面后按反汇编删除，见功能级 30）；`CreateInstance` 按反汇编改为 4 参数；`RegComInfo` 改为 1-based + ANSI + 4 参数。
5. **构件位数**：仅支持 64 位构件；检测到 32 位 DLL 时明确拒绝并提示。
6. **SaveAs/打印页范围**：路径生效；打印按 `起始页/结束页`。
7. **搜索**：移除 32 位截断；实现去四相模糊；帧长查找按原版同步列算法重写（检测行数/最少同步列数，找到即应用）。

### P1
- 不等长帧集合进入 `DocState` 并参与渲染。
- 参数设置补颜色页/构件选项页，颜色真实可编辑。
- 二维块选取、行/列范围、`SelectedBitsEx` 统计修正。
- 过滤结果进入视图（可见帧映射）。
- 书签/标记/光标/起点绘制。
- 文档多视图引用计数、关闭释放。

### P2
- 新增 `DlgComSelect`（构件选择）、`DlgProgress`（进度/取消）、`DlgGroupItems`（分组项）。
- 分组树、工具栏独立显隐、界面风格互斥、设置持久化。
- `test_ui`（offscreen）+ `test_plugin`（64 位）+ `tools/golden.bat` 黄金截图。

## 本轮继续补齐（功能级）

1. **打印**（原版为占位，见 35；本版扩展）：打印整个文档（起点到文件尾）；左右/上下边距按毫米输入（默认 10mm）并按打印机 DPI 换算，页面范围支持 `n` 或 `n-m`（空=全部），保留打印机/纸张设置与纸张标签。
2. **帧过滤**：按 `TFilterForm` 重写为三页条件（帧长/数值 1/8/16/32 位/CRC）+ 条件列表（添加/更改/删除/清空、应用）；多条件相“与”；数值要求从低字节到高字节输入；CRC 固定校验帧尾最后 2 字节（CCITT-FALSE，高字节在前）。
3. **端口输出**：构件数据输出端口进入输出设置端口树（右侧 32 个端口单选，树/单选联动）；输出写入临时文件并在事件面板提示路径；仅持久化构件 DLL 路径，端口值运行期从实例信息块读取。
4. **不等长帧处理**：按原始 `NetDataForm` 补齐预置、前导码、二进制/十六进制/字符、允许误码、去除四相模糊、输出 `.bnt`、开始/中止/进度。
5. **命令语义**：参数（原 `ParamBtn`）打开参数设置；显示模式/纵轴菜单互斥勾选；清除当前/全部选取分离；事件面板可显隐；定位“应用”不关闭。
6. **回归**：新增不等长帧处理与构件注册持久化 UI 测试。
7. **构件 ShowModal**：按确认的 ABI 调用 `ShowModal(const char* file)`，路径以 ANSI(GBK) 传入；识别/数据流处理选择构件后直接打开构件界面，无该导出时给出明确提示。（原记为「串口管理」，该菜单项不存在，见 32）
8. **输出端口编号**：输出设置端口树/单选保留构件的真实端口号（而非列表序号），未注册端口禁用；端口数据写入对应 `bes_port_N.dat`。
9. **BNT 读入**：不等长帧对话框支持“读入帧表…”，解析 `BES-BNT 1.0`、格式/前导码/误码/去模糊及 `Frame=起点,长度`，并按当前文件长度裁剪越界帧。
10. **状态栏/最近文件**：状态栏按原版三面板与标记/选取文本复刻（见 34）；最近文件上限改为 8 条，与原版一致。
11. **下标尺**：数据区底部增加列坐标标尺，与上标尺、绘制/命中/滚动范围统一。
12. **参数设置（TParamSetForm）**：按原版重写为 4 页 + 左侧窗口列表，逐窗口编辑格式（5 种模式、起点/帧长/步进/取反/高位在前）与显示参数（X/Y 步进、TAB、标尺、TAB 跟随）；颜色 9 项及默认方案/方案一；构件选项含缓存/块大小裁剪、临时目录可写检测与可用空间；“应用”不关闭并立即生效，“确定”仅在修改后应用；帧长/步进/缓存/块等数值按原版规则裁剪。
    - 状态端口语义按反汇编还原：窗口端口号非 0 时启用“从对应状态端口读取”，勾选值存于按端口号索引的 32 字节数组（同端口窗口共享）；从端口取格式时“格式设置”组禁用。
    - 独立启动时原版以 **nil 状态基**创建主窗口（`0x401F08 push 0` → `0x406DB8` 存入 `[main+0x4A0]`），参数设置继承该 nil（`0x4196E7` 存入 `[form+0x3E8]`）：`0x419E64` 判空后隐藏“构件选项”页，且所有窗口端口号为 0、只能“自行设置”。本版文件窗口同样无端口号，故行为一致，并按此隐藏“构件选项”页（页面仍按 DFM 构建，缓存/块/临时目录继续生效）。
    - 非独立宿主路径（`0x4184BA`，消息码 `0x401`）会传入非空状态基，此时该页显示、各端口独立记忆“从端口”选择；本版未接入该宿主通道。
13. **标题**：按原版改为独立开关（不再作为互斥布局），默认选中；与分页/横排/竖排/分组布局叠加，绘制帧号/帧长/起点。
14. **关于/选取文件**：关于对话框按 `TAboutBox` 复刻 332×256 布局与文案（徽标为简化绘制）；选取参数读入/存盘对话框标题、`.slp` 过滤与默认扩展名对齐原版。
15. **格式栏 SpinEdit**：起点/帧长/步进的文本框补一对上下箭头，等效原版 `TdxBarSpinEdit`；按当前显示单位 ±1（0 处钳制），文本框仍保留 64 位十进制输入；黄金基线更新。
16. **状态栏插入指示**：按原版右侧 30px 面板补「插入/改写」指示，默认插入，Insert 键切换（现驱动粘贴方式，见 23）；黄金基线更新。
17. **标尺补齐**：横排/分组横排也绘制左侧行号标尺（YFN=行号、YFL=行宽、YFB=行首比特）；上/下标尺在 8 比特边界画短刻度（比特位图 8 格一刻、字节模式每格一刻，随模式/缩放变化）；黄金基线更新。

18. **格式栏按钮外观**：按原版 DFM（按钮同时带 `ImageIndex` 与 `Caption`）改为图在上、文字在下的 `ToolButtonTextUnderIcon`；工具栏文字去掉菜单助记符与省略号；参数按钮沿用原版标题“参数”，放大/缩小/恢复按原版顺序；`test_ui` 链接图标资源以便校验；黄金基线更新。

19. **倒序 = 高位在前(0-7)**：按原版 DFM 证据（`DataFormatBtn` 标题“倒序”、提示“选中为高位在前（0-7）未选为低位在前（7-0）”、`bsChecked`；参数设置 `ReverseCheckBox` 同为“高位在前(0-7)”），合并为单一 `highBitFirst` 选项：工具栏按钮与参数页复选框共享状态；关闭时在**每字节内**反转比特（比特模式下比特格按 7-0 取位，不跨字节），不是整帧倒序也不是 nibble 交换；取反仍在其后应用；黄金基线更新（按钮默认呈选中态）。

20. **单步与迭代分离**：原版工具栏 `StepBtn` 标题“单步”、提示“切换单步和连续状态 / 只对数据流输入有效”、`bsChecked`，而“迭代”是另一个按钮（`DiedaiBtn`）。现拆成两个命令：工具栏「单步」可勾选，仅切换文档 `singleStep` 模式（无数据流时不移动视图）；「迭代」保留在“处理”菜单，但按原版 `DiedaiBtnClick`（反汇编为单个 `ret`）作为空占位，不再推进起点；工具栏不再错误地显示“迭代”。

21. **快速执行**：此前“定制 → 快速执行”被接到刷新视图上。按原版 `FastExecuteBtnClick` 重写为：文件对话框（标题「选择构件」、过滤「局级系统构件(*.dll)」）→ 用**临时**宿主 `LoadLibrary` 该 DLL 并校验 `RegComNum`/`RegComInfo`/`CreateInstance`（载入失败或缺导出统一报「错误」+「不是局级系统构件！」，构件数为 0 报「没有可用构件！」）→ `DlgComSelect` 列出该 DLL 的构件 → 对所选构件调用 `ShowModal(当前数据文件)`。临时宿主不写入持久构件注册表，与原版一致；`ComponentHost` 新增 `HasComponentAbi()` 用于导出校验。

22. **`.slp` 选取参数文件改为原版二进制格式**：反汇编 `BesCom.dll` 的 `Load@0x440624`/`Save@0x4407AC`（同一 VMT@0x5DDBF0 的 +04/+08 槽）确认：文件以**裸 3 字节 ASCII `slp`** 开头（无前导长度、无版本号、无 `'BES'/'BESFORMAT'`——那两个串属于读写 `bes.cfg` 的 profile 代码），随后固定 4 个段，每段为 `Int32 记录数` + 记录数 × **24 字节**（小端），记录统一为 `{Int64 p1; Int32 c1; Int64 p2; Int32 c2}`；读侧逐字节比较 `s/l/p`，不符即报「不是选取参数文件！」。段序当时按显示串（`数×数`）推断为 行选取 / 块选取（两个「行+列」角标，行号 64 位、列 32 位，与「第%Ld行」一致）/ 连续选取（两个比特位置）/ 列选取（`Int32` 起止闭区间），其中前两段的归属与「段 2 的 +0x10..17、段 3 的两个 Int64 半边」语义当时未决（见 25，已按证据纠正并落定）；书签/标记/聚焦不写入 `.slp`（相关文案只用于事件窗）。`SelectionSet` 新增 `SaveSlp()/LoadSlp()`，Qt 侧存盘/读入改走二进制（原 `Serialize/Deserialize` 文本格式保留给 `legacy_mfc`）；`test_core` 增加 13 条格式断言（魔数、段尺寸、记录内字段偏移、四类选区往返、魔数错误与截断拒绝）。

23. **编辑模式（工具栏「进行编辑」+ 剪切/复制/粘贴/撤销/重做）**：反汇编 `EditBtnClick@0x40B724` 确认——按钮是 `bsChecked` 文字按钮（DFM 标题/提示均为「进行编辑」），进入编辑**无任何前置校验**（只调 `editIface vtbl[+0x24](obj,1)` 置引擎编辑态并禁用一组控件），退出时先看后台任务标志 `[engine+0xc]`，再按脏标志 `[engine+0x1a4]` 弹「文件内容已改变，保存吗？」(Yes/No/Cancel)：Yes 走 `SaveModified@0x40B93C`（取 `[engine+0x10]` 原路径，`GetFileAttributes` 判只读→「文件不可写！」，否则**按 500000 字节分块**原位回写），No 直接退出（脏缓冲保留，之后仍可保存），Cancel 把按钮弹回按下态。剪切/复制/粘贴/撤销/重做分别是 `editIface vtbl[+0x28/+0x2C/+0x30/+0x34/+0x38]`（`CutBtnClick@0x40B8DC` 等，均非空壳），DFM 里重做快捷键是 **Shift+Ctrl+Z**。实现落点：`FileMap` 增加可增长内存副本方式打开（`OpenEditable/InsertBytes/DeleteBytes/Reload`，映射视图无法改长度，编辑态下 `Flush` 按 500000 字节分块回写）；新增 `src/core/Editor.h/.cpp` 的 `BinaryEditor`（改写/插入/删除 + 逐操作撤销栈 + 脏标志 + 保存/丢弃）；`BesDoc` 提供 `EnterEdit/LeaveEdit/SaveEdits`（进入后再次进入不重读磁盘，避免「不保存退出」留下的改动被覆盖）；`MainWindow` 提供 `ToggleEditMode/LeaveEditMode/DoCut/DoCopy/DoPaste/DoUndo/DoRedo`，剪贴板用自有 MIME `application/x-bes-bytes` 承载数据字节（同时给出文本便于外部查看），粘贴按状态栏插入/改写决定覆盖还是后移；非编辑态下剪切/粘贴/撤销/重做禁用。`test_core` +26 条（缓冲可增长、三类操作与撤销/重做、落盘、只读拒绝、映射模式下插入/删除被拒、尾部越界改写裁剪），`test_ui` +41 条（按钮文案/可勾选、编辑态与按钮可用性、复制字节、改写与插入粘贴、撤销/重做可用性、取消保持按下、保存退出回写内容逐字节比对、只读文件报「文件不可写！」、内容复原）；黄金基线更新（快捷工具栏新增「进行编辑」文字按钮）。

    **未复刻**：网格内直接键入十六进制改字节——`EditKeyPress@0x40FFF0` 只是「文件位置/行/缓冲大小」三个 `TEdit` 的十六进制过滤，与字节编辑无关，故未虚构键入改字节行为（编辑态禁用的控件组见 24，已补齐）。

24. **编辑态禁用的控件补齐**：`[form+0x45c]` 通过 `TMainForm` 的 RTTI 字段表定位到 **`ViewModeComboBox`**——本 DLL 的 VMT 偏移为 类名`-44`、字段表`-56`、方法表`-52`，字段表按 `{Word 数量=112; Int32 属性表指针; 112 × {Int32 Offset; Int16 类型索引; Byte 名字长度; 名字}}` 排布，正好结束于方法表地址（用 `+0x3E8 = EditBtn`、`+0x410 = StepBtn` 校验无错位）。该控件是格式栏 `TdxBarCombo`（DFM `Caption = "显示"`、`Hint = "显示模式"`、设计期 `Enabled = False`、`OnChange = DrawContextChanged`、栏内 `UserWidth = 114`）。`EditBtnClick@0x40B724` 进入编辑时经布尔 Enabled 设置器 `0x509AA4` 置 `dl=0`，成功退出路径置 `dl=1`，退出被拒时用 `0x50AA0C` 把 `EditBtn` 重新按下并保持禁用；`0x40A3D8`（配置记录回填，`esi := 记录 + 0x180`）另有 `[form+0x45c].Enabled := ([esi+0x18] = 0)` 的集中再断言。实现落点：`MainWindow::UpdateMenuState()` 末尾 `m_cboViewMode->setEnabled(has && !editing)`（无文档时与设计期一致为禁用），格式栏「显示」下拉框补 `显示模式` 提示，`test_ui` +5 条断言（非编辑态可用、进入编辑禁用、取消退出仍禁用、保存退出/离开编辑态恢复）。同场解码 `SaveModified@0x40B93C` 的三条失败提示（「文件不可写！」「后台任务运行时不能保存文件！」「目标磁盘空间不够！」），后两条依赖后台任务标志与短写判定，当前路径不可达，仅记录。

25. **`.slp` 段序纠正 + 未决字段落定**（改动 22 的推断）：`Load@0x440624`/`Save@0x4407AC` 与选区语义无关——对象 `+4/+8/+0xC/+10` 是四个 `TList`，按字段地址顺序写「Int32 记录数 + 记录数 × 24 字节」，段含义只能由消费方确定。证据两段：① 记录投递方 `0x444924` 按 `[[self+0x18]+0]`（选取模式）分支，模式 0 → `[self+4]`（随后 `Sort`，比较器 `0x4033AC`）、1 → `[self+8]`、2 → `[self+0xC]`、3 → `[self+0x10]`，模式 2/3 还会合并相邻/重叠区间并在必要时拆分；② 该「模式」字段写的就是工具栏按钮 `Tag`——`SelectBtnClick@0x40AC88` 读 `[Sender+0x83]`（Down 态）后把 `[Sender+0xC]`（Tag；未按下传 `-1`）传入选区对象（`[[form+0x494]+0xC]+0x2E0]+0x1E4`），DFM 中 `ContinueSelectBtn` 无 Tag（⇒0）、`RectSelectBtn Tag=1`、`RowSelectBtn Tag=2`、`ColSelectBtn Tag=3`，四者共用 `OnClick=SelectBtnClick`/`GroupIndex=2`/`bsChecked`/`AllowAllUp`。⇒ **段序 = 连续 / 块 / 行 / 列**，原实现把段 0 与段 2 写反。同时落定 22 记录的未决字段：块选取对两组「行+列」各取 min/max 归一化；行选取只用 Int64 半边（min@+0、max@+0xC），两个 Int32 恒 0；列选取只用 Int32 半边（min@+8、max@+0x14），两个 Int64 恒 0（`[self+0x68]=-1` 时收尾到最后一列）；连续段逐字拷贝 p1/c1/p2/c2，两个 Int32 是随选区携带的待定参数，本模型写 0。原版行/列列表存区间，本模型每行一条 `row1=row2` 记录、读侧统一按 min..max 展开，与原程序双向可互换。落点：`Selection.cpp` 的 `SaveSlp()` 段序与 `LoadSlp()` switch 重映射（case 0 连续，多段取并集；1 块；2 行；3 列）、`Selection.h` 格式注释、`test_core` 的 `.slp` 断言改为校验新段序（偏移 3/31/59/111 的段计数 = 1/1/2/1、连续记录 p1@7=100/p2@19=200、首条行记录 p1@63=7），核心增至 151 断言；`QT_NOTES.md` 的 `.slp` 一节同步改写，`docs/OPERATION_COMPARISON.md` 的 21 与 §1 行「选取参数读入/存盘」不再标注为推断。

26. **联动浏览的范围与语义补齐**（此前只记为「原版具体联动范围未知」）：开关控件由 `TMainForm` RTTI 字段表定位为 `+0x3C8` 的 **`LockScrollBtn`**（TdxBarButton，DFM `Caption`/`Hint`「联动浏览」、`Style = bsChecked`、快捷键约 `Ctrl+A`、**无 OnClick**，`bsChecked` 自翻 Down 态，状态读 `[obj+0x83]`，与 `SelectBtnClick@0x40AC88` 读 `Sender+0x83` 同一偏移）。同步对象收集器 `0x407B4C(当前视图, outList)`：`Clear` → `[[form+0x3C8]+0x83] = 0` 时直接返回 → 取 `当前视图.[+0x24].vtbl[+0x108]()`（所属停靠站点）并要求 `is TdxSideContainerDockSite` → 遍历 items 收 `item.[+0xC]`，跳过 nil 与触发者 ⇒ **联动范围 = 同一停靠容器内的其它视图，不含自己、不跨容器**；`0x407AF8` 在 `[form+0x2F4]`(GroupTreeView)`. [+0x254]` 按 `item.[+0xC]` 比对，只用于定位视图、不扩大范围。三个触发点（滚轮 `0x405C34`，行数 `-WheelDelta div 120`；滚动条通知 `0x4061C6`，按 `TScrollCode` 0..5 换算，源基准 `rec+0x160`/其它基准 `rec+0x15C`；跳转表 `0x4064A9` 的 ±1/3/5）一律「自己的位置 + 同一移动量」（Int64 加 `0x5C90B0`、存 `0x56EF8C`，写回 `rec.[+0x2E0].[+0x1E4].vtbl[+8]`），即原版是**相对位移**而非绝对对齐。落点：新增 `BesView::LinkShift(dRow, dCol)`，`OnVScroll/OnHScroll` 记录前值并把差值交给它，`LinkShift` 在 `MainWindow::LinkBrowse()` 为真时遍历 `centralWidget()` 的 `QTabWidget`、跳过自己，把增量加到各视图自己的 `ScrollRow()/HOffset()`（钳到 ≥0）后经 `SetScrollRowQuiet/SetHOffsetQuiet` 静默写入；广播绝对位置的 `SetScrollPosQuiet` 删除（本版本只有一个标签组，「同一停靠容器」等价于「其它所有 Tab」，见「明确剩余差距」）。`test_ui` +10 条断言（两视图均有滚动范围、开启后其它视图按同一移动量位移且不跳到源视图绝对行、保持各自基准、关闭后不跟随、反向带动、开关恢复初值），界面增至 211 断言。

27. **构件数据流处理窗体补齐**（此前记为「`TProcessForm` 当前为简化版、数据端口管线未知」）：`TProcessForm`（VMT `0x5D86A0`、实例 `0x3A4`、字段表 `0x5D8784` 28 项、方法表 `0x5D896F` 16 项）就是「识别 → 数据流输入 / 数据格式输入 / 数据流输出」三项共用的窗口——DFM（`0x22B9F8`）的行分类为 数据输入 `DataInRow` / 数据输出 `DataOutRow` / 构件参数 `ComParamRow` / 构件自定义 `AdvanceRow`，「数据格式」即其中的 `DataTypeRow`，故三个命令现统一走 `MainWindow::DoProcessFlow()` → `DlgProcess`。界面按 DFM：434×448、`BorderWidth 2`，按钮行 开始/暂停/停止/复位/退出（48×22，`暂停` 为 `AllowAllUp + GroupIndex 1` 的开关 ⇒ `setCheckable(true)`），选项行 优先级 + 完成后退出 + 自动打开输出文件（后两者 DFM 均 `Checked`），行 输入选择/反选/选取参数/数据类型/输出标志/输出文件/自定义参数/自定义显示，底部 `InfoLabel` + 进度条（`Min 0 / Max 256`）。运行期默认值来自 `FormCreate@0x413A08`：`PriorityComboBox.ItemIndex := 3`（`0x413A8F`，DFM 未带 ItemIndex）、`AutoQuitCheckBox.Checked := [ctx+0x1A]`（`0x413C7B`）、`OutFlagRow` 仅在 `[ctx+0x7C] = 0` 时可用（`0x413AF7`）。启用规则来自 `SelectModeRowChange@0x4163B0`：反选仅 mode ∈ {全部数据, 块选取} 可用，可用时把值清空（此前误把 `[[ctx+0x74]+4] = 7` 当作「列选取且构件类型 7」，那其实是另一上下文对象的字段，与本 ABI 的 1 基构件类型 1..4 无关，已删除该映射，见 31）；选取参数 mode = 0 时可用并写「全部数据」，否则写段名（`[form+0x340]` 列表）并禁用，仅当行文本（`0x4A2210` 写 `+0x88`）不等于「不接受手工输入」(`0x5D7696`，等值比较 `0x5D0B60`) 时重新可用。状态机：空闲 `0x415650`（仅开始可用 + 进度条 `Visible = False`，`0x42A3E4`）→ `RunBtnClick` 尾部 `0x416198`（禁开始、启暂停/停止、复位取 `[form+0x344]`、`[ctx+0x19] = 1`）→ `PauseBtnClick 0x4161F8` 读 `Down`(`+0x130`) 发命令 3/4 → `StopBtnClick 0x416250` 禁暂停/停止/复位并清运行标志 → `ResetBtnClick 0x41629C` 发命令 5 → `CancelBtnClick 0x4162AC` 四键全禁 + 清运行 + 置退出请求（`FormClose` 先调它）。校验（`0x415C15..0x415C70`）：mode ≠ 0 且选取参数为空 → InfoLabel 红字「选取参数不能为空！」(`0x5D7670`) 并中止；另有「: 文件名不能为空！」(`0x5D7683`)。优先级由 `PriorityComboBoxChange@0x416590` 从 `0x5D7600` 的 `{-15,-2,-1,0,+1}` 表按 ItemIndex 取值，`DlgProcess::priorityValue()` 返回同一组值。`DlgProgress` 一并按 `TProgressForm` 收紧：客户区 302×86、`ProgressInfo` (9,22,201,13)、进度条 (8,48,202,17) 固定 0..256（原实现自行放大标度）、取消 (217,47,75,23) 默认按钮、`poDesktopCenter`、关窗等同取消。`test_ui` +37 条断言，界面增至 248 断言；核心 151 断言、`GOLDEN OK`。**未做**（本轮已落地，见 28）：`[form+0x360]` 引擎对象与 `[form+0x364]` 构件实例之间的数据推送（命令 1/3/4/5/100、`ContainerPanel` 自定义视图尺寸同步 `com.vtbl[+0x34](com, 100, &rect, 16)`）。

28. **构件数据端口管线落地（新增 `src/core/FlowEngine.h/.cpp`）**：原版把搬运放在主窗体持有的数据流上下文里——`FormCreate@0x413844` 只把上下文句柄存进 `[form+0x360]`（`@0x410689` 由 `TMainForm` 传入，`@0x41067B` 里 `[ctx+0x8c] := 0x10000` 为块上限），`[form+0x364]` 置 nil、`[form+0x344] := 1`（「复位」能力，故现版默认 `m_resetSupported = true`）、`[form+0x358] := 100`；`RunBtnClick` 尾部 `@0x4161D7` 也只是 `mov byte [eax+0x19],1` 置运行标志，暂停走 `SuspendThread([ctx+0x14])`（`@0x416213`）+ 构件命令 3（`@0x416228`）、继续走命令 4（`@0x41623C`）+ `ResumeThread`（`@0x416248`），复位走命令 5（`@0x4162A7`），退出把 `[ctx+0x18]` 置 1。构件侧接口按 vtable 槽调用：命令 `+0x28`、优先级 `+0x30`（`@0x4165E0`）、自定义参数 `+0x18`（`@0x41714A`/`@0x4174A9`）、自定义显示 `+0x34`（`@0x41660E` 传 1/2，`@0x416133` 传 `101,0,0`）。`BesCom.dll` 只导出 4 个符号（名字串在文件 `0x1D3325`/`0x1DA61D`，`.edata` 在 `0x206112`），上下文/引擎类并不在 DLL 里，其记录二进制 ABI 无从验证，故本版本不虚构该 ABI，改在既有端口接缝上等价落地：`bes::IFlowPort`（数据流输入端口）↔ `MainWindow::FlowInputFile`（临时文件承载，与「输出到端口」的 `TempFilePort` 对称）。`bes::FlowEngine` 用与 `OutputWriter::emit` 相同的比特→字节换算，把 `FlowParams`（`mode/反选/输出标志/数据类型/选取参数/优先级` + `[ctx+0x8c]` 块上限）解析出的连续/块/行/列选区合并为字节区间、按块推送，块间回调驱动进度与事件循环（原版靠工作线程 + `SuspendThread`，本版本单线程 + 显式泵），`Pause/Resume/Stop/Reset/RequestExit` 分别对应命令 3/4/5 与 `[ctx+0x18]`；端口缺失报「没有可用的数据流输入端口！」（「不支持多个数据流输入端口」「不支持数据流输入端口数目可变！」等前置校验已在构件选择时按原版拦截，见 31），空数据报「数据长度为0！」，写失败报「端口输入失败！」。`MainWindow::DoProcessFlow` 绑定 `setSource(文档映射, 选区, 帧模型, 每格比特)` 与端口，关闭后在事件面板回显「已输入 N 字节（端口文件 …）」。`test_core` +20 条断言（三类端口校验、全量推送、块上限切分 11 块/末块 24、行/块/列/连续选区、反选补集、比特模式、停止保留进度、暂停后继续不丢数据、命令 3/4/5、退出中断、写失败），核心增至 171 断言；`test_ui` +6 条（复位能力开关两个方向、「开始」经引擎把 256 字节推入端口、`SelectMode` 映射），界面增至 254 断言；`GOLDEN OK`。

29. **窗口排布（分页/横排/竖排/分组横排/分组竖排）按反汇编补齐**：此前把 `bes::ViewLayout` 当作「当前文档的绘制方式」，反汇编 `BesCom.dll` 的主窗体确认它是**全局窗口排布状态** `[form+0x498]`——`PageBtnClick@0x408B20` 写 0（只刷新，不重新插入：恢复单页），`HorizBtnClick@0x408B94` 写 1 并把每个站点承载的控件 `[item+0xC]` 以 `cl=3` 重新插回根面板 `[form+0x2E4]`（`0x544BC8(eax=控件, edx=根面板, cl=对齐, 栈上索引=-1)`），`VertBtnClick@0x408C24` 写 2、`cl=2`，`PageHorizBtnClick@0x408CB4` 写 3（组内首项 `cl=1` 进根面板，其后各子项相对前一项 `cl=5` 链式插入；子项数 `0x426BB0`、取子项 `0x4267D8`、单项判定 `0x4265A0`），`PageVertBtnClick@0x408E2C` 写 4（链上的 `cl=5` 换成 `cl=4`）。站点集合取自 `[form+0x2F4]`(GroupTreeView)`.+0x254`（`0x426F54` 计数、`0x4273BC` 取项），`cl` 即 Delphi `TAlign`（None0/Left1/Top2/Client3/Bottom4/Right5）。五个处理函数都遍历**全部**站点 ⇒ 排布作用于所有视图、全局只有一份，切文档/切标签不改变；原版也不靠它改字节流。落点：新增 `src/app/ViewArea.h/.cpp` 取代中心控件 `QTabWidget`（`QTabBar` + `QSplitter`，手柄 4px、子视图不可折叠）：`Paged` 只把当前视图 `setVisible(true)`，其余视图**仍留在 splitter 里**（移出 splitter 会丢几何尺寸，联动浏览按位移同步时基准就错了——这是本轮踩到的坑），其它四种按 `setShownViews()` 决定可见集合并切换分隔方向（竖排/分组竖排 = 纵向）。`MainWindow` 增加全局 `m_viewLayout` 与 `SetViewLayout/ApplyArrangement/VisibleViewCount`（对应 `[form+0x498]`），一次把排布写进所有文档的 `params.layout` 再重排；分组排布的可见集合取当前标签所在组，无分组时回退为全部视图；`Actions.cpp` 五个命令收敛为 `SetViewLayout(...)`，`OnTabChanged/CloseTab` 调用 `ApplyArrangement()`，`BesView::LinkShift` 改遍历 `ViewArea`；新增私有 `ApplyLayoutTo(BesDoc*)`，`OpenDocumentFile` 建视图时也把当前全局排布写给新文档（全局字段只有一份，新开窗口不该回到分页）。`ViewLayout` 枚举值刻意与 `[form+0x498]` 一一对应（0..4），`test_core` +1 条断言钉住编码与中文模式名；`test_ui` +9 条（默认分页只有一个视图同屏、横排/竖排切换 `QSplitter` 方向与可见集合、分组横排/分组竖排只同屏当前组、排布切换把显示模式一并写进全部视图、回到分页后其余视图不再同屏），核心增至 172 断言、界面增至 263 断言，`GOLDEN OK`（默认排布未变，基线不变）。同场修掉黄金截图的一个不稳定因素：进程没有前台焦点时 Qt 认为窗口 Inactive，样式按 `QStyle::State_Active` 取调色板 Inactive 组，菜单文字变成灰 `(160,160,160)`，同一画面会有两种哈希；`QApplication::setActiveWindow()` 与 `QPalette::setCurrentColorGroup()` 都压不住（前者被平台的激活状态覆盖，后者被样式重新判定），截图模式改为把 Active 组整组复制进 Inactive 组。**本版本扩展**：渲染器仍让 横排/分组横排 把行当作连续数据流（原版排布不改字节流），已在「明确剩余差距」标注。

30. **定制构件管理（`TComCustomForm`）按反汇编重建**：此前该对话框是自创的「可编辑端口属性表 + 构件类型下拉」。证据：DFM 资源 `TPF0 @ 0x213EAC` 的 `ComCustomForm: TComCustomForm`（`BorderStyle = bsSingle`、`BorderIcons = [biSystemMenu]`、客户区 468×312）与 RTTI 字段表（本 DLL 的 VMT 偏移：类名 `-44`、实例大小 `-40`、父类 `-36`、字段表 `-56`、方法表 `-52`）给出 `+0x2D0 ComListBox`(11,13,136,285)、`+0x2D4 GroupBox1「构件属性」`(153,11,308,257)、`+0x2DC ComNameEdit`(8,39,216,20，`MaxLength = 32`，唯一可编辑项)、`+0x2E4 ComFileEdit`(8,83,266,20，`ReadOnly`)、`+0x2EC ComInfoMemo`(8,125,289,124，`ReadOnly`)、`+0x2F0 BrowseBtn`(275,83,26,21，标题「…」)、`+0x2F4 AddBtn`/`+0x2F8 DelBtn`/`+0x2FC QuitBtn`/`+0x300 EditBtn`（添加 151,275｜删除 229,275｜修改 308,275｜退出 386,275，前三项初始 `Enabled = False`）、`+0x304 OpenDialog`（`Title = 选择构件程序文件`）、私有 `+0x308/+0x30C` = 待「添加」的源路径与 `com\` 目标路径、`+0x310` 字节标志（已解码为「数据源构件」标志，见 31）。处理函数：入口 `MainComManageClick@0x40D3A0`，`FormCreate@0x436470`（列表非空即选中第 0 项并走一次 `ComListBoxClick@0x436568`）、`BrowseBtnClick@0x4349C8`、`AddBtnClick@0x435EC0`、`DelBtnClick@0x436A3C`、`EditBtnClick@0x436B80`、`ComInfoText@0x435120`。`ComFileEdit` 的文本是 `ExtractFileName(路径)`(`0x5BB254`) + `": "` + 注册名（`_LStrCat3@0x5D0AD4`；注册名即 `RegComInfo` 的第 2 缓冲区、`CreateInstance` 的入参，此前误记为完整路径，见 31）；`ComInfoText` 的标签是镜像里 `0x5DB0BD..0x5DB12B` 连续存放的常量串（自带冒号+空格）：`作者: `/`版本: `/`简要说明: `/`构件类型: `/`物理端口数量: `，其后每端口 5 行 `物理端口 ID:`（此标签无尾随空格）/`端口数据类型: `/`端口类型: `/`缺省端口数量: `/`最大端口数量: `；类型名取自 `0x5DAFF8`（构件 4 类：普通构件/数据源构件/通用显示构件/探头构件）与 `0x5DB008`（端口 4 类：数据流输入/数据流输出/状态输入/状态输出）两张 PChar 表 ⇒ **原版既没有可编辑端口表，也没有构件类型下拉**。

    浏览流程为 `LoadLibrary`(`0x5D1520`) → `GetProcAddress`(`0x5D146c`) 取 `RegComNum`/`RegComInfo`/`CreateInstance` → `RegComInfo(1..n)` 枚举 → `FreeLibrary`(`0x5D13EE`)，即只探测不登记；失败按原口径弹「错误」框 +「不是局级系统构件！」（载入失败/缺导出）或「没有可用构件！」（`RegComNum` 失败或为 0）；探测到多个构件时弹 `ComSelectForm: TComSelectForm`（`TPF0 @ 0x21455C`，`BorderStyle = bsDialog`、`Position = poScreenCenter`、308×211：`Label1`(15,20) + `ListBox1`(13,39,196,157，`ItemHeight = 16`、`lbOwnerDrawFixed`、`OnDblClick = OKBtnClick`) + `OKBtn「确定」`(218,39,75,23，默认按钮) + `Button2「取消」`(218,67,75,23)）；非 `.dll` 文件走原版另一分支——名称取文件名、信息清空、直接允许「添加」。`添加`(`AddBtnClick`) 把浏览选中的文件复制进 `<exe>\com\` 后登记（复制失败报「复制构件文件失败！（错误码 N）」），成功写事件表（该行文本为 `'Com'`，现版事件面板显示构件名）；`删除`(`DelBtnClick`) 只反注册、**不删磁盘文件**；`修改`(`EditBtnClick`) 提交编辑后的构件名称；`退出` = `mrCancel`。

    落点：`ComponentHost::PeekAbi()`（探测不登记的 `ProbeAbi`，含 32 位前置拒绝）与 `RegisterDllCopied()`、`DlgComponent`/`DlgComSelect` 按上述 DFM 几何与状态机重写、`MainWindow::DoComponentManage()` 经 `ComponentRegistered` 信号回显事件；构件名称/作者/版本/简要说明/类型/端口均运行期取自实例信息块（见 31，此前误读为 DLL 版本资源，`version.lib` 已移除）。`test_ui` +19 条断言（客户区 468×312、九个 DFM 对象名齐全、`FormCreate` 选中首项、`MaxLength = 32`、名称可编辑而文件/信息只读、「文件名: 注册名」、memo 含 `构件类型: `/`物理端口数量: `、添加/删除/修改启用矩阵、`PeekAbi` 枚举但不注册、对非构件二进制报「不是局级系统构件！」、删除后 DLL 仍在磁盘上、空列表三键禁用、退出 = `QDialog::Rejected`），界面增至 282 断言，核心 172 断言不变；`GOLDEN OK`（本轮进一步增至 299，见 31）。

    **同时纠正旧结论**：`ANALYSIS.md` 把 `TAgentForm` 当作「定制构件管理」、把 `TComCustomForm`/`TComSelectForm` 当作「串口（COM）自定义与选择」是错的——`AgentForm: TAgentForm`（`TPF0 @ 0x213DB0`，`AgentUnit`，359×235）是**不含任何控件的空窗体**，而带 `ComListBox/ComNameEdit/ComFileEdit/ComInfoMemo/添加/删除/修改` 的正是 `TComCustomForm`；「定制」菜单里也不存在「串口管理」项（两份对照表的该行已删除）。

    **已解码**（见 31）：`Self+0x310` 与构件记录 `+0x194` 是「数据源构件」字节标志（`info.comType == 2`），由选择窗 `OKBtnClick` 写回、`0x40D6B0` 消费；现版以实例信息块的 `comType` 等价表达。「修改」只改名、不重新复制 DLL——现版宿主按路径索引已载入模块，改指向会让 `CreateInstance` 失效。构件记录本身在原版是固定 `0x19C` 字节：`+0x000` 文件名、`+0x100` 完整路径、`+0x164` 事件文本（`'Com'`）、`+0x174` 构件名称、`+0x194` 数据源标志、`+0x198` 所属指针。

31. **构件实例信息块与选择窗体校验按反汇编定案**（补齐 30 的「未解码」并纠正两处旧误读）：`RegComInfo` 的两个字符缓冲区分工明确——第 2 参数（调用者栈上 `0x6C` 字节）是**注册名**（如 `BESCOM.BESCOM.1`，即 `CreateInstance` 的入参），第 3 参数（`0x64` 字节）是**显示名**（如「浏览编辑选取」）；构件列表显示显示名，`ComFileEdit` 显示 `文件名: 注册名`，旧文里的「完整路径」和「把名称/描述当作者/版本」均不成立。实例信息来自 `CreateInstance` 后的 vtable：槽 18（x64 字节 `0x90`，32 位 `0x48`）以 `(nullptr, 0, &size)` 取长度、按长度分配后再 `(buf, size, &size)` 填充，槽 1（x64 `0x08`）释放；信息块为指针宽度无关的内联固定布局：`+0x00` 作者[20]、`+0x14` 版本[10]、`+0x1E` 简要说明[50]、`+0x50` 构件类型（1 基 1..4）、`+0x54` 端口数、`+0x58` 起每端口 `0x3C` 字节（`+0x28` 端口 ID、`+0x2C` 原始数据类型、`+0x30` 最大数量、`+0x34` 缺省数量、`+0x38` 端口类型 1..4）。`TComSelectForm.OKBtnClick` 的校验与文案（标题「错误」，多条直接拼接、无分隔，失败不关窗）：`CreateInstance` 失败 →「构件创建失败！」；块长 < `0x58` 或 < `0x58 + 端口数×0x3C` →「构件注册信息不正确！」；数据流输入端口的原始数据类型不在 `0..0x0D` 或 `0xFF` →「不支持该构件的数据流输入端口数据类型！\n支持的数据类型为(LSAS_TYPE_BIT~LSAS_TYPE_64F,LSAS_TYPE_VOID)」；该端口最大数量 ≠ 1 →「不支持数据流输入端口数目可变！」（中止）；数据流输入端口多于一个 →「不支持多个数据流输入端口！」（中止）。选择成功把「唯一数据流输入端口 ID」与「是否存在输出端口」写回 `+0x2E4/+0x2F4` 供 `TProcessForm` 直接取用；记录 `+0x194`（窗体 `+0x310`）= `(comType == 2)`，即数据源构件标志。窗体 `Label1` 在 `FormCreate` 写「找到」+ N +「个构件」（常量 `0x5D8B30`/`0x5D8B35`）；memo 中端口数据类型按原始整数输出，缺省数量在最大数量之前，每行以换行结束、端口数量行后多一个空行。落点：`ComponentHost::QueryInstanceInfo()`（按需创建/读取/释放，失败区分 `CreateFailed` 与 `BadInfo`，不缓存实例）、`DlgComSelect` 的原版校验与 `DataInPortId/HasOutputPort/ComType/PortCount/Instance`、`DlgComponent` 的文件/信息显示；`MainWindow` 删除 `plugins/portcfg` 持久化（只存 `plugins/dlls`，端口改为运行期现取），`DlgProcess` 删除基于 `comType == 7` 的反选映射。`test_ui` 更新至 299 断言（`RegComInfo` 双缓冲、`QueryInstanceInfo` 字段与端口、选择窗「找到1个构件」与校验、`文件名: 注册名`、memo 全行文本、注册表重建），核心 172 断言不变；`GOLDEN OK`。

32. **主菜单按 `TdxBarManager` 重建**（此前分组为自创，且含一条不存在的命令）：DFM 中主菜单 `dxMainMenu` 的顺序是 `dxBarSubItem1/3/2/4/5/6/CustomComSubItem/8/9`，即 **文件(&F)、编辑(&E)、查看(&V)、处理(&P)、分析(&A)、识别(&R)、定制(&C)、窗口(&W)、帮助(&H)**。各子项的 `ItemLinks` 与按钮 Caption 逐条核对结果：文件 = 新建/打开/保存/另存为/数据输出(`OutputBtn`)/打印/关闭/全部关闭/最近文件/退出；编辑 = 进行编辑(`EditBtn`)/剪切/复制/粘贴/撤销/重做/连续·块·行·列选取/`dxBarSubItem18`「选取参数」(读入/存盘/清除当前/清除全部)/独特码查找/帧长查找；查看 = 联动浏览(`LockScrollBtn`，`Caption/Hint` 均为「联动浏览」)/设为起点(`SetStartBtn`)/起点·帧长·步进(`FbEdit/FlEdit/StepEdit`)/显示(`ViewModeComboBox`)/不等长帧查看(`NetDataBtn`)/取反(`DataNotBtn`)/**倒序(`DataFormatBtn`)**/参数(`ParamBtn`)/标尺(`StuffBtn`，`Hint` 显示或隐藏标尺)/XTAB/YTAB/TAB跟随(`Hint` 横轴和纵轴的分割线跟随数据滚动)/纵轴显示(`YDispModeSubItem`：起始比特/帧长/帧号)/放大·缩小·恢复/界面风格(`dxBarSubItem10`：传统风格/VS.NET风格/Office2003)；处理(`dxBarSubItem4`)、识别(`dxBarSubItem6`) 的 `ItemLinks` **为空**；分析只有 `DiedaiBtn`「迭代」；定制只有 `ComManageBtn`「管理...」(Hint 管理) 与 `FastExecuteBtn`「快速执行...」；窗口 = 分页/横排/竖排/分组横排/分组竖排/标题/窗口分组/事件/工具栏列表；帮助只有 `AboutBtn`「关于...」。据此删除三条误加/重复命令：`CMD_CUSTOM_COM`「串口管理」（两个二进制中「串口」均 0 次，见 QT_NOTES）、`CMD_CUSTOM_PARAM`（与查看的 `ParamBtn` 重复，参数不再出现在定制）、`CMD_ANALYZE_FORMAT`「数据格式」（`DataFormatBtn` 实为「倒序」，本版已有 `CMD_DATA_REVERSE`；唯一的参数对话框入口改为查看→参数）。同时删除与 XTAB/YTAB 重复的「显示或隐藏横轴/纵轴分割线」命令。落点：`Actions.{h,cpp}` 去掉上述 ID 与分发，`MainWindow::CreateMenuBar()` 按上表重建；查看菜单补齐标尺/设为起点/不等长帧查看/参数，编辑菜单补「进行编辑」与「选取参数」子菜单，窗口菜单接管五种排布、标题与工具栏显隐；`DoComSelect()` 随串口命令删除。**保留的差异**：原版处理/识别菜单为空、分析的显示模式与过滤只在格式栏，本版为这些命令保留菜单入口（处理=不等长帧处理/定位/退出查找，识别=打开 `TProcessForm` 的三项，分析=迭代+显示模式+过滤）；原版查看菜单内嵌的四个栏控件（起点/帧长/步进/显示）在 Qt 中最初只出现在格式栏（已在 37 补齐）；窗口菜单末尾的「新建窗口/上一个/下一个」是 Qt 多标签结构的补充项。`test_ui` +6 条断言（定制只有两项、查看含标尺/取反/倒序/参数/不等长帧、窗口含五种排布与标题、编辑含进行编辑/两个查找/选取参数子菜单、识别助记符 `(&R)`、全窗无「串口管理」），界面增至 305 断言，核心 172 断言不变；`GOLDEN OK`（顶部菜单仅「识别」助记符 `I→R` 变化，基线已按此更新）。

33. **视图右键菜单 = 隐藏栏「快捷操作」；补「清除标签」并修好快捷栏按钮**：`TMDIChild.FormMouseUp@0x405AE0` 只在右键抬起（`button == 1`）时动作——在 `dxBarManager1` 里按名称 `快捷操作`（VA `0x5d3512`）找到 **隐藏工具栏**，把它的 `ItemLinks` 逐条拷进 `MainForm.PopupMenu`（`+0x378`）后在该点弹出，随后照常把消息转发给 `[RootPanel+0x1e4].vtbl+0x60`（`FormMouseDown@0x405A8C` 对应 `vtbl+0x64`）。DFM 中该栏的 `ItemLinks` 顺序是：联动(`LockScrollBtn`，Hint「联动浏览」)、设为起点(`SetStartBtn`)、|、连续选取、块选取、行选取、列选取、`dxBarSubItem18`「选取参数」(读入…/存盘…/|/清除当前选取/清除全部选取)、|、数据输出…、另存为…、|、帧长查找…、独特码查找…、退出查找状态、|、切换书签、下一标签、上一标签、**清除标签**、|、定位…。据此新增 `CMD_BOOKMARK_CLEAR = 40059`（Caption 清除标签），`BesView::CreateContextMenu()/contextMenuEvent()` 按上表建菜单（分隔符与子菜单位置一致）；`BookMarkBtnClick@0x40D250` 按 sender 分派四个书签按钮——`+0x37c/+0x380/+0x384` 分别走书签接口 `vtbl+0x3c/+0x40/+0x44`，其余（即 `+0x388` 清除）走 `+0x48`，故 `DoClearBookmarks()` 直接清空标记集并重绘。**修正两处旧误**：`LockScrollBtn` 在 DFM 中 `Caption` 是「联动」、`Hint` 才是「联动浏览」（32 条记录为两者相同）；快捷工具栏的「清除标签」图标此前错接 `CMD_SELECT_CLEAR_CURRENT`（清除当前选取），现已改为 `CMD_BOOKMARK_CLEAR`。`test_ui` +7 条断言（清除标签文案、快捷栏按钮指向、书签置/清、右键菜单非空、完整项序、选取参数子菜单内容），界面增至 312 断言，核心 172 断言不变；`GOLDEN OK`（截图未展开弹窗，仅「联动」文案变化不影响基线）。

34. **状态栏按原版 `dxStatusBar1` 定案（面板几何 + 光标/选区文本）**：DFM `tools/forms.dfm.txt` 1187 行起给出三面板——0：文本面板宽 380；1：容器面板宽 400 且 `Fixed = False`（随窗口伸展），内嵌 `CursorInfoLabel`（`Align = alClient`、`Layout = tlCenter`、`Transparent`）；2：文本面板宽 30、`taCenter`；`PaintStyle = stpsFlat`。反汇编确认 `CursorInfoLabel` 是各视图共同写入的唯一文本目标：视图构造时把 `[owner+0x1e8]` 存进自身 `+0x20`（`Constructor@0x47C870`），更新函数一律调 `[+0x20].vtbl+0x20` 设置 Caption。文本格式：比特视图 `标记: 文件第<起点>比特 第<光标>比特 <行>行 <列>列`（`0x442D84`，构造默认串 `标记: 文件第0比特 第0比特 0行 0列` @`0x5E31E1`），光标离开起点时另有更新函数追加 ` 距离[...]`（`0x443DE0`）；字节视图只报行/列 `标记: 第<行>行 <列>列`（`0x47FA88`/`0x4803CC`），拖动类更新另行追加 ` 距离[...]`（`0x480630`）。有选区时同一标签改显示选取统计：比特视图 `选取了 %d 比特` / `%d行×%d列` / `%d 行` / `%d 列`（`0x43E308`），字节视图 `选取了 %d 字节` / `%d行` / `%d 列`（`0x47CB14`，行文案无空格）。落点：`CreateStatusBar()` 改为 380/400(伸展)/30 三面板（`statusFile`/`statusCursor`/`statusInsert`），删除此前自创的「文件总比特/字节 + 帧长 + 选取」三个弹性标签；`UpdateStatusText()` 按 `UnitOf()`（Hex/字符/位图/十六进制字符 = 8，其余 = 1）生成上述文本，选区非空时替换标记、清空后复原。**保留差异**：字节视图的 `距离` 后缀只在三个同类更新函数之一出现，本版只要光标离开起点就追加（按显示单位换算）；`0x443DE0` 的比特视图距离串实际以逗号分隔多个数值（反汇编里连续追加 3 个逗号），含义未解码，本版只报单个距离；原版程序在本机无法启动（致命对话框 `Failed to set data for ''`），全部文本格式来自 DFM/反汇编而非运行观察。`test_ui` +14 条断言（三面板存在与 380/400 宽度、文件面板仅 `文件: 路径`、比特/字节两种标记、距离后缀、四种选取文案与字节视图行文案空格差异、选区清空复原），界面增至 326 断言，核心 172 断言不变；`GOLDEN OK`（黄金样例为比特视图，状态栏宽度与默认标记随基线更新一次）。

35. **打印的「差距」按反汇编定案：原版是占位，本版实现是扩展**：`TMainForm` 的方法表（VMT `0x5D5710`，`vmtMethodTable@-0x34 = 0x5D5FF9`，`u16` 数量 58，条目为 `{u16, u32 地址, u8 长度, 名称}`）给出 `PrintBtnClick = 0x40D9BC`；函数体只有 `MessageBoxA(form.Handle, 0x5D3D7A, 0x5D3C2D, MB_ICONINFORMATION)` 后返回——`0x5D3D7A` 是 `未完待续...`，`0x5D3C2D` 是 `提示`（GBK）。同级方法按类引用逐一验证：`ComManageBtnClick→TComCustomForm`、`SearchStringBtnClick→TSearchStringForm`、`SearchFrameBtnClick→TSearchFlForm`、`NetDataBtnClick→TNetDataForm`、`FilterBtnClick→TFilterForm`，故条目配对无歧义。`TPrintForm` 的 DFM 仍在 `.rsrc`（`TPF0 @ 0x22B5A0`，305×216，`PrintSetupBtn` 等），但类名短串不在 `.data`、找不到类 VMT，也没有任何代码按类引用创建它；`BesCom.dll` 的导入表只有 `WINSPOOL` 的 `Open/Enum/ClosePrinter`，没有 `StartDoc/StartPage/EndPage`（仅 VCL 静态链接带入 `EndDoc`），即原版从未链接打印代码。结论：原版「打印...」按钮/菜单只弹「未完待续...」，本版的 `DlgPrint` + `DoPrint()` 是**本版本扩展**，没有原版样张可校准；保留实现，文档改标为扩展，`DlgPrint` 的边距/页范围字段沿用 `TPrintForm` 的控件集合（毫米/DPI 换算是本版补充）。无代码改动；测试与断言数不变（核心 172 / UI 326）。

36. **位图模式定案：原版「位图」是休眠的字节网格视图（mode 7），不是灰度像素图**：
    - **格式栏下拉只有 5 项**：DFM `ViewModeComboBox`（`TdxBarCombo`，设计期 `Enabled = False`、`ItemIndex = -1`、`OnChange = DrawContextChanged`）的 `Items.Strings` 为 比特位图 / 等长帧比特字符 / 等长帧十六进制 / 字符方式 / 十六进制字符对应；`DrawContextChanged@0x40A888` 在 `0x40A95E` 取 `[combo+0x100]`（ItemIndex）作为新 mode，仅当当前记录 mode 已是 7 时保持 7。参数对话框的 `modeCombo` 也一直是这 5 项。
    - **位图按钮不切模式**：DFM 中 `BMPBtn` 与 `BinBtn/HexBtn/TextBtn` 同为 `bsChecked`、`GroupIndex = 3`、`OnClick = DrawContextChanged`，但该函数从不读取 `BMPBtn`（`+0x488`）；Bin/Hex/Text/BMP 的按下态只在 `[view+4] == 7` 时才写入记录 `+0x64` 子格式（`0x40AA4A`）。因此在正常 UI 中按「位图」只会重绘当前下拉模式，不会进入 mode 7。
    - **mode 7 的唯一入口**：视图构造函数 `0x403D50` 从流中读出 4 字节魔数 `0x1234ABCD`（写入点 `0x47C08D`），相等才把视图 mode 与记录 mode 置 7；mode 7 下下拉框被禁用（状态应用路径 `0x509AA4` 传 `dl=0`）。
    - **mode 7 的实际绘制**（painter `0x47C870`）：按 `+0x64` 分子格式——0 Bin / 1 Hex / 2 Text / 3 BMP。1 为十六进制文本网格（4 格一组、每 4 格补分隔、字体 Courier New）；0/2 为 5 格一组的文本网格（每 10 格标号，Text 用楷体）；3（BMP）在 `0x47D374` 起画**方格网格**：格宽取缩放值（`+0x54`），50 格一组、每 50 格画下划线、每 100 格标号，矩形用固定画笔/画刷颜色，循环内没有任何按字节值取色的代码——所以它是「每字节一个方格的网格视图」，不是灰度图像。`0x47E414` 只是求行内最大格宽/字节数的辅助函数。
    - **落点**：`MainWindow` 格式栏 `cboViewMode` 去掉多余的「位图」项，恢复原版 5 项（`ComboIndexFromView`/`ViewFromComboIndex` 处理 combo 4↔`ViewMode::HexChar`；`ViewMode::Pixel` 在 combo 中显示为 -1 空选）；「位图」工具按钮/菜单仍选中本版的灰度 `ViewMode::Pixel` 渲染，作为**文档化扩展**保留（原版正常 UI 无法到达 mode 7，且原版在本机无法启动，无样张可校准）。`test_ui` +5 断言（下拉 5 项、第 5 项文案、位图按钮仍切到 Pixel、Pixel 不回显到下拉框、combo 4↔HexChar），UI 增至 331 断言，核心 172 断言不变；`GOLDEN OK`。

37. **查看菜单内嵌栏控件补齐**：DFM 中 `FbEdit/FlEdit/StepEdit` 是 `TdxBarSpinEdit`（Caption 起点/帧长/步进、ShowCaption=True、Enabled=False、`OnChange = DrawContextChanged`，FbEdit 另有 `OnButtonClick = FbEditButtonClick`、`ValueType = svtFloat`、MaxLength 11/8/8），`ViewModeComboBox` 是 `TdxBarCombo`（Caption 显示）；四者的 `ItemLinks` 同时挂在格式栏与 `dxMainMenu` 的查看子项里，即原版两处显示的是同一组控件。落点：把格式栏 lambda 里的 spin 行提取为 `MainWindow::CreateSpinRow()`（QLineEdit + 上下箭头，箭头沿用按显示单位 ±1、0 处钳制的 bump），查看菜单在「设为起点」之后用 `QWidgetAction` 插入 起点/帧长/步进/显示 四行镜像控件（`menuEdStart`/`menuEdFrameLen`/`menuEdStep`/`menuCboViewMode`）；`WireSignals()` 把菜单文本框接到 `OnMenu*Edited()`（先把菜单文本回写格式栏，再走原处理槽），菜单下拉直接走 `OnViewModeCombo()`；`UpdateMenuState()` 同步两处文本/下拉索引/帧长单位提示，并把「显示」下拉的可用性与格式栏一起按编辑态禁用。Qt 无法把一个 widget 同时放进 QToolBar 与 QMenu，故用镜像控件而非同一实例（行为/外观等价）。`test_ui` +6 断言（四个控件存在、起点镜像格式栏、菜单下拉 5 项、菜单改起点生效且格式栏跟随、菜单下拉 4↔十六进制字符对应并同步格式栏），UI 增至 337 断言，核心 172 断言不变；`GOLDEN OK`（菜单未展开，截图不变）。
38. **分组排布不再伪造 XTAB/YTAB 边界线**：此前把「分组横排/竖排」当成额外的分隔线来源——`ApplyLayoutTo()` 在组模式下强制 `showXTab/showYTab = true`，`BesView` 再把线宽加到 2px（文档里的「组内边界线为近似」即指此）。反汇编 `PageHorizBtnClick@0x408CB4`（置 `[form+0x498]=3`）与 `PageVertBtnClick@0x408E2C`（置 4）逐条核对：两者只做①取根面板 `[form+0x2E4]` 的客户区、②遍历 `[form+0x2F4]`(GroupTreeView)`.+0x254` 的组项（`0x4273BC` 取项、`0x426BB0` 为真时跳过该组、`0x4265A0` 判定单/多子项）、③对承载控件 `[item+0xC]` 调 `0x544BC8` 重新插入（单子项 `cl=1`；多子项首项 `cl=1`，其余相对前一项 `cl=5` / `cl=4`）、④刷新（`0x5442B4/0x5573CC/0x407F38/0x54C208/0x54C1DC/0x54DB60`）；全程不写视图参数、不碰 XTAB/YTAB 的 Down 态、也没有绘制调用 ⇒ 分组边界是停靠站点/分隔条自身的边框，不是视图内的 TAB 线。落点：`MainWindow::ApplyLayoutTo()` 只写 `params.layout`；`BesView` 的 XTAB/YTAB 只由用户开关决定（删除 `|| groupH/groupV` 与 2px 线宽）。`test_ui` 的全局断言拆成两条：模式写入全部视图、排布不改 XTAB/YTAB 开关（UI 增至 338）；核心 172；`GOLDEN OK`（默认分页不变）。

39. **参数设置状态基语义定案（「构件选项」页随状态基隐藏）**：此前「从端口」只在 `DlgParamSet` 里以 `hasStatusPort=false` 禁用，文档仍标「⚠ 无状态端口来源」。本轮把整条链路反汇编补齐：
    - 主窗口私有字段 `+0x4A0` 是宿主状态基，由 `0x406DB8` 用 `[ebp+0xC]` 赋值（`0x406E3C`），全模块仅此一处写入。
    - 独立启动 `bes.exe → bescom.dll!ShowModal@0x401ED0`：`0x401F08 push 0`，即以 nil 状态基创建主窗口；`0x406DB8` 再把它存入 `[main+0x4A0]`。
    - `ParamBtnClick@0x40CF80` 把 `[main+0x4A0]` 压栈交给 `TParamSetForm` 构造 `0x419688`，后者存入 `[form+0x3E8]`（`0x4196E7`）。
    - `FormCreate` 在 `0x419E64` 判空：非空时从 `[base+0x11E]` 拷 0x20 字节到 `[form+0x3EC]`；为 nil 时执行 `ComOptionPage.TabVisible := False`（`0x42513C` 写 `+0x1F8`）。
    - `WindowListChange@0x41A198` 用窗口 `+8`（1 基端口号）决定「从端口」是否可用，勾选值读 `[form+0x3EB+端口]`；`EditChange@0x41A9A4` 比较后写回 ⇒ 同端口窗口共享、按端口记忆；从端口取格式时 `GroupBox2` 整体禁用。
    - 独立启动时所有窗口端口号为 0 ⇒「从端口」恒禁用；本版文件窗口相同。落点：`DlgParamSet` 照原版构建 4 页但隐藏「构件选项」页（`setTabVisible`），页面内缓存/块/临时目录逻辑保留；`test_ui` 增隐藏页断言（UI 339）；核心 172；`GOLDEN OK`。宿主嵌入通道 `0x4184BA`（消息码 `0x401`）可传非空状态基使该页显示、按端口记忆，本版未接入。

40. **连续/块/行/列选取交互对齐（鼠标、模式按钮、初始态）**：此前只做「二维块」，连续选区是单区间，且默认进入连续模式，交互细节未核对。本轮按反汇编把四种选取的鼠标流程与模式按钮语义全部对齐：
    - **鼠标三段式**：`MouseDown@0x443840`（仅左键）把命中格同时写入「锚点」(`+0x54/+0x5C`) 与「当前格」(`+0x60/+0x68`)，置拖动标志 `+0x51=1`；`MouseMove@0x443DE0` 只更新当前格、刷新并处理拖出自动滚动（`+0x52`），**不写任何选区列表**；`MouseUp@0x444924` 才按当前模式向对应列表追加一条 24 字节记录（连续段还调 `Sort`，比较器 `0x4033AC` 按 p1 再 c1 升序；行/列模式合并相邻/重叠区间）。⇒ 拖动过程只是预览，松开才提交；每次拖动独立累积，不是替换。
    - **连续选区是多区间**：排序列出所有记录，`IsSelected`/输出/数据流按并集逐区间消费。落点：`SelectionSet::AddContinuous()` 用 `upper_bound` 按起点有序插入（`Continuouses()` 遍历；`SelectedBits/SelectedBitsEx`、`Renderer::IsSelected`、`OutputWriter`、`FlowEngine` 全部改为多区间）。
    - **模式按钮**：`SelectBtnClick@0x40AC88` 读按钮 Down 态，把 `Tag` 写入选区模式（连续 0 / 块 1 / 行 2 / 列 3），未按下写 -1；`SetSelectMode@0x43FFBC` 切换，清空核心 `0x4404CC` 在 `dl=0` 时只清上一模式列表。DFM 四个按钮 `GroupIndex=2` + `AllowAllUp` 且无 `Down=True` ⇒ **初始模式为 -1（无选取模式）**，点同一按钮再点一次会全部取消。落点：`BesDoc::SetSelectMode()` 同模式再点 → `ClearAll()` 并退出，切模式只 `ClearMode(old)`；ctor/`OpenFile` 不再默认连续模式；四个动作改为可勾选（`Actions.cpp`），`UpdateMenuState()` 同步勾选。
    - **Ctrl 不参与**：`0x43E000–0x449000` 鼠标处理器内无任何 `GetAsyncKeyState` 调用 ⇒ 原版没有 Ctrl 追加/取消语义（追加来自「每次拖动独立记录」）。
    - **落点**：`BesView::CommitSelection()`（松开提交，100k 上限）、`BuildPreview()`（拖动中复制状态并叠加待提交区间，不改列表）、`BesDoc::SetSelectMode`；`QT_NOTES.md` 新增「选取交互（反汇编确认）」。
    - **验证**：`test_core` +13（多区间有序插入/渲染/求和、行/列区间合并、`ClearMode`、`.slp` 多区间往返与段计数）；`test_ui` +12（动作可勾选、切换只清上一模式、再点清空退出、勾选同步、按下/移动不提交、松开提交、二次拖动累积）。合计**核心 185 / UI 351**，`ALL TESTS PASSED`，`GOLDEN OK`。
    - **原残留已补齐**（见 41）：Shift+方向键扩展、拖出视口自动滚动，以及无 Shift 方向键的光标移动；剪贴板复制仍按全部连续区间的外接矩形取字节。

41. **选取的键盘扩展与拖出自动滚动**：`KeyDown@0x445B0C` 跳表确认——选取模式 ≠ -1 且 Shift 按下时，方向键/PgUp/PgDn/Home/End 移动「当前端点」：Up/Down 行 ±1（保持列）、Left/Right 列 ±1（保持行）、Home=行首、End=行尾、PgUp/PgDn ±`+0x40`（可视化行数）；首次按键从锚点走一步（Shift 按下把端点置 -1）。`KeyUp@0x4477DC` 在 Shift 松开时 `call [vmt+0x60]`（`MouseUp@0x444924`）提交，Esc 分支删除与锚点/端点匹配的记录。鼠标侧 `MouseMove@0x443DE0` 用 `PtInRect`（`0x443F5C`）判断离开客户区后置 `+0x52` 并启动定时器自动滚动。落点：`BesView` 新增 `keyPressEvent/keyReleaseEvent` 的 Shift 分支与 `BeginKeyboardSelection/MoveKeyboardEndpoint/EndKeyboardSelection`（端点按显示行×格列移动，换算回比特后走既有 `CommitSelection`；Esc 取消未提交预览）；拖出视口用 60ms `QTimer`（`UpdateAutoScroll/OnAutoScrollTick`）滚动并把鼠标点钳进视口重新命中，端点跟到边缘格。`test_ui` +13 断言（Shift+Right/Down/End/Home、松开提交 16..64、Esc 取消、块/行/列三种提交、无模式不动作、拖出自动滚动与松开提交），UI 增至 364、核心 185；`GOLDEN OK`。

    **无 Shift 方向键补光标移动**（原尾段 `0x44611D`）：无选取模式或未按 Shift 时，`KeyDown` 尾段要求 `state+0x18 == 0` 且 Shift 未按下，移动光标 `+0x54/+0x58`（比特）与 `+0x5c`（列），再调 `0x4404CC(dl=0)` 只清当前模式列表。落点：`BesView::MoveCaret(dRow, dCol, absCol)`——Up/Down 行 ±1（保持列、钳到有效行）、Left/Right 列 ±1 且行首/行尾折返到相邻行、Home/End 行首/行尾、PgUp/PgDn ±`pageStep()` 行；写回 `BesDoc::SetCursorBit` 前用 `ClearMode` 清当前模式列表，随后 `EnsureBitVisible` 滚动到光标并重绘，方向键不再驱动滚动条。`test_ui` +9 断言（Right/Down/End/Home 光标位置、带连续选区按方向键只清列表不动滚动条、行尾 Right 与行首 Left 折返、首格 Left 与末格停留、PgDown 按页步进），UI 增至 373、核心 185；`GOLDEN OK`。

42. **工具栏按钮级定制（原版 `TdxBarManager` + `TdxBarCustomizingForm`）**：此前只做到每条工具栏的显隐，栏内按钮不可调整。证据：主窗体 DFM 持有 `dxBarManager1: TdxBarManager`（4 条栏：主菜单/主工具栏/格式栏/快捷操作），`CanCustomize` 未被关闭（DevExpress 默认 True）；资源里保留标准定制窗体——类名短串 `TdxBarCustomizingForm` @VA `0x51fa27`、VMT `0x51ede4`，DFM `TPF0 @ 0x214800` 的 Caption 为 `Customize`，页签 ` Toolbars `/` Commands `/` Options `/` All Commands `/` Groups `，底部 `Close`/`&New...`/`R&ename...`/`&Delete`/`&Reset...`；DevExpress 文档给出的入口是工具栏右键菜单与双击停靠站点。落点：`MainWindow` 以「每条栏一份命令 ID 有序列表」为模型（`m_barItems`，`0` = 分隔符，`-1` = 格式栏纵轴下拉框占位；默认值逐项取自 DFM `ItemLinks`），`RebuildToolBar()` 只摘除布局管理的命令动作与动态分隔符，格式栏的标签/数值框/「显示」下拉/YDisp 下拉保持为固定控件——占位符之前的分组插到 YDisp 前面，之后的分组追加到栏尾；右键工具栏（含按钮）弹菜单「三条栏勾选 + 自定义...」，双击栏空白处打开 `DlgCustomize`。对话框按标准窗体建两页：`Toolbars`（三条栏显示勾选，立即生效）与 `Commands`（栏选择 + 当前按钮/可用命令双列表，添加/删除/上移/下移/Reset.../Close；YDisp 占位不可选）。栏内容与显隐分别写入 `view/barMain|barFormat|barQuick` 与 `view/tbMain|tbFormat|tbQuick`，下次启动恢复；删除原「显示/隐藏全部」占位命令 `CMD_VIEW_TOOLBARS`（原版窗口菜单的「工具栏」是子菜单，不是切换命令）。`test_ui` +25 断言（默认布局 = DFM 顺序、改栏只影响该栏按钮、分隔符唯一、固定控件存活、单栏显隐与菜单同步、右键菜单三条栏 + 自定义、对话框标题/两页/控件、可用列表、添加/上移/删除、YDisp 占位锁定、Reset、勾选显隐立即生效、关闭、三项布局键与三项显隐键持久化并在下次启动恢复），UI 增至 398、核心 185；`GOLDEN OK`（格式栏「参数」与「标尺」之间补上原版 BeginGroup 分隔，基线更新）。

43. **窗口分组支持把窗口拖入分组（原版停靠/拖动分组的等价交互）**：此前分组只能通过「窗口 → 窗口分组」对话框建组时勾选成员，之后无法调整，文档在 §2 标为「⚠️ 非拖拽」。原版的数据窗口是挂在组树下的停靠站点（`[form+0x2F4]`(GroupTreeView)`.+0x254` 的组项 `+0xC` 即承载控件，联动收集器 `0x407AF8` 也按它定位），因此原版「拖进分组」是停靠系统本身的拖动，组树并不接受拖放。本版用分组树承担同一语义：`GroupPane` 打开 `DragDrop` 模式并覆盖 `mimeData()`——只有单个窗口项（`UserRole ≥ 0`）能产生 `application/x-bes-window-tab` 数据，分组节点不可拖；`dragEnterEvent/dragMoveEvent` 只放行该 MIME，`dropEvent` 用 `itemAt(pos)` → `GroupOf()` 解析目标组（落在组内子项上视为其父组），随后 `QTimer::singleShot(0, …)` 延迟发 `WindowDroppedOnGroup`，避免在 Qt 仍持有拖放 item 指针时清空模型。`MainWindow::MoveTabToGroup()` 维护单一归属：先从其它自定义组移除再加入目标组，目标是「数据窗口」时只做移除；完成后 `SyncGroupPane()` 重建树、`ApplyArrangement()` 更新分组排布的可见集合，因此拖动会立刻影响分组横排/竖排。新增 `CreateGroup()` 供右键「新建分组...」复用，两个入口共享同一约束。`test_ui` +15 断言（拖动模式、MIME 往返/拒绝非法数据、空组、移入、重复拖入去重、跨组移动、新建分组时勾选窗口移出原组、同组同屏、移出后可见集缩小、拖回「数据窗口」、空组回退全部视图、真实 `DragEnter`+`Drop` 路径），界面增至 415、核心 185；`GOLDEN OK`（分组树不在黄金截图内）。测试侧记一笔：`QApplication::notify` 对 `Drop` 只转发给 `QDragManager::currentTarget()`，合成事件必须先被 `DragEnter` 接受，单独 send `QDropEvent` 会被静默丢弃。

44. **标尺数字避让 + 行/列单击反选**（用户报告：「列上的数字刻度全挤到一起了。行、列选取，选中后无法取消」）：
    - **列刻度重叠**：上/下标尺此前在每个 8 比特边界都画一条刻度线和一个数字，且数字矩形固定按 4 格宽排布。字节/十六进制模式下每一格都是 8 比特边界，于是相邻数字互相压字；比特模式每 8 格一个数字，碰巧不重叠。落点：`BesView::PaintEvent` 的 `drawRuler` 保留全部刻度线，但数字改为按 `QFontMetrics::horizontalAdvance` 的实际字宽排版，与上一个数字右边界 + 6px 相交就跳过该数字（任何格宽/缩放/模式都不会再叠字）。黄金截图为比特字符模式的 8 比特宽格，标签间距本就足够，基线不变。
    - **行/列选中后无法取消**：反汇编确认原版 `MouseUp@0x444924` 的行/列分支只做区间并集合并（没有 toggle、没有 XOR），`KeyDown@0x445B0C` 里 Esc 跳到 `0x44611D` 是空操作——原版取消只能再点当前模式按钮（`AllowAllUp`）或「清除当前/全部选取」。这些路径本版都已具备，但用户直觉是再点一下选中的行/列。落点：`SelectionSet::ToggleRow/ToggleCol`（有序插入/删除，保持 `SelectMode::Row/Col`），`BesView::CommitSelection` 在行/列起止帧（列）相同时调用 toggle，拖动跨多行/列时仍走 `AddRowRange/AddColRange` 累加。`test_core` +4（ToggleRow/ToggleCol 删除、按序重新插入），`test_ui` +4（行/列单击→取消→再选回且模式保持），核心 189 / 界面 426；`GOLDEN OK`。

## 明确剩余差距

| 差距 | 影响 | 计划 |
|---|---|---|
| 32 位构件不再支持 | 原版 32 位构件无法加载 | 按需求放弃 32 位，仅使用 64 位构件 |
| 原版 `BesCom.dll` 为 32 位，无法在本版本中运行 | 无法证明与原版逐像素/逐行为一致 | 以当前截图/测试为基线；如需对拍需另建 32 位参考环境 |
| 非 MDI，使用 Tab | 原版是 MDI 子窗体 + 停靠站点（分组由停靠图层拖动），现版是单一标签组 + 分组树（组内拖放已等价落地，见 43）；原版联动只覆盖「同一停靠容器」，现版单一标签组 ⇒ 等同其它所有 Tab | 可保留 Tab（现代化）或改 QMdiArea |
| 宿主嵌入通道 | 原版 `0x4184BA`（消息码 `0x401`）可由宿主传入非空状态基创建主窗口，从而显示「构件选项」页并按端口记忆「从端口」；独立启动不走此路径 | 本版无宿主通道；独立启动行为已与原版一致（见 39），不实现宿主嵌入 |
| 排布不改字节流 | 原版五种排布只重排窗口（停靠站点重新插入 RootPanel），视图内容仍是各自的分页帧网格；本版本额外让 横排/分组横排 把行作为连续数据流绘制（见 29） | 保留为文档化的本版本扩展；如需逐行为一致可切回「仅重排窗口」 |
| 构件端口按二进制 ABI 直连 | 原版引擎在工作线程里直接调构件 vtable（命令 `+0x28`、优先级 `+0x30`、自定义参数 `+0x18`、自定义显示 `+0x34`），现版选区经 `IFlowPort` 落到临时文件端口 | 数据流管线已等价落地（见 28）；`BesCom.dll` 只导出 4 个符号，引擎/记录的二进制布局无法验证，不虚构 ABI；自定义视图尺寸同步（命令 100）仍未接 |
| 打印为本版本扩展 | 原版 `PrintBtnClick` 只弹「未完待续...」，`TPrintForm` DFM 未实例化，无原版打印产出 | 保留现实现并在 35 标注为扩展；不追求原版样张对齐 |
| 位图为本版本扩展 | 原版下拉框仅 5 项；「位图」按钮不切换模式，mode 7 是只能由魔数 `0x1234ABCD` 流恢复的字节网格视图（Bin/Hex/Text/BMP 子格式），原版正常 UI 不可达 | 位图按钮/菜单保留灰度 Pixel 实现并标注为扩展；不追求原版 mode 7 的网格外观（原版无法启动，无样张） |

## 构建与验证

```bat
build-qt.bat          :: 主程序
run-tests.bat         :: 核心 + UI 测试
tools\golden.bat      :: 黄金截图（首次生成 baseline，后续哈希对比）
```

测试覆盖：

- `test_core`：189 断言（自动帧长、Hex 双 nibble、水平偏移、二维块、不等长帧、帧过滤（多条件“与”、数值低字节在前、CRC、比特位置）、横排行号标尺（YFN/YFL/YFB）、40 比特独特码、去模糊、同步列帧长、输出、序列化、步进与帧布局解耦、倒序=高位在前关闭（每字节内 7-0，含比特模式不跨字节）、取反、连续选区多区间（有序插入、逐区间渲染/求和、按模式清空）、行/列单击反选（`ToggleRow`/`ToggleCol` 删除与按序重新插入）、`.slp` 二进制选区格式（`slp` 魔数 + 4 段 24 字节记录、段序 连续/块/行/列 与段计数偏移、记录内字段偏移、四类选区往返、多区间计数与往返、错误魔数与截断拒绝）、字节编辑（可增长内存副本、改写/插入/删除、撤销/重做、脏标志、分块落盘、丢弃后仍保持编辑方式、映射模式拒绝插入/删除、尾部越界改写裁剪、只读文件保存失败）、数据流管线（端口缺失/空数据/写失败三类提示、全量与行/块/列/连续选区推送、块上限切分、反选补集、比特模式、停止/暂停/继续/复位/退出与命令 3/4/5、`ViewLayout` 编码与 `[form+0x498]` 一一对应（0/1/2/3/4 及中文模式名））。
- `test_ui`：426 断言（打开文件、滚动范围、Hex 渲染、标题独立开关、格式栏数值箭头（按显示单位 ±1、64 位文本）、格式栏「显示」下拉 5 项与原版一致（位图仅工具栏扩展，不回显；combo 4↔十六进制字符对应）、查看菜单内嵌起点/帧长/步进/显示并与格式栏同步、格式栏图标+文字（短标题/参数标题）、倒序按钮与“高位在前”同步、单步/迭代分离（工具栏单步可勾选、提示数据流、模式往返；迭代为空占位）、状态栏三面板几何（文件 380px、光标 400px、插入 30px）与文本（文件面板仅「文件: 路径」、比特视图标记含绝对比特/行/列与距离、字节视图标记只报行/列、四种选取文案与字节视图行文案空格差异、选区清空复原标记）、状态栏插入/改写指示与 Insert 切换、左侧行号标尺（分页/横排）及横排绘制、14 个对话框全部构造（含 `DlgProcess`/`DlgProgress`）、参数设置 4 页构建（独立启动隐藏构件选项页）/窗口列表/从端口禁用/应用与数值裁剪/高位在前勾选与提交、关于对话框、帧过滤条件列表/低字节在前/CRC 与描述文本、搜索帧长默认参数与 64 比特命中、打印边距/页范围解析、构件 ABI（`RegComInfo` 双缓冲、`QueryInstanceInfo` 字段/端口/校验、`ShowModal`、端口增删）、构件选择（「找到1个构件」、选择成功带回类型/端口与数据流输入端口 ID）、定制构件管理（`文件名: 注册名`、memo 全行文本）、快速执行（临时宿主枚举构件、非构件二进制报「不是局级系统构件！」、与“刷新”为两个命令）、编辑模式（「进行编辑」按钮文案/按下态、编辑态开关控件可用性、显示模式下拉框编辑态禁用/退出恢复、复制进剪贴板的字节、改写与插入两种粘贴、撤销/重做可用性、取消保持按下、保存退出后磁盘内容逐字节比对、只读文件提示「文件不可写！」）、构件注册持久化、输出端口树与 32 端口单选联动及真实编号、不等长帧前导码处理与 .bnt 输出/读入、多视图、联动浏览（同容器其它视图按移动量位移、关闭不跟随、双向）、构件数据流处理（空闲/运行/停止/复位/退出五态、进度 0..256 映射与封顶、输入选择启用规则、数据类型 255、输出端口与输出标志、校验文案、优先级默认值、复位能力开关两个方向、「开始」经 `FlowEngine` 把选区字节推入数据流输入端口）、进度对话框 302×86 几何与 0..256 刻度、窗口排布（默认分页只一个视图同屏、横排/分组横排为横向分隔且全部/当前组视图可见、竖排/分组竖排为纵向分隔、排布切换把模式写入全部视图、排布不改 XTAB/YTAB 开关、回到分页后其余视图不再同屏）、主菜单分组（定制仅「管理...」「快速执行...」、查看含标尺/取反/倒序/参数/不等长帧、窗口含五种排布与标题、编辑含进行编辑与选取参数子菜单、识别助记符 `(&R)`、无「串口管理」）、视图右键菜单（项序/分隔/子菜单与隐藏栏「快捷操作」一致、书签切换与清除标签）、选取交互（四个模式按钮可勾选、初始无选取模式、切换只清上一模式、同按钮再点清空退出、勾选状态同步、拖动中按下/移动不提交且预览不改列表、松开提交一条记录、多次拖动累积、Ctrl 拖动同样累积、单击已选行→取消→再选回且保持行模式、单击已选列→取消→再选回且保持列模式、Shift+方向/PgUp/Home/End 移动端点并在松开 Shift 时提交、Esc 取消预览、块/行/列三种键盘提交、无模式时 Shift+导航键不动作、拖出视口自动滚动并在松开时提交、无 Shift 方向键移动光标/左右折返到相邻行/Home/End/PgDown 并清除当前模式列表）、工具栏按钮级定制（默认布局 = DFM ItemLinks、逐栏替换按钮不影响其它栏或命令本身、分隔符重建唯一、格式栏固定控件存活、单栏显隐与窗口菜单勾选同步、右键菜单三条栏 + 自定义、`DlgCustomize` 标题/两页/控件、可用命令列表、添加/上移/删除、YDisp 占位锁定、Reset 恢复默认、勾选显隐立即生效、关闭对话框、布局与显隐键持久化并在下次启动恢复）、窗口分组拖放（拖动模式、MIME 往返/非法数据拒绝、空组、移入/去重/跨组移动、新建分组勾选窗口会移出原组、分组排布跟随、拖回「数据窗口」、空组回退、真实 DragEnter+Drop）、截图）。
