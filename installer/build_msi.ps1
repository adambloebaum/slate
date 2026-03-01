param(
  [string]$QtPath = $env:QT_PATH,
  [string]$WixBin = $env:WIX_BIN,
  [string]$Generator = "Visual Studio 18 2026",
  [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$versionPath = Join-Path $repoRoot "VERSION.txt"
$rawVersion = (Get-Content -Path $versionPath -TotalCount 1).Trim()
$rawVersion = $rawVersion.TrimStart("v","V")
$parts = $rawVersion -split "\."
$parts = $parts + @("0","0","0","0")
$msiVersion = ($parts[0..3] -join ".")

foreach ($part in $msiVersion -split "\.") {
  if ($part -notmatch "^\d+$") {
    throw "VERSION.txt must be numeric for MSI (x.x.x.x). Current: $rawVersion"
  }
  if ([int]$part -gt 65534) {
    throw "MSI version parts must be <= 65534. Current: $msiVersion"
  }
}

if (-not $QtPath) {
  $QtPath = "C:\Qt\6.10.1\msvc2022_64"
}

$windeployqt = Join-Path $QtPath "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
  throw "windeployqt not found at $windeployqt. Set QT_PATH to your Qt install."
}

if (-not $WixBin) {
  $defaultWix = "C:\Program Files (x86)\WiX Toolset v3.11\bin"
  if (Test-Path (Join-Path $defaultWix "candle.exe")) {
    $WixBin = $defaultWix
  } else {
    $candle = Get-Command candle.exe -ErrorAction SilentlyContinue
    if ($candle) {
      $WixBin = Split-Path $candle.Path
    }
  }
}

if (-not $WixBin) {
  throw "WiX Toolset v3 not found. Install WiX v3.11 and set WIX_BIN or add candle.exe to PATH."
}

$buildDir = Join-Path $repoRoot "build"
$distDir = Join-Path $repoRoot "dist"
$stageDir = Join-Path $distDir "Slate"
$wixBuildDir = Join-Path $repoRoot "installer\_build"

Remove-Item -Path $stageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
New-Item -ItemType Directory -Force -Path $wixBuildDir | Out-Null

cmake --fresh -S $repoRoot -B $buildDir -G $Generator -A $Arch -DCMAKE_PREFIX_PATH="$QtPath"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

Copy-Item (Join-Path $buildDir "Release\Slate.exe") $stageDir -Force
Copy-Item (Join-Path $repoRoot "LICENSE") $stageDir -Force
Copy-Item (Join-Path $repoRoot "VERSION.txt") $stageDir -Force
& $windeployqt --release --no-translations (Join-Path $stageDir "Slate.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

$harvest = Join-Path $wixBuildDir "Harvested.wxs"
& (Join-Path $WixBin "heat.exe") dir $stageDir -cg HarvestedFiles -dr INSTALLFOLDER -var var.SourceDir -gg -srd -sfrag -sreg -out $harvest
if ($LASTEXITCODE -ne 0) { throw "WiX heat failed." }

# Mark harvested components as 64-bit to avoid ICE80 when installing to ProgramFiles64Folder.
# Only target <Component ...> (not <ComponentGroup>).
$harvestContent = Get-Content -Path $harvest -Raw
$harvestContent = $harvestContent -replace '<Component\s(?![^>]*\bWin64=)', '<Component Win64="yes" '
$harvestContent | Set-Content -Path $harvest -Encoding UTF8

$wxsTemplate = Join-Path $repoRoot "installer\Slate.wxs"
$wxsResolved = Join-Path $wixBuildDir "Slate.resolved.wxs"
$iconPath = Join-Path $repoRoot "src\icons\slate.ico"
$licensePath = Join-Path $repoRoot "installer\license.rtf"

$wxsContent = Get-Content -Path $wxsTemplate -Raw
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

$msiName = "Slate-$rawVersion-Setup.msi"
$msiPath = Join-Path $distDir $msiName
& (Join-Path $WixBin "light.exe") -nologo -ext WixUIExtension -out $msiPath `
  -b "$stageDir" `
  $wxsObj `
  $harvestObj
if ($LASTEXITCODE -ne 0) { throw "WiX light failed." }

Write-Host "MSI created: $msiPath"
