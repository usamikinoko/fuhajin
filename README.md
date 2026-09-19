# Fuhajin Widget

轻量 Windows 桌面挂件，显示 CPU / GPU / 网速 / 内存 等实时数据。

## 关于 CPUT 行

CPUT 显示的是**芯片片上最高温度**，也就是通常说的 Tctl/Tdie 口径：

- 优先取 ACPI 热区 `\Thermal Zone Information(*)\High Precision Temperature`（0.1 K），不存在时回退到 `\Thermal Zone Information(*)\Temperature`（1 K），统一到 ℃。
- 再与 iGPU 温度传感器（`D3DKMTQueryAdapterInfo(type=62)`，同一颗 APU 裸片）取较大值，更接近真实片上峰值。
- 对读数做短窗口峰值保持（默认 5 秒），避免采样点刚好落在尖峰之间。
- 当当前瞬时温度明显低于保持中的峰值时，数值后会显示 ▲，表示仍在峰值后的衰减期。

右键菜单可以关闭或切换峰值保持窗口。

## 构建

需要 MinGW-w64 g++（支持 `-std=c++17`）：

```bash
powershell -ExecutionPolicy Bypass -File build.ps1
```

或直接用 g++：

```bash
g++ -std=c++17 -Os -s -static -mwindows \
    -finput-charset=UTF-8 -fwide-exec-charset=UTF-16LE \
    -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    src/widget.cpp src/metrics.cpp -o widget.exe \
    -lgdi32 -luser32 -lpdh -liphlpapi -limm32
```

运行后右键挂件可调整位置、刷新间隔、主题等。
