# Builds the example and packs it into build\SampleAppSetup.exe, at the repository's root.

$ErrorActionPreference = "Stop"
$root  = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$build = Join-Path $root "build"

cmake -S $root -B $build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $build --config MinSizeRel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$pack = Join-Path $build "MinSizeRel\si_pack.exe"
$bin  = Join-Path $build "examples\sample-app\MinSizeRel"

# English first: a string a locale lacks falls back to the first locale.
$locales = @()
Get-ChildItem (Join-Path $PSScriptRoot "locales") -Filter "*.txt" |
    Sort-Object @{ Expression = { $_.BaseName -ne "en" } }, BaseName |
    ForEach-Object { $locales += "--locale", $_.BaseName, $_.FullName }

# The uninstaller carries the locales too, and the installer writes it beside the application as uninstall.exe.
& $pack "$bin\SampleUninstall.exe" -o "$bin\uninstall.exe" @locales
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$setup = Join-Path $build "SampleAppSetup.exe"
& $pack "$bin\SampleSetup.exe" -o $setup --add "$bin\SampleApp.exe" --add "$bin\uninstall.exe" @locales
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Created $setup ($([math]::Round((Get-Item $setup).Length / 1KB)) KB)" -ForegroundColor Green
