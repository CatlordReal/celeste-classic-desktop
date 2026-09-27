#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/dist/ccleste-windows"
mingw_bin="${MINGW_PREFIX:?Run under an MSYS2 MinGW shell}/bin"

rm -rf "$out"
mkdir -p "$out"

make -C "$root" CC=gcc OUT=ccleste-game.exe
cp "$root/ccleste-game.exe" "$out/ccleste-game.exe"
gcc -O2 -municode "$root/scripts/launch-windows.c" -o "$out/ccleste.exe"
cp -R "$root/data" "$out/data"
cp "$root/gamecontrollerdb.txt" "$out/"
cp "$root/README.md" "$out/"

copy_dependencies() {
    local binary="$1" dependency dependency_path
    while IFS= read -r dependency; do
        dependency_path="$mingw_bin/$dependency"
        if [[ -f "$dependency_path" && ! -f "$out/$dependency" ]]; then
            cp "$dependency_path" "$out/$dependency"
            copy_dependencies "$out/$dependency"
        fi
    done < <(objdump -p "$binary" | awk '/DLL Name:/{print $3}')
}
copy_dependencies "$out/ccleste-game.exe"
copy_dependencies "$out/ccleste.exe"

mkdir -p "$root/dist"
(
    cd "$root/dist"
    rm -f ccleste-windows.zip
    zip -9rq ccleste-windows.zip ccleste-windows
)
