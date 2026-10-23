# Third-party software

BitFly source is MIT licensed. Dependencies retain their own licenses.

| Component | Version baseline | License / source |
|---|---|---|
| Qt Core, GUI, Quick, Quick Controls 2, Network, SQL, Test | 6.8.3 | LGPL-3.0 or GPL/commercial; [source and licenses](https://code.qt.io/cgit/qt/qt5.git/tree/?h=v6.8.3) |
| libtorrent-rasterbar | 2.0.11 | BSD-3-Clause; [source](https://github.com/arvidn/libtorrent/tree/v2.0.11) |
| Boost | 1.86.0 baseline | Boost Software License 1.0; [source](https://www.boost.org/users/history/version_1_86_0.html) |
| OpenSSL (TLS trackers) | selected by toolchain lock | Apache-2.0 for 3.x; [source](https://github.com/openssl/openssl) |
| SQLite (Qt SQL plugin) | Qt bundled version | Public domain; [source](https://www.sqlite.org/copyright.html) |
| MinGW runtime, when distributed | Qt matching 13.1 | GCC GPL with runtime library exception and mingw-w64 licenses |

Qt must be dynamically linked in distributed packages. Users may replace the Qt
libraries with compatible modified builds. BitFly does not prohibit reverse
engineering needed to debug such modifications. Distributors must include all
applicable Qt and bundled third-party notices and provide corresponding source
or a compliant source offer under the licenses. Include the exact SDK license
files in the package, not just this summary. Qt tool modules used only during
building are not automatically runtime dependencies. Audit the deployed plugin
set before publishing. Release license auditing remains a required gate.

VLC is discovered on the user's machine and is not bundled. VLC branding and
licensing belong to VideoLAN. BitFly is not affiliated with VideoLAN.
