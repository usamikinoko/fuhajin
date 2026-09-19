# 本文件必须保存为 UTF-8 **with BOM**。
# Windows PowerShell 5.1 读「无 BOM 的 .ps1」时按系统 ANSI(GBK) 解码，中文注释的乱码
# 有几率把行尾的 LF 一起吞掉，于是下一行的命令被并进注释里凭空消失，报出
# 莫名其妙的 CommandNotFoundException（例如「无法将 -finput-charset=UTF-8 项识别为 …」）。
# 重新另存此文件后，务必确认 BOM 还在。

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

# 找 g++：优先 PATH，其次几处常见的本机安装位置（还找不到就在这里加一行）
$gxx = (Get-Command g++ -ErrorAction SilentlyContinue).Source
if (-not $gxx) {
    $gxx = @(
        "E:\01-application\20-scoop\apps\mingw\current\bin\g++.exe",
        "$env:USERPROFILE\scoop\apps\mingw\current\bin\g++.exe",
        "C:\msys64\mingw64\bin\g++.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $gxx) {
    "BUILD FAILED: 找不到 g++，请把 MinGW-w64 的 bin 目录加进 PATH"
    exit 1
}

# 编译参数写成数组，不用反引号续行。原因有两条，都踩过：
#   1) PowerShell 参数模式下逗号是列表分隔符，-Wl,--gc-sections 会被解析成
#      「参数 -Wl + 参数列表 --gc-sections」，直接语法错 MissingArgument；
#   2) 反引号续行一旦被任何不可见字符破坏，命令会被静默拆成多条语句，报错完全指不到真凶。
$flags = @(
    '-std=c++17', '-Os', '-s', '-static', '-mwindows', '-Wall', '-Wextra',
    '-finput-charset=UTF-8', '-fwide-exec-charset=UTF-16LE',
    '-fno-exceptions', '-fno-rtti', '-fno-asynchronous-unwind-tables',
    '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections'
)

Push-Location $root
# 递归收集源文件：加模块目录不用改这里
$src = Get-ChildItem -Path "$root\src" -Recurse -Filter *.cpp | ForEach-Object { $_.FullName }
& $gxx $flags $src -o widget.exe -lgdi32 -luser32 -lpdh -liphlpapi -limm32
$code = $LASTEXITCODE
Pop-Location

if ($code -eq 0) {
    $f = Get-Item "$root\widget.exe"
    "OK -> $($f.FullName)  {0:N0} KB" -f ($f.Length / 1KB)
} else {
    "BUILD FAILED (g++ 退出码 $code)；若提示 Permission denied，先结束运行中的 widget.exe"
    exit $code
}
