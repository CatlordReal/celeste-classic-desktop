#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob nocaseglob

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/dist/Celeste Classic.app"
macos="$app/Contents/MacOS"
resources="$app/Contents/Resources"
frameworks="$app/Contents/Frameworks"
player="$resources/player"
sdl_prefix="$(brew --prefix sdl2)"
mixer_prefix="$(brew --prefix sdl2_mixer)"
mpv_prefix="$(brew --prefix mpv)"
brew_cellar="$(brew --cellar)"
brew_prefix="$(brew --prefix)"

rm -rf "$app"
mkdir -p "$macos" "$resources" "$frameworks" "$player/licenses/mpv"

(cd "$root/records" && shasum -a 256 -c SHA256SUMS)

make -C "$root" \
    CFLAGS="-Wall -g -O2 $(sdl2-config --cflags) -I$mixer_prefix/include/SDL2" \
    LDFLAGS="-L$sdl_prefix/lib -L$mixer_prefix/lib -lSDL2 -lSDL2_mixer -lm"
cp "$root/ccleste" "$resources/ccleste-bin"
cp -R "$root/data" "$resources/data"
cp "$root/gamecontrollerdb.txt" "$resources/"
cp "$root/README.md" "$resources/"
cp "$root/assets/celeste-classic.icns" "$resources/"
cp -R "$root/records" "$resources/records"
cp "$(command -v mpv)" "$player/mpv"
cp "$root/player/README.md" "$player/README.md"
for license in "$mpv_prefix"/LICENSE* "$mpv_prefix"/COPYING* "$mpv_prefix"/COPYRIGHT* "$mpv_prefix"/NOTICE*; do
    [[ -f "$license" ]] && cp "$license" "$player/licenses/mpv/"
done
cc -O2 "$root/scripts/launch-macos.c" -o "$macos/ccleste"

cat > "$app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>ccleste</string>
  <key>CFBundleIdentifier</key><string>com.ccleste.classic</string>
  <key>CFBundleName</key><string>Celeste Classic</string>
  <key>CFBundleIconFile</key><string>celeste-classic.icns</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
</dict></plist>
EOF

copy_formula_licenses() {
    local binary="$1" resolved relative formula version prefix destination license found=0
    resolved="$(realpath "$binary")"
    [[ "$resolved" == "$brew_cellar"/* ]] || return 0
    relative="${resolved#"$brew_cellar"/}"
    formula="${relative%%/*}"
    relative="${relative#*/}"
    version="${relative%%/*}"
    prefix="$brew_cellar/$formula/$version"
    destination="$player/licenses/dependencies/$formula"
    for license in "$prefix"/LICENSE* "$prefix"/COPYING* "$prefix"/COPYRIGHT* "$prefix"/NOTICE*; do
        if [[ -f "$license" ]]; then
            mkdir -p "$destination"
            cp "$license" "$destination/"
            found=1
        fi
    done
    if [[ "$found" -eq 0 && "$formula" == libxmp && -f "$prefix/README" ]]; then
        mkdir -p "$destination"
        cp "$prefix/README" "$destination/README"
        found=1
    fi
    if [[ "$found" -eq 0 && -d "$root/player/licenses-extra/$formula" ]]; then
        mkdir -p "$destination"
        cp "$root/player/licenses-extra/$formula"/* "$destination/"
        found=1
    fi
    if [[ "$found" -eq 0 ]]; then
        echo "Missing license for bundled Homebrew formula: $formula" >&2
        exit 1
    fi
}

queue=("$resources/ccleste-bin" "$player/mpv")
while ((${#queue[@]})); do
    binary="${queue[0]}"
    queue=("${queue[@]:1}")
    while IFS= read -r dependency; do
        case "$dependency" in
            /usr/lib/*|/System/Library/*|@*) continue ;;
        esac
        name="$(basename "$dependency")"
        [[ -f "$dependency" ]] || { echo "Missing macOS dependency: $dependency" >&2; exit 1; }
        if [[ ! -f "$frameworks/$name" ]]; then
            cp "$dependency" "$frameworks/$name"
            copy_formula_licenses "$dependency"
            queue+=("$frameworks/$name")
        fi
    done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
done

for binary in "$resources/ccleste-bin" "$player/mpv" "$frameworks"/*; do
    [[ -f "$binary" ]] || continue
    if [[ "$binary" == "$frameworks"/* ]]; then
        install_name_tool -id "@rpath/$(basename "$binary")" "$binary"
    fi
    while IFS= read -r dependency; do
        name="$(basename "$dependency")"
        if [[ -f "$frameworks/$name" ]]; then
            install_name_tool -change "$dependency" "@rpath/$name" "$binary"
        fi
    done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
    if [[ "$binary" == "$player/mpv" ]]; then
        install_name_tool -add_rpath "@executable_path/../../Frameworks" "$binary" 2>/dev/null || true
    else
        install_name_tool -add_rpath "@executable_path/../Frameworks" "$binary" 2>/dev/null || true
    fi
    codesign --force --sign - "$binary"
done

codesign --force --sign - "$macos/ccleste"
codesign --force --sign - --deep "$app"

for binary in "$resources/ccleste-bin" "$player/mpv" "$frameworks"/*; do
    [[ -f "$binary" ]] || continue
    while IFS= read -r dependency; do
        if [[ "$dependency" == "$brew_prefix"/* ]]; then
            echo "Unbundled Homebrew dependency: $binary -> $dependency" >&2
            exit 1
        fi
    done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
done
[[ -f "$player/licenses/mpv/LICENSE.GPL" ]]
[[ -f "$player/licenses/mpv/LICENSE.LGPL" ]]

mkdir -p "$root/dist"
archive="ccleste-macos-$(uname -m).zip"
(
    cd "$root/dist"
    rm -f "$archive"
    ditto -c -k --keepParent "Celeste Classic.app" "$archive"
)
