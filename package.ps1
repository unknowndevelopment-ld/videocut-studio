# Builds the branded installer (no MSI) and the update manifest.
#
#   .\package.ps1                                   # build + deploy + setup.exe
#   .\package.ps1 -Repo "you/videocut-studio"       # bake in the update feed
#
# Requires: CMake, Ninja, MinGW, Qt 6.8 (same toolchain as before) and Python.

Param(
    [string]$Repo = "",
    [string]$Version = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;C:\Qt\6.8.3\mingw_64\bin;" + $env:PATH

if (-not $Version) {
    $Version = (Select-String -Path "cpp\CMakeLists.txt" -Pattern 'project\(VideoCutStudio VERSION ([0-9.]+)').Matches[0].Groups[1].Value
}
if (-not $Repo) { $Repo = "OWNER/REPO" }
Write-Host "Packaging VideoCut Studio $Version for $Repo" -ForegroundColor Cyan

$outDir = Join-Path $PSScriptRoot "dist-cpp"
$stage = Join-Path $outDir "stage"
$setupRaw = Join-Path $outDir "VideoCutSetup.raw.exe"
$setup = Join-Path $outDir "VideoCutStudio-Setup-$Version.exe"

# ------------------------------------------------------------------- build
if (-not $SkipBuild) {
    cmake -S cpp -B cpp\build -G Ninja -DCMAKE_BUILD_TYPE=Release `
          -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\mingw_64 `
          -DVIDEOCUT_UPDATE_REPO="$Repo" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    cmake --build cpp\build
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

# ------------------------------------------------------------------ staging
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item "cpp\build\VideoCutStudio.exe" $stage -Force
& windeployqt.exe --release --no-translations --no-system-d3d-compiler --compiler-runtime `
    --dir $stage (Join-Path $stage "VideoCutStudio.exe") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

New-Item -ItemType Directory -Force -Path (Join-Path $stage "ffmpeg") | Out-Null
Copy-Item "vendor\ffmpeg\ffmpeg.exe", "vendor\ffmpeg\ffprobe.exe" (Join-Path $stage "ffmpeg") -Force
New-Item -ItemType Directory -Force -Path (Join-Path $stage "legal") | Out-Null
Copy-Item "legal\*" (Join-Path $stage "legal") -Force
Copy-Item "cpp\build\Uninstall.exe" $stage -Force

# ------------------------------------------- bootstrap (setup UI + Qt runtime)
$bootstrap = Join-Path $outDir "bootstrap"
if (Test-Path $bootstrap) { Remove-Item -Recurse -Force $bootstrap }
New-Item -ItemType Directory -Force -Path $bootstrap | Out-Null
Copy-Item "cpp\build\VideoCutSetup.exe" $bootstrap -Force
& windeployqt.exe --release --no-translations --no-system-d3d-compiler --compiler-runtime `
    --no-opengl-sw --dir $bootstrap (Join-Path $bootstrap "VideoCutSetup.exe") | Out-Null
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed for the setup UI" }

# ------------------------------------------------------- payload + installer
python tools\make_payload.py $stage "cpp\build\SetupStub.exe" $setup $Version $bootstrap
if ($LASTEXITCODE -ne 0) { throw "Payload build failed" }

# --------------------------------------------------------- update manifest
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item $setup).Length
$manifest = [ordered]@{
    version = $Version
    file    = (Split-Path $setup -Leaf)
    sha256  = $hash
    size    = $size
    repo    = $Repo
    built   = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $outDir "update.json") -Encoding UTF8

Write-Host ""
Write-Host "Installer : $setup" -ForegroundColor Green
Write-Host ("Size      : {0:N0} MB" -f ($size / 1MB)) -ForegroundColor Green
Write-Host "SHA-256   : $hash" -ForegroundColor Green
Write-Host "Manifest  : $outDir\update.json" -ForegroundColor Green
Write-Host ""
Write-Host "Next: tag v$Version, push, and attach $([System.IO.Path]::GetFileName($setup)) + update.json to the GitHub release." -ForegroundColor Yellow
