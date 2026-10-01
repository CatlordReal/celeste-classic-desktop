# Bundled player

Release archives include mpv under GPL-2.0-or-later and LGPL-2.1-or-later components. License files for mpv and copied runtime dependencies are in `licenses/` beside this file.

- Upstream source: https://github.com/mpv-player/mpv
- Windows package source: https://packages.msys2.org/base/mingw-w64-mpv
- macOS formula source: https://github.com/Homebrew/homebrew-core/blob/HEAD/Formula/m/mpv.rb

The release workflow obtains the platform binary and its runtime libraries from the package source listed above. This player is used only for bundled world-record recordings.

The macOS package uses additional upstream notices when Homebrew omits them: [GLib 2.88.0](https://github.com/GNOME/glib/tree/2.88.0/LICENSES), [libunibreak](https://github.com/adah1972/libunibreak/blob/master/LICENCE), and libxmp's installed README.
