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
