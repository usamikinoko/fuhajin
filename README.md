# Fuhajin Widget

轻量 Windows 桌面挂件，显示 CPU / GPU / 网速 / 内存 等实时数据。
原生 C++17 + Win32 + GDI，静态链接，单文件 exe 免安装。

## 卡片效果

右键 → `卡片效果`：

| 项 | 说明 |
|---|---|
| 实体卡片 | 背板完全不透明 |
| 半透明 → `20%` ~ `80%` | 背板按透明度与背后画面混合，**默认 50%** |

- 半透明档位就是「透明度」：`50%` = 背板只保留 50% 的不透明度。
- 菜单里 `半透明` 后面括号内的百分比是当前档位，子菜单中打勾的那项也是它。
- 文字**始终不透明**，任何档位下都不参与透明混合 —— 半透明卡片上读数依旧要看得清。
- 实现在 `src/ui/render.cpp`：背板不透明度 = `255 * (100 - 透明度) / 100`，
  再按 `alpha = 背板不透明度 * 圆角覆盖率`、`src_rgb + bg_rgb * alpha * (1 - 文字覆盖率)`
  逐像素合成到 32 位 DIB，最后 `UpdateLayeredWindow` 一次性提交。
- 旧版本的「高斯模糊」（`SetWindowCompositionAttribute` + `ACCENT_ENABLE_BLURBEHIND`）
  已被半透明取代：分层窗口上叠 DWM 模糊既不稳也不可控，且要为了刷新模糊另起一个 250ms 定时器。
  旧配置项 `glass` 仍会被读取一次，用来决定 `effect` 的初始值，老用户的选择不会被重置。

## 鼠标穿透

是否穿透**跟着"卡片效果"走**（`ui/input.cpp: InputSetPass`，`app/settings.h` 的 `EFFECT_*`）：

| 卡片效果 | 左上角行为 | 说明 |
|---|---|---|
| **实体卡片** `EFFECT_SOLID` | 整块不穿透 | 所有按键都作用于挂件本体；右键弹菜单 |
| **半透明**（任意透明度）`EFFECT_ALPHA` | 只有左键穿透 | 左键（含拖动）落到下面的窗口；中/侧键/滚轮被吞；右键归挂件 |

- **左键（含拖动）**：半透明档下是**原生**穿透 —— 按下保持、拖动、双击的语义全部由系统完成，钩子不掺和。
- **中键 / 侧键 / 滚轮**：半透明档下，落点在卡片矩形内时被钩子吞掉，不下传。
- **右键**：吞掉后转交给挂件本体，用来呼出自己的菜单；下层窗口收不到它，不会同时弹自己的菜单。

实现：

- 窗口带不带 `WS_EX_TRANSPARENT` 决定"这块区域参不参与命中测试"，`InputSetPass()` 按当前模式同步这个样式位。
  `EFFECT_SOLID` 时摘掉它 ⇒ 整块区域可点击；`EFFECT_ALPHA` 时挂上它 ⇒ 左键**原生**穿透。
- **运行时切换样式位不会立刻改变命中测试**：`SetWindowLongW(GWL_EXSTYLE)` 之后必须让系统重建窗口区域，
  `SWP_FRAMECHANGED`、`UpdateLayeredWindow` 都**实测无效**，只有 `ShowWindow(SW_HIDE)` +
  `ShowWindow(SW_SHOWNOACTIVATE)` 有效（分层窗口的内容不会因此丢）。所以 `InputSetPass` 里做了一次显隐。
  值没变时直接返回，避免每次关菜单都闪一下；窗口还没显示（启动期）时只改样式位。
- 半透明档下，除左键外的按键走全局低级鼠标钩子 `WH_MOUSE_LL`（`src/ui/input.cpp`）：
  落点在卡片矩形内就吞掉（`return 1`），其中右键额外 post 一条真实的 `WM_RBUTTONDOWN / WM_RBUTTONUP`
  给挂件自己，菜单由 `window.cpp` 的 `WM_RBUTTONUP` 分支弹出。实体卡片档下钩子完全不拦这些键
  （窗口本来就收得到），否则会和原生消息重复计一次。
- **右键一旦按下被截下，这一对的抬起也一定被截下**（`s_catchRight` 记到 up 为止，不再看位置）。
  这条不是洁癖：真人点击时鼠标会在按下与抬起之间漂十几像素，若按下在卡片内、抬起漂到卡片外，
  只吞 down 而放行 up，下层会收到一个**孤立的 `WM_RBUTTONUP`** —— 很多程序（资源管理器、浏览器、
  桌面）正是在 up 上弹上下文菜单，表现就是"右键还是透过去了"。
- 菜单展开期间（`InputMenuOpen(true)`）：左/中/右键落在菜单窗口（`#32768`，本进程）之外，
  就给挂件 post 一条 `WM_CANCELMODE` 把菜单收起来。之后按落点分流 —— 落在**卡片矩形内**的右键
  （含被菜单盖住的区域）**只收菜单，不再下传**；落在卡片外的照常放行给下层。
  挂件**不是前台窗口**（点击都被钩子吞了，进程从来没"收到过输入"，`SetForegroundWindow` 会被系统拒绝），
  系统不会替它收起菜单，所以只能自己收。
- 菜单上的点击一律放行（`OverOwnMenu` 判定 `#32768`），否则菜单项点不动。
- 也正因为不抢前台，挂件不会在开菜单时被系统抬到菜单之上 —— 菜单是置顶窗口，
  挂件始终在它下面，层级稳定。但**菜单收掉之后要自己抬回置顶带最上层**（`WM_RBUTTONUP` 里的
  `SetWindowPos(HWND_TOPMOST)`）：菜单关闭 / 切模式时系统会把别的置顶窗口抬到挂件之上，
  不补这一下挂件会沉到别的置顶窗后面。此时菜单已销毁，抬自己不会再压住菜单。
- 若钩子安装失败（`SetWindowsHookExW` 返回 NULL），半透明档下的右键菜单会失效，此时只能结束进程重启。
- `WH_MOUSE_LL` 有系统级的回调超时（`LowLevelHooksTimeout`，默认约 300 ms），回调超时的那次事件会被系统
  直接放行。所以钩子回调里只做矩形比较，不要在里面查询或等待任何东西。

**试过但走不通的路**（别再回头）：让窗口常态接收鼠标消息，只在钩子看到"左键按在卡片内"时临时挂上
`WS_EX_TRANSPARENT`，好让这一下左键穿过去。实测不行 —— 低级钩子被调用时，这次点击的命中目标**已经定好了**，
钩子里改 ex-style 只对下一次事件生效，那一下左键仍然落在挂件上。补一个 `SendInput` 注入合成 down
（`LLMHF_INJECTED` 判断、状态机判断都试过）同样没能把 down 落到下层。
结论：只能在"常态穿透"的骨架上做减法，而不是在"常态接收"的骨架上做加法。

## 关于 CPUT 行

CPUT 显示的是**芯片片上最高温度**，也就是通常说的 Tctl/Tdie 口径：

- 优先取 ACPI 热区 `\Thermal Zone Information(*)\High Precision Temperature`（0.1 K），不存在时回退到 `\Thermal Zone Information(*)\Temperature`（1 K），统一到 ℃。
- 再与 iGPU 温度传感器（`D3DKMTQueryAdapterInfo(type=62)`，同一颗 APU 裸片）取较大值，更接近真实片上峰值。
- 对读数做短窗口峰值保持（默认 5 秒），避免采样点刚好落在尖峰之间。
- 当当前瞬时温度明显低于保持中的峰值时，数值后会显示 ▲，表示仍在峰值后的衰减期。

右键菜单可以关闭或切换峰值保持窗口。

## 目录结构

分层原则：**数据采集 / 应用状态 / 界面渲染** 互不越界。UI 只认 `Metrics` 结构，不碰任何系统 API。

```
src/
  main.cpp              进程入口：单实例、DPI 感知、初始化顺序、消息循环
  app/
    settings.h/.cpp     全部可持久化配置 + 取值表（配置 schema）+ ini 读写
  metrics/              数据采集层，唯一调用系统 API 的地方
    metrics.h           对外契约：Metrics 结构 / MetricsSample / MetricsSetHoldMs
    metrics.cpp         采样编排 + 进程自身内存
    pdh.h/.cpp          PDH 查询与计数器数组的共用封装
    cpu.h/.cpp          CPU 占用（GetSystemTimes 差分）、片上温度（ACPI 热区）、峰值保持
    gpu.h/.cpp          GPU 占用/显存（PDH）、iGPU 温度（D3DKMT type=62）
    net.h/.cpp          上下行速率（GetIfTable2 差分）
  ui/
    layout.h/.cpp       DPI 缩放、卡片尺寸、挂靠位置计算
    theme.h/.cpp        明暗主题配色表
    rows.h/.cpp         一帧读数 → 7 行文案与颜色（纯格式化，无 GDI）
    render.h/.cpp       离屏 DIB、字体、底/文字层绘制、逐像素合成、提交
    input.h/.cpp        穿透开关（跟随卡片效果）+ 全局鼠标钩子：半透明档下只放行左键
    menu.h/.cpp         右键菜单构建与命令分发（只返回动作，不直接改窗口）
    window.h/.cpp       WndProc、定时器、把菜单动作落到窗口状态
```

### 硬约束

- **单文件不超过 500 行**（当前最大 `ui/render.cpp` 约 200 行）。超了按职责拆到对应模块目录。
- 新增一类指标：在 `metrics/` 建一对 `xxx.h/.cpp`，在 `metrics.cpp` 的 `Init/Sample` 里挂一行，
  给 `Metrics` 加字段，再到 `ui/rows.h` 加一行。其余模块不用动。
- 新增一个配置项：改 `app/settings.h`（结构体 + 取值表）、`app/settings.cpp`（读写）、使用方模块。
  菜单命令 ID 分段集中在 `ui/menu.cpp`。

## 构建

需要 MinGW-w64 g++（支持 `-std=c++17`；本机为 scoop 装的 16.1.0）。Windows PowerShell 5.1
直接跑即可 —— 本机**没有安装** PowerShell 7（`Program Files\PowerShell`、WindowsApps、PATH 都没有）：

```powershell
.\build.ps1
```

`build.ps1` 递归收集 `src/**/*.cpp`，新增模块目录不用改脚本。

### 改这个脚本前必读：两个真实踩过的坑

1. **文件必须保存为 UTF-8 with BOM**。Windows PowerShell 5.1 读「无 BOM 的 `.ps1`」时按系统
   ANSI(GBK) 解码，中文注释产生的乱码有几率**连带吞掉行尾的 LF**，于是下一行的命令被并进注释里
   凭空消失，报成 `无法将 -finput-charset=UTF-8 项识别为 cmdlet、函数…` 这种指不到真凶的错误。
   重新另存此文件后，确认前 3 字节是 `EF BB BF`（`head -c 3 build.ps1 | od -c`）。
2. **编译参数写成数组，不要用反引号续行**。PowerShell 在参数模式下把逗号当列表分隔符，
   `-Wl,--gc-sections` 会被解析成「参数 `-Wl` + 参数列表 `--gc-sections`」而直接语法错
   `MissingArgument / 参数列表中缺少参量`（含逗号的参数一律要整体加引号）；
   而反引号续行一旦被任何不可见字符破坏，整条命令会被**静默拆成多条语句**，现象更迷惑。

想定位这类问题，用 AST 看 PowerShell 到底把脚本解析成了什么，比读报错快：

```powershell
$t=$null;$e=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile("build.ps1",[ref]$t,[ref]$e)
$e; $ast.EndBlock.Statements | % { "L$($_.Extent.StartLineNumber)-$($_.Extent.EndLineNumber)" }
```

或直接用 g++（bash，本仓库所有命令都验证过）：

```bash
g++ -std=c++17 -Os -s -static -mwindows -Wall -Wextra \
    -finput-charset=UTF-8 -fwide-exec-charset=UTF-16LE \
    -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    src/main.cpp src/app/*.cpp src/metrics/*.cpp src/ui/*.cpp -o widget.exe \
    -lgdi32 -luser32 -lpdh -liphlpapi -limm32
```

编译前先结束正在运行的 `widget.exe`，否则链接器写不进目标文件。

`metrics/net.cpp` 的 include 顺序不能改（`winsock2.h` → `ws2ipdef.h` → `iphlpapi.h` → `netioapi.h`），
`MIB_IF_TABLE2` 的声明依赖它。

运行后右键挂件可调整位置、刷新间隔、主题、卡片效果、字体大小与峰值保持。
