# Build

Use the Qt kit's matching compiler/runtime. Do not mix the machine's arbitrary
MinGW installation with the Qt MinGW kit. Local development uses Qt 6.8.3,
MinGW 13.1, CMake 3.24+, Ninja, Boost 1.86.0 headers and libtorrent 2.0.11.
MSVC 2022 and vcpkg are used by the Windows CI matrix. Dependencies must match
the target architecture. No JavaScript or Python runtime is used by the app.

Local source-dependency build, after obtaining the official dependencies:

```powershell
$env:PATH="C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.8.3\mingw_64\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64 `
  -DBITFLY_LIBTORRENT_SOURCE=C:/deps/libtorrent `
  -DBITFLY_BOOST_ROOT=C:/deps/boost_1_86_0 `
  -DOPENSSL_ROOT_DIR=C:/deps/openssl
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
.\build\BitFly.exe
```

Alternatively, provide installed LibtorrentRasterbar through CMAKE_PREFIX_PATH
or the vcpkg toolchain and omit BITFLY_LIBTORRENT_SOURCE/BITFLY_BOOST_ROOT.
Use libtorrent v2.0.11 commit `9d7443f467147d1784fb7516d2a882db1abb5a8b`
with its pinned submodules. OpenSSL enables HTTPS trackers; Qt Network can use
Windows Schannel. Packaging must deploy all discovered DLL dependencies and Qt
plugins. See scripts/package.ps1 and the release guide.
