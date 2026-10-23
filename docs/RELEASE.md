# Windows builds and releases

The Windows workflow builds and tests native x64 and ARM64 packages. Pull requests and pushes to `main` run both architecture jobs plus the reusable source-quality workflow. A tag matching `v*` runs the same jobs and publishes a GitHub Release only after the quality and both architecture jobs have completed successfully. `workflow_dispatch` can also be run against a version tag to repeat the same gated build and publish step.

The build pins Qt 6.8.3 and aqtinstall 3.3.0. vcpkg is checked out at `84bab45d415d22042bd0b9081aea57f362da3f35` (the 2025.12.12 baseline), which resolves libtorrent to 2.0.11. x64 builds use the Windows Server 2022 runner and the MSVC 2022 x64 Qt kit. ARM64 builds run natively on `windows-11-arm` and use the MSVC 2022 ARM64 Qt kit with the installed v143 toolset.

Each architecture job runs the QtTest suite, installs the application into a clean staging directory, deploys the Qt DLLs and QML imports with `windeployqt`, copies the vcpkg runtime DLLs and license notices, validates that every staged EXE and DLL has the requested PE machine type, then creates a portable ZIP and an Inno Setup per-user installer. For MSVC builds, the workflow discovers the matching Visual Studio installation and copies only release CRT files from its architecture-matched `VC/Redist/MSVC/<version>/<architecture>/Microsoft.VC143.CRT` folder. It does not source runtime DLLs from `System32` or compiler `bin` directories. This per-user app-local deployment does not require administrator rights; the project must update those CRT files with later releases because app-local copies are not serviced by Windows Update. The package includes its MSVC runtime provenance and licensing notice. CI downloads the pinned Inno Setup release asset and verifies its Authenticode signature and publisher before installation. A SHA-256 file accompanies each architecture's outputs. The release job uploads only the artifacts from that successful workflow run.

## Local packaging

Build and test with the project's pinned Qt and vcpkg dependencies, then install to a staging directory. From the repository root, run:

```powershell
cmake --install build --config Release --prefix build/stage
.\scripts\package.ps1 `
  -Architecture x64 `
  -StagePath build/stage `
  -QtRoot .tools\Qt\6.8.3\msvc2022_64 `
  -VcpkgRoot .tools\vcpkg `
  -MsvcRedistDir '<Visual Studio>\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT' `
  -OutputDir dist
```

For a local build that uses a static MinGW libtorrent and external OpenSSL, pass the relevant dependency directories explicitly instead of `-VcpkgRoot`; for example `-RuntimeDir @('.tools\openssl\ucrt64\bin', '<matching MinGW bin directory>')`. The script copies DLLs from every supplied directory and checks their PE architecture. Use `-Architecture arm64` with the ARM64 kit and its matching dependency directories for a native ARM64 build. Install Inno Setup 6 to generate the installer. If it is absent, local packaging reports that installer generation was skipped; pass `-RequireInstaller` when the installer is a required output. CI always requires it.

The Qt deployment contains Qt's versioned SPDX records and the project third-party notices. Review those records and notices whenever Qt or vcpkg dependencies change. Qt's Windows deployment and licensing guides describe the DLL deployment model, module terms, and third-party components: [Qt 6.8 Windows deployment](https://doc.qt.io/qt-6.8/windows-deployment.html), [Qt 6.8 licensing](https://doc.qt.io/qt-6.8/licensing.html), and [Qt 6.8 third-party components](https://doc.qt.io/qt-6.8/licenses-used-in-qt.html).

The packages are not code-signed because the project has no signing certificate configured. Windows may show the normal publisher warning until a release certificate is provisioned and signing is added. MSVC app-local CRT deployment is supported by Microsoft, but central deployment with the official redistributable is preferred because Windows Update can service it; app-local runtime files must be refreshed by the project in later releases. See [Microsoft's deployment-method guidance](https://learn.microsoft.com/cpp/windows/choosing-a-deployment-method) and [Visual C++ file redistribution guidance](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files).

## Publishing

Create and push a signed version tag such as `v0.1.0`; do not move or delete a
tag that has already been published. The release job runs only for `v*` tags,
waits for both native builds and the source-quality job, checks out the tag,
downloads that run's six architecture artifacts, verifies the expected count, and
creates the GitHub Release. A hyphen in the tag name marks the release as a
prerelease. Release notes are generated from the repository history; review the
generated body before announcing a release. The job creates a new release and
does not update or replace an existing one.

Do not upload locally rebuilt or unverified binaries to a release. The local
MinGW x64 package described in STATUS.md is a development artifact and differs
from the MSVC packages produced by CI; only CI artifacts belong on GitHub.

## Preview 1 publication (2026-10-23)

`v0.1.0-preview.1` points to `9c27337` and is a GPG-signed annotated tag. Run
`39977378333` passed source quality and both architecture jobs but its publish
step failed because the release job had no repository checkout for
`gh release create --verify-tag`. Commit `9003a06` added the checkout. The six
verified assets from that run were uploaded through the GitHub API as
prerelease `422010008` and downloaded again to confirm their SHA-256 values.
That is a documented manual recovery, not the normal automated path.

The corrected automatic publish path has not been exercised by a subsequent tag.
Validate it on the next intentionally created preview or stable tag; do not move
`v0.1.0-preview.1` to test it. If a future release job fails after creating a
draft or release, inspect its state before retrying rather than deleting a
published tag.
