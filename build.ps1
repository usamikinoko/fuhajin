$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
Push-Location $root
g++ -std=c++17 -Os -s -static -mwindows `
    -finput-charset=UTF-8 -fwide-exec-charset=UTF-16LE `
    -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables `
    -ffunction-sections -fdata-sections -Wl,--gc-sections `
    src\widget.cpp src\metrics.cpp -o widget.exe `
    -lgdi32 -luser32 -lpdh -liphlpapi -limm32
Pop-Location
if ($LASTEXITCODE -eq 0) {
    $f = Get-Item "$root\widget.exe"
    "OK -> $($f.FullName)  {0:N0} KB" -f ($f.Length / 1KB)
} else {
    "BUILD FAILED"
}
