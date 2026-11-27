# Build Slate on Windows: compile, test, and stage a runnable copy in dist\Slate.
#
#   .\build.ps1               build + test, produce dist\Slate and a portable zip
#   .\build.ps1 -InstallQt    also download Qt if it is not installed (needs uv)
#   .\build.ps1 -Msi          also produce the MSI installer (needs WiX Toolset v3)
#
# Requires Visual Studio (Desktop development with C++) and CMake 3.19+.

param(
  [string]$QtPath = $env:QT_PATH,
  [string]$WixBin = $(if ($env:WIX) { Join-Path $env:WIX "bin" }),
  [switch]$InstallQt,
  [switch]$Msi
)

$ErrorActionPreference = "Stop"

$qtVersion = "6.10.3"
$qtArch = "msvc2022_64"

$repoRoot = $PSScriptRoot
$version = (Get-Content -Path (Join-Path $repoRoot "VERSION.txt") -TotalCount 1).Trim()
$buildDir = Join-Path $repoRoot "build"
$distDir = Join-Path $repoRoot "dist"
$stageDir = Join-Path $distDir "Slate"

# --- Locate Qt (QT_PATH, else the newest install under C:\Qt) ---
if (-not $QtPath) {
  $QtPath = Get-ChildItem "C:\Qt\6.*\$qtArch" -Directory -ErrorAction SilentlyContinue |
    Sort-Object { [version]$_.Parent.Name } | Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $QtPath -or -not (Test-Path (Join-Path $QtPath "bin\Qt6WebEngineCore.dll"))) {
  if (-not $InstallQt) {
    throw "Qt 6 with WebEngine not found. Re-run with -InstallQt to download it (about 3 GB, needs uv), or set QT_PATH."
  }
  uvx --from aqtinstall aqt install-qt windows desktop $qtVersion "win64_$qtArch" -m qtwebengine qtwebchannel qtpositioning -O C:\Qt
  if ($LASTEXITCODE -ne 0) { throw "Qt install failed." }
  $QtPath = "C:\Qt\$qtVersion\$qtArch"
}
Write-Host "Using Qt at $QtPath"

# --- Build and test ---
cmake -S $repoRoot -B $buildDir -DCMAKE_PREFIX_PATH="$QtPath"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
ctest --test-dir $buildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }

# --- Stage a self-contained copy ---
Remove-Item -Path $stageDir -Recurse -Force -ErrorAction SilentlyContinue
cmake --install $buildDir --config Release --prefix $stageDir
if ($LASTEXITCODE -ne 0) { throw "CMake install failed." }
& (Join-Path $QtPath "bin\windeployqt.exe") --release --no-translations --no-compiler-runtime (Join-Path $stageDir "Slate.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

$zipPath = Join-Path $distDir "Slate-$version-windows.zip"
Compress-Archive -Path $stageDir -DestinationPath $zipPath -Force
Write-Host "Run it:       $(Join-Path $stageDir 'Slate.exe')"
Write-Host "Portable zip: $zipPath"

if (-not $Msi) { return }

# --- MSI installer ---
if (-not $WixBin -or -not (Test-Path (Join-Path $WixBin "candle.exe"))) {
  throw "WiX Toolset v3 not found. Install it from https://wixtoolset.org/docs/wix3/ or pass -WixBin."
}

# MSI versions are x.x.x.x, each part numeric
$msiVersion = (($version -split "\.") + @("0", "0", "0"))[0..3] -join "."
if ($msiVersion -notmatch "^\d+\.\d+\.\d+\.\d+$") {
  throw "VERSION.txt must be numeric for MSI (x.x.x). Current: $version"
}

$wixBuildDir = Join-Path $repoRoot "installer\_build"
New-Item -ItemType Directory -Force -Path $wixBuildDir | Out-Null

$harvest = Join-Path $wixBuildDir "Harvested.wxs"
& (Join-Path $WixBin "heat.exe") dir $stageDir -cg HarvestedFiles -dr INSTALLFOLDER -var var.SourceDir -gg -srd -sfrag -sreg -out $harvest
if ($LASTEXITCODE -ne 0) { throw "WiX heat failed." }

# Mark harvested components as 64-bit to avoid ICE80 when installing to ProgramFiles64Folder.
# Only target <Component ...> (not <ComponentGroup>).
$harvestContent = Get-Content -Path $harvest -Raw
$harvestContent = $harvestContent -replace '<Component\s(?![^>]*\bWin64=)', '<Component Win64="yes" '
$harvestContent | Set-Content -Path $harvest -Encoding UTF8

$wxsResolved = Join-Path $wixBuildDir "Slate.resolved.wxs"
$iconPath = Join-Path $repoRoot "src\icons\slate.ico"
$licensePath = Join-Path $repoRoot "installer\license.rtf"

$wxsContent = Get-Content -Path (Join-Path $repoRoot "installer\Slate.wxs") -Raw
$wxsContent = $wxsContent -replace "\$\(\s*var\.ProductVersion\s*\)", $msiVersion
$wxsContent = $wxsContent -replace 'SourceFile="[^"]*slate\.ico"', ('SourceFile="' + $iconPath + '"')
$wxsContent = $wxsContent -replace 'Value="license\.rtf"', ('Value="' + $licensePath + '"')
$wxsContent | Set-Content -Path $wxsResolved -Encoding UTF8

$wxsObj = Join-Path $wixBuildDir "Slate.wixobj"
$harvestObj = Join-Path $wixBuildDir "Harvested.wixobj"
& (Join-Path $WixBin "candle.exe") -nologo -dSourceDir="$stageDir" -out $wxsObj $wxsResolved
if ($LASTEXITCODE -ne 0) { throw "WiX candle failed (main wxs)." }
& (Join-Path $WixBin "candle.exe") -nologo -dSourceDir="$stageDir" -out $harvestObj $harvest
if ($LASTEXITCODE -ne 0) { throw "WiX candle failed." }

$msiPath = Join-Path $distDir "Slate-$version-windows.msi"
& (Join-Path $WixBin "light.exe") -nologo -ext WixUIExtension -out $msiPath -b "$stageDir" $wxsObj $harvestObj
if ($LASTEXITCODE -ne 0) { throw "WiX light failed." }

Write-Host "MSI:          $msiPath"
