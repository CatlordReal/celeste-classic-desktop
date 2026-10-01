#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/dist/ccleste-windows"
mingw_bin="${MINGW_PREFIX:?Run under an MSYS2 MinGW shell}/bin"
player="$out/player"

rm -rf "$out"
mkdir -p "$out" "$player/licenses"

(cd "$root/records" && tr -d '\r' < SHA256SUMS | sha256sum -c -)

make -C "$root" CC=gcc OUT=ccleste-game.exe \
    LDFLAGS="$(sdl2-config --libs) -lSDL2_mixer -lm"
cp "$root/ccleste-game.exe" "$out/ccleste-game.exe"
gcc -O2 -municode "$root/scripts/launch-windows.c" -o "$out/ccleste.exe"
cp -R "$root/data" "$out/data"
cp "$root/gamecontrollerdb.txt" "$out/"
cp "$root/README.md" "$out/"
cp -R "$root/records" "$out/records"
cp "$mingw_bin/mpv.exe" "$player/mpv.exe"
cp "$root/player/README.md" "$player/README.md"

copy_package_licenses() {
    local binary="$1" package destination license_path
    package="$(pacman -Qoq "$binary" 2>/dev/null | head -n 1 || true)"
    [[ -n "$package" ]] || return 0
    destination="$player/licenses/${package//\//_}"
    while IFS= read -r license_path; do
        [[ -f "$license_path" ]] || continue
        mkdir -p "$destination"
        cp "$license_path" "$destination/$(basename "$license_path")"
    done < <(pacman -Ql "$package" | awk '$2 ~ /\/share\/licenses\// {print $2}')
}
copy_package_licenses "$mingw_bin/mpv.exe"

copy_dependencies() {
    local binary="$1" destination="$2" dependency dependency_path
    while IFS= read -r dependency; do
        dependency_path="$mingw_bin/$dependency"
        if [[ -f "$dependency_path" && ! -f "$destination/$dependency" ]]; then
            cp "$dependency_path" "$destination/$dependency"
            copy_package_licenses "$dependency_path"
            copy_dependencies "$destination/$dependency" "$destination"
        fi
    done < <(objdump -p "$binary" | awk '/DLL Name:/{print $3}')
}
copy_dependencies "$out/ccleste-game.exe" "$out"
copy_dependencies "$out/ccleste.exe" "$out"
copy_dependencies "$player/mpv.exe" "$player"

mkdir -p "$root/dist"
(
    cd "$root/dist"
    rm -f ccleste-windows.zip
    zip -9rq ccleste-windows.zip ccleste-windows
)

installer="$root/dist/ccleste-windows-installer"
rm -rf "$installer"
mkdir -p "$installer"
cp -R "$out"/. "$installer/"
cp "$root/scripts/Install-CelesteClassic.ps1" "$installer/"
cp "$root/scripts/Uninstall-CelesteClassic.ps1" "$installer/"
cp "$root/scripts/Install-CelesteClassic.cmd" "$installer/"
(
    cd "$root/dist"
    rm -f ccleste-windows-installer.zip
    zip -9rq ccleste-windows-installer.zip ccleste-windows-installer
)
