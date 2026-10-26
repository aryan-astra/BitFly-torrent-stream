[CmdletBinding()]
param(
    [string]$QtRoot = "$PSScriptRoot/../.tools/Qt/6.8.3/mingw_64",
    [string]$BuildPath = "$PSScriptRoot/../build",
    [string]$Suite = '.',
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$qt = (Resolve-Path -LiteralPath $QtRoot).Path
$build = (Resolve-Path -LiteralPath $BuildPath).Path
foreach ($required in @('bin/Qt6Core.dll', 'plugins/platforms/qoffscreen.dll', 'plugins/platforms/qwindows.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $qt $required) -PathType Leaf)) { throw "Incomplete Qt SDK: $required" }
}
$repo = (Resolve-Path -LiteralPath "$PSScriptRoot/..").Path
$runtime = Join-Path $repo '.tools/Qt/Tools/mingw1310_64/bin'
if (!(Test-Path -LiteralPath (Join-Path $runtime 'libstdc++-6.dll'))) { throw 'Matching MinGW 13 runtime not found.' }
$env:PATH = "$runtime;$qt/bin;$repo/.tools/openssl/ucrt64/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "$qt/plugins"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "$qt/plugins/platforms"
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = '1'
$env:QT_QPA_FONTDIR = "$env:WINDIR/Fonts"
Remove-Item Env:QML_IMPORT_PATH, Env:QML2_IMPORT_PATH -ErrorAction SilentlyContinue
if ($CheckOnly) { Write-Output "Qt SDK and offscreen plugin ready: $qt"; return }
& cmake -E env 'QT_QPA_PLATFORM=offscreen' ctest --test-dir $build --output-on-failure -R $Suite
if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }
