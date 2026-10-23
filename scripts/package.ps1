[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$StagePath,
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$VcpkgRoot,
    [string[]]$RuntimeDir = @(),
    [Parameter(Mandatory)][string]$OutputDir,
    [string]$Version,
    [string]$InnoCompiler,
    [switch]$RequireInstaller
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stage = [IO.Path]::GetFullPath($StagePath)
$qt = [IO.Path]::GetFullPath($QtRoot)
$vcpkg = if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) { $null } else { [IO.Path]::GetFullPath($VcpkgRoot) }
$output = [IO.Path]::GetFullPath($OutputDir)
if (!(Test-Path -LiteralPath $stage -PathType Container)) { throw "Staging directory not found: $stage" }
if (!(Test-Path -LiteralPath $qt -PathType Container)) { throw "Qt kit not found: $qt" }
if ([string]::IsNullOrWhiteSpace($Version)) {
    $cmake = Get-Content -LiteralPath (Join-Path $repo 'CMakeLists.txt') -Raw
    if ($cmake -notmatch 'project\(BitFly VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Could not read BitFly version from CMakeLists.txt.' }
    $Version = $Matches[1]
}
$exe = Join-Path $stage 'BitFly.exe'
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Installed executable not found: $exe" }
$windeployqt = Join-Path $qt 'bin\windeployqt.exe'
if (!(Test-Path -LiteralPath $windeployqt -PathType Leaf)) { throw "windeployqt not found: $windeployqt" }

& $windeployqt --release --no-translations --qmldir (Join-Path $repo 'qml') $exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

$triplet = if ($Architecture -eq 'x64') { 'x64-windows' } else { 'arm64-windows' }
$runtimeDirs = [Collections.Generic.List[string]]::new()
if ($vcpkg) {
    if (!(Test-Path -LiteralPath $vcpkg -PathType Container)) { throw "vcpkg root not found: $vcpkg" }
    $vcpkgBin = Join-Path $vcpkg "installed\$triplet\bin"
    if (!(Test-Path -LiteralPath $vcpkgBin -PathType Container)) { throw "vcpkg runtime directory not found: $vcpkgBin" }
    $runtimeDirs.Add($vcpkgBin)
}
foreach ($dir in $RuntimeDir) {
    $resolved = [IO.Path]::GetFullPath($dir)
    if (!(Test-Path -LiteralPath $resolved -PathType Container)) { throw "Runtime directory not found: $resolved" }
    $runtimeDirs.Add($resolved)
}
if ($runtimeDirs.Count -eq 0) { throw 'Supply -VcpkgRoot or one or more -RuntimeDir values to stage native dependencies.' }
$runtimeDlls = @($runtimeDirs | ForEach-Object { Get-ChildItem -LiteralPath $_ -Filter '*.dll' -File })
if ($runtimeDlls.Count -eq 0) { throw "No runtime DLLs found in: $($runtimeDirs -join ', ')" }
foreach ($name in $runtimeDlls.Name | Sort-Object -Unique) {
    $matches = @($runtimeDlls | Where-Object Name -EQ $name)
    Copy-Item -LiteralPath $matches[-1].FullName -Destination (Join-Path $stage $name) -Force
}
if (!($runtimeDlls.Name -match '(?i)ssl.*\.dll') -or !($runtimeDlls.Name -match '(?i)crypto.*\.dll')) {
    throw "Expected OpenSSL runtime DLLs were not found in: $($runtimeDirs -join ', '). Supply the matching OpenSSL bin directory with -RuntimeDir."
}
if ($vcpkg) {
    $libtorrentDlls = @($runtimeDlls | Where-Object Name -Match '(?i)(lib)?torrent.*\.dll')
    if ($libtorrentDlls.Count -eq 0) { throw "No libtorrent runtime DLLs found in the vcpkg triplet directory $vcpkgBin" }
}
if ($qt -match 'msvc') {
    foreach ($crt in 'vcruntime140.dll', 'msvcp140.dll') {
        if (!(Test-Path -LiteralPath (Join-Path $stage $crt) -PathType Leaf)) {
            throw "windeployqt did not stage app-local MSVC runtime $crt."
        }
    }
}

$licenseDir = Join-Path $stage 'licenses'
$qtLicenseDir = Join-Path $licenseDir 'Qt'
$vcpkgLicenseDir = Join-Path $licenseDir 'vcpkg'
New-Item -ItemType Directory -Force -Path $qtLicenseDir, $vcpkgLicenseDir | Out-Null
$qtSbom = Join-Path $qt 'sbom'
if (!(Test-Path -LiteralPath $qtSbom -PathType Container)) { throw "Qt SBOM directory missing: $qtSbom" }
$sbomFiles = @(Get-ChildItem -LiteralPath $qtSbom -File | Where-Object { $_.Name -match '\.(spdx|json)$' })
if ($sbomFiles.Count -eq 0) { throw "No Qt SPDX license/SBOM files found in $qtSbom" }
Copy-Item -LiteralPath $sbomFiles.FullName -Destination $qtLicenseDir -Force
$projectLicenseDir = Join-Path $repo 'licenses'
if (!(Test-Path -LiteralPath $projectLicenseDir -PathType Container)) { throw "Project license directory missing: $projectLicenseDir" }
Get-ChildItem -LiteralPath $projectLicenseDir | Copy-Item -Destination $licenseDir -Recurse -Force
$qtLicenseIndex = @"
Qt 6.8.3 is dynamically linked. Qt modules and bundled components carry their own license terms.
The shipped SPDX records in this directory identify the Qt and Qt third-party components used by this Qt kit.
See the Qt 6.8 licensing index: https://doc.qt.io/qt-6.8/licensing.html
Third-party components: https://doc.qt.io/qt-6.8/licenses-used-in-qt.html
Qt source: https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v6.8.3
Review THIRD_PARTY_NOTICES.md for the product's notices and license information.
"@
Set-Content -LiteralPath (Join-Path $qtLicenseDir 'README.txt') -Value $qtLicenseIndex -Encoding utf8
$thirdParty = Join-Path $stage 'THIRD_PARTY_NOTICES.md'
if (!(Test-Path -LiteralPath $thirdParty -PathType Leaf)) { throw 'THIRD_PARTY_NOTICES.md must be installed into the package.' }
$shareDir = if ($vcpkg) { Join-Path $vcpkg "installed\$triplet\share" } else { $null }
if ($shareDir -and (Test-Path -LiteralPath $shareDir -PathType Container)) {
    foreach ($copyright in Get-ChildItem -LiteralPath $shareDir -Directory | ForEach-Object { Join-Path $_.FullName 'copyright' } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }) {
        $name = (Split-Path (Split-Path $copyright -Parent) -Leaf) + '-copyright.txt'
        Copy-Item -LiteralPath $copyright -Destination (Join-Path $vcpkgLicenseDir $name) -Force
    }
}

function Get-PeMachine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Invalid PE file: $Path" }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 64 -or $peOffset + 6 -gt $stream.Length) { throw "Invalid PE header offset: $Path" }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Invalid PE signature: $Path" }
        return $reader.ReadUInt16()
    } finally { $stream.Dispose() }
}
$expectedMachine = if ($Architecture -eq 'x64') { 0x8664 } else { 0xAA64 }
$peFiles = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll' })
if ($peFiles.Count -eq 0) { throw 'No PE executables or DLLs found in package.' }
foreach ($file in $peFiles) {
    $machine = Get-PeMachine $file.FullName
    if ($machine -ne $expectedMachine) { throw ('Wrong PE architecture {0:X4} in {1}; expected {2:X4}.' -f $machine, $file.FullName, $expectedMachine) }
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
$archive = Join-Path $output "BitFly-$Version-windows-$Architecture.zip"
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal

if ([string]::IsNullOrWhiteSpace($InnoCompiler)) {
    $candidate = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ($candidate) { $InnoCompiler = $candidate.Source }
    else {
        $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
        $candidatePath = Join-Path $programFilesX86 'Inno Setup 6\ISCC.exe'
        if (Test-Path -LiteralPath $candidatePath -PathType Leaf) { $InnoCompiler = $candidatePath }
    }
}
if ($InnoCompiler -and (Test-Path -LiteralPath $InnoCompiler -PathType Leaf)) {
    $iss = Join-Path $repo 'packaging\BitFly.iss'
    & $InnoCompiler "/DSourceDir=$stage" "/DOutputDir=$output" "/DAppVersion=$Version" "/DAppArch=$Architecture" $iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }
} elseif ($RequireInstaller) {
    throw 'Inno Setup compiler was not found; install Inno Setup 6 or pass -InnoCompiler.'
} else {
    Write-Warning 'Inno Setup compiler was not found; portable ZIP was created and installer generation was skipped.'
}

$artifacts = @(Get-ChildItem -LiteralPath $output -File | Where-Object { $_.Name -like "BitFly-$Version-windows-$Architecture*" -and $_.Extension -in '.zip', '.exe' })
if ($artifacts.Count -eq 0) { throw 'No release artifacts were created.' }
$hashes = $artifacts | ForEach-Object { "$(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256 | Select-Object -ExpandProperty Hash)  $($_.Name)" }
Set-Content -LiteralPath (Join-Path $output "BitFly-$Version-windows-$Architecture-SHA256SUMS.txt") -Value $hashes -Encoding ascii
Write-Host "Packaged $($peFiles.Count) PE files for $Architecture; artifacts: $($artifacts.Count)"
