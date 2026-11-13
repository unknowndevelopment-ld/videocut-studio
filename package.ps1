# Builds the Windows installer + update manifest.
#
#   .\package.ps1                        # build + deploy + MSI
#   .\package.ps1 -Repo "you/videocut"   # bake in the GitHub repo for updates
#
# Requires: CMake, Ninja, MinGW, Qt 6.8 (same as build.ps1) and WiX 3 in tools\wix.

Param(
    [string]$Repo = "",
    [string]$Version = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# ---------------------------------------------------------------- toolchain
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;C:\Qt\6.8.3\mingw_64\bin;" + $env:PATH
$wix = Join-Path $PSScriptRoot "tools\wix"
$candle = Join-Path $wix "candle.exe"
$light = Join-Path $wix "light.exe"
$heat = Join-Path $wix "heat.exe"
foreach ($tool in @($candle, $light, $heat)) {
    if (-not (Test-Path $tool)) { throw "WiX not found at $wix (download WiX 3.14 into tools\wix)" }
}

if (-not $Version) {
    $Version = (Select-String -Path "cpp\CMakeLists.txt" -Pattern 'project\(VideoCutStudio VERSION ([0-9.]+)').Matches[0].Groups[1].Value
}
if (-not $Repo) { $Repo = "OWNER/REPO" }
Write-Host "Packaging VideoCut Studio $Version for $Repo" -ForegroundColor Cyan

$stage = Join-Path $PSScriptRoot "dist-cpp\VideoCutStudio"
$installerDir = Join-Path $PSScriptRoot "dist-cpp"
$msi = Join-Path $installerDir "VideoCutStudio-$Version-x64.msi"

# ------------------------------------------------------------------- build
if (-not $SkipBuild) {
    cmake -S cpp -B cpp\build -G Ninja -DCMAKE_BUILD_TYPE=Release `
          -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\mingw_64 `
          -DVIDEOCUT_UPDATE_REPO="$Repo" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    cmake --build cpp\build
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

# ------------------------------------------------------------------ deploy
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item "cpp\build\VideoCutStudio.exe" $stage -Force
Copy-Item "assets\videocut.ico" (Join-Path $stage "app.ico") -Force

& windeployqt.exe --release --no-translations --no-system-d3d-compiler --compiler-runtime `
    --dir $stage (Join-Path $stage "VideoCutStudio.exe") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

New-Item -ItemType Directory -Force -Path (Join-Path $stage "ffmpeg") | Out-Null
Copy-Item "vendor\ffmpeg\ffmpeg.exe", "vendor\ffmpeg\ffprobe.exe" (Join-Path $stage "ffmpeg") -Force
New-Item -ItemType Directory -Force -Path (Join-Path $stage "legal") | Out-Null
Copy-Item "legal\*" (Join-Path $stage "legal") -Force

# -------------------------------------------------------------------- MSI
Write-Host "Harvesting files…" -ForegroundColor Cyan
$filesWxs = Join-Path $installerDir "files.wxs"
& $heat dir $stage -cg AppFiles -dr INSTALLDIR -gg -g1 -sfrag -srd -sreg `
    -var var.SourceDir -x (Join-Path $stage "VideoCutStudio.exe") -out $filesWxs
if ($LASTEXITCODE -ne 0) { throw "heat failed" }

$objDir = Join-Path $installerDir "obj"
if (Test-Path $objDir) { Remove-Item -Recurse -Force $objDir }
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

Write-Host "Compiling…" -ForegroundColor Cyan
& $candle -nologo -arch x64 -ext WixUIExtension `
    -dSourceDir="$stage" -dProductVersion="$Version" -dRepo="$Repo" `
    -out (Join-Path $objDir "product.wixobj") (Join-Path $PSScriptRoot "installer\Product.wxs")
if ($LASTEXITCODE -ne 0) { throw "candle failed" }

& $candle -nologo -arch x64 -ext WixUIExtension `
    -dSourceDir="$stage" -dProductVersion="$Version" -dRepo="$Repo" `
    -out (Join-Path $objDir "files.wixobj") $filesWxs
if ($LASTEXITCODE -ne 0) { throw "candle failed on harvested files" }

Write-Host "Linking…" -ForegroundColor Cyan
& $light -nologo -ext WixUIExtension -cultures:en-us `
    -out $msi (Join-Path $objDir "product.wixobj") (Join-Path $objDir "files.wixobj")
if ($LASTEXITCODE -ne 0) { throw "light failed" }

Remove-Item $filesWxs -Force

# --------------------------------------------------------- update manifest
$hash = (Get-FileHash $msi -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item $msi).Length
$manifest = [ordered]@{
    version = $Version
    file    = (Split-Path $msi -Leaf)
    sha256  = $hash
    size    = $size
    repo    = $Repo
    built   = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $installerDir "update.json") -Encoding UTF8

Write-Host ""
Write-Host "Installer : $msi" -ForegroundColor Green
Write-Host ("Size      : {0:N0} MB" -f ($size / 1MB)) -ForegroundColor Green
Write-Host "SHA-256   : $hash" -ForegroundColor Green
Write-Host "Manifest  : $installerDir\update.json" -ForegroundColor Green
Write-Host ""
Write-Host "Next: create a GitHub release tagged v$Version and attach the MSI" -ForegroundColor Yellow
Write-Host "      plus update.json. The app checks /releases/latest." -ForegroundColor Yellow
