[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$StagePath,
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$VcpkgRoot,
    [string[]]$RuntimeDir = @(),
    [string]$MsvcRedistDir,
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
function Assert-PeArchitecture([string]$Path, [string]$Source) {
    $machine = Get-PeMachine $Path
    if ($machine -ne $expectedMachine) {
        throw ('Wrong PE architecture {0:X4} in {1} (from {2}); expected {3:X4} for {4}.' -f $machine, $Path, $Source, $expectedMachine, $Architecture)
    }
}
function Get-PeImports([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 0x40 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5A4D) { throw "Invalid PE file: $Path" }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -lt 0x40 -or $peOffset + 24 -gt $bytes.Length) { throw "Invalid PE header offset: $Path" }
    if ([BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) { throw "Invalid PE signature: $Path" }
    $coff = $peOffset + 4
    $sectionCount = [BitConverter]::ToUInt16($bytes, $coff + 2)
    $optionalSize = [BitConverter]::ToUInt16($bytes, $coff + 16)
    $optional = $coff + 20
    $magic = [BitConverter]::ToUInt16($bytes, $optional)
    if ($magic -eq 0x20B) { $dataDirectory = $optional + 112 }
    elseif ($magic -eq 0x10B) { $dataDirectory = $optional + 96 }
    else { throw "Unsupported PE optional header in $Path" }
    $importRva = [BitConverter]::ToUInt32($bytes, $dataDirectory + 8)
    if ($importRva -eq 0) { return @() }
    $sectionTable = $optional + $optionalSize
    $sections = @()
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $offset = $sectionTable + $i * 40
        if ($offset + 40 -gt $bytes.Length) { throw "Truncated section table in $Path" }
        $sections += [pscustomobject]@{
            VirtualAddress = [BitConverter]::ToUInt32($bytes, $offset + 12)
            VirtualSize    = [Math]::Max([BitConverter]::ToUInt32($bytes, $offset + 8), [BitConverter]::ToUInt32($bytes, $offset + 16))
            RawOffset      = [BitConverter]::ToUInt32($bytes, $offset + 20)
            RawSize        = [BitConverter]::ToUInt32($bytes, $offset + 16)
        }
    }
    function Convert-RvaToFileOffset([uint32]$Rva) {
        foreach ($section in $sections) {
            if ($Rva -ge $section.VirtualAddress -and $Rva -lt ($section.VirtualAddress + $section.VirtualSize)) {
                $delta = $Rva - $section.VirtualAddress
                if ($delta -ge $section.RawSize) { throw "RVA $Rva is not backed by file data in $Path" }
                return [int]($section.RawOffset + $delta)
            }
        }
        throw "RVA $Rva is not mapped by any section in $Path"
    }
    $names = [Collections.Generic.List[string]]::new()
    $descriptor = Convert-RvaToFileOffset $importRva
    while ($true) {
        if ($descriptor + 20 -gt $bytes.Length) { throw "Truncated import table in $Path" }
        $nameRva = [BitConverter]::ToUInt32($bytes, $descriptor + 12)
        if ($nameRva -eq 0) { break }
        $nameOffset = Convert-RvaToFileOffset $nameRva
        $end = $nameOffset
        while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) { $end++ }
        if ($end -ge $bytes.Length) { throw "Unterminated import name in $Path" }
        $names.Add([Text.Encoding]::ASCII.GetString($bytes, $nameOffset, $end - $nameOffset))
        $descriptor += 20
    }
    return $names
}
$skippedCrt = [Collections.Generic.List[string]]::new()

$isMsvc = $qt -match 'msvc'
if ($isMsvc) {
    & $windeployqt --release --no-translations --skip-plugin-types qmltooling --exclude-plugins qsqlmimer,qsqlodbc,qsqlpsql --no-compiler-runtime --qmldir (Join-Path $repo 'qml') $exe
} else {
    & $windeployqt --release --no-translations --skip-plugin-types qmltooling --exclude-plugins qsqlmimer,qsqlodbc,qsqlpsql --qmldir (Join-Path $repo 'qml') $exe
}
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
    $candidates = @($runtimeDlls | Where-Object Name -EQ $name)
    $source = $candidates[-1]
    Assert-PeArchitecture $source.FullName $source.DirectoryName
    Copy-Item -LiteralPath $source.FullName -Destination (Join-Path $stage $name) -Force
}
if (!($runtimeDlls.Name -match '(?i)ssl.*\.dll') -or !($runtimeDlls.Name -match '(?i)crypto.*\.dll')) {
    throw "Expected OpenSSL runtime DLLs were not found in: $($runtimeDirs -join ', '). Supply the matching OpenSSL bin directory with -RuntimeDir."
}
if ($vcpkg) {
    $libtorrentDlls = @($runtimeDlls | Where-Object Name -Match '(?i)(lib)?torrent.*\.dll')
    if ($libtorrentDlls.Count -eq 0) { throw "No libtorrent runtime DLLs found in the vcpkg triplet directory $vcpkgBin" }
}
$licenseDir = Join-Path $stage 'licenses'
if ($isMsvc) {
    if ([string]::IsNullOrWhiteSpace($MsvcRedistDir)) {
        throw 'MSVC builds require -MsvcRedistDir pointing to the matching official Microsoft.VC143.CRT redistributable directory.'
    }
    $msvcRedist = [IO.Path]::GetFullPath($MsvcRedistDir)
    if (!(Test-Path -LiteralPath $msvcRedist -PathType Container) -or
        (Split-Path $msvcRedist -Leaf) -ne 'Microsoft.VC143.CRT') {
        throw "Invalid MSVC app-local redistributable directory: $msvcRedist"
    }
    $expectedRedistArch = if ($Architecture -eq 'x64') { 'x64' } else { 'arm64' }
    if ((Split-Path (Split-Path $msvcRedist -Parent) -Leaf) -ne $expectedRedistArch) {
        throw "MSVC redistributable architecture does not match ${Architecture}: $msvcRedist"
    }
    $crtFiles = @(Get-ChildItem -LiteralPath $msvcRedist -File |
        Where-Object { $_.Extension -in '.dll', '.manifest' })
    $crtDlls = @($crtFiles | Where-Object Extension -EQ '.dll')
    if (!($crtDlls.Name -contains 'vcruntime140.dll') -or !($crtDlls.Name -contains 'msvcp140.dll')) {
        throw "The selected Microsoft.VC143.CRT directory lacks required redistributable DLLs: $msvcRedist"
    }
    foreach ($crtFile in $crtFiles) {
        if ($crtFile.Extension -eq '.dll') {
            $machine = Get-PeMachine $crtFile.FullName
            if ($machine -ne $expectedMachine) {
                if ($Architecture -eq 'arm64' -and $crtFile.Name -ieq 'vcruntime140_1.dll' -and $machine -eq 0x8664) {
                    Write-Warning "Skipping the x64 vcruntime140_1.dll shipped in the ARM64 Visual C++ redistributable (known Microsoft packaging defect): $($crtFile.FullName)"
                    $skippedCrt.Add($crtFile.Name)
                    continue
                }
                throw ('Wrong PE architecture {0:X4} in {1} (from {2}); expected {3:X4} for {4}.' -f $machine, $crtFile.FullName, $msvcRedist, $expectedMachine, $Architecture)
            }
        }
        Copy-Item -LiteralPath $crtFile.FullName -Destination (Join-Path $stage $crtFile.Name) -Force
    }
    $msvcLicenseDir = Join-Path $licenseDir 'MSVC'
    New-Item -ItemType Directory -Force -Path $msvcLicenseDir | Out-Null
    $msvcVersion = Split-Path (Split-Path $msvcRedist -Parent) -Leaf
    $crtSkipNote = ''
    if ($skippedCrt.Count -gt 0) {
        $crtSkipNote = @"

Excluded files: $($skippedCrt -join ', ')
The ARM64 Visual Studio redistributable layout above contains an x64 build of vcruntime140_1.dll. That binary was excluded instead of shipping a wrong-architecture file. Every staged ARM64 executable and DLL was verified not to import it.
"@
    }
    @"
The app-local Visual C++ 2015-2022 Redistributable files in this package were copied from the licensed Visual Studio redistributable layout:
$msvcRedist
Redistributable version directory: $msvcVersion
Architecture: $expectedRedistArch

Microsoft permits app-local deployment of licensed Visual C++ Redistributable DLLs. Central deployment with the official VC++ Redistributable installer is preferred for automatic servicing. This product uses app-local deployment so installation remains per-user and does not require administrator rights. The product maintainer is responsible for shipping updated runtime files with later application releases.
https://learn.microsoft.com/cpp/windows/choosing-a-deployment-method
https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files$crtSkipNote
"@ | Set-Content -LiteralPath (Join-Path $msvcLicenseDir 'README.txt') -Encoding utf8
}

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

$peFiles = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll' })
if ($peFiles.Count -eq 0) { throw 'No PE executables or DLLs found in package.' }
foreach ($file in $peFiles) {
    Assert-PeArchitecture $file.FullName 'staged package'
}
foreach ($skipped in $skippedCrt) {
    $importers = @($peFiles | Where-Object { @(Get-PeImports $_.FullName) -contains $skipped })
    if ($importers.Count -gt 0) {
        throw "Excluded $skipped because the ARM64 redistributable copy is x64, but these staged binaries import it: $($importers.FullName -join ', ')"
    }
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
