#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="$root/dist/Celeste Classic.app"
macos="$app/Contents/MacOS"
resources="$app/Contents/Resources"
frameworks="$app/Contents/Frameworks"
sdl_prefix="$(brew --prefix sdl2)"
mixer_prefix="$(brew --prefix sdl2_mixer)"

rm -rf "$app"
mkdir -p "$macos" "$resources" "$frameworks"

make -C "$root" \
    CFLAGS="-Wall -g -O2 $(sdl2-config --cflags) -I$mixer_prefix/include/SDL2" \
    LDFLAGS="-L$sdl_prefix/lib -L$mixer_prefix/lib -lSDL2 -lSDL2_mixer -lm"
cp "$root/ccleste" "$resources/ccleste-bin"
cp -R "$root/data" "$resources/data"
cp "$root/gamecontrollerdb.txt" "$resources/"
cp "$root/README.md" "$resources/"
cc -O2 "$root/scripts/launch-macos.c" -o "$macos/ccleste"

cat > "$app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>ccleste</string>
  <key>CFBundleIdentifier</key><string>com.ccleste.classic</string>
  <key>CFBundleName</key><string>Celeste Classic</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
</dict></plist>
EOF

queue=("$resources/ccleste-bin")
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
            queue+=("$frameworks/$name")
        fi
    done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
done

for binary in "$resources/ccleste-bin" "$frameworks"/*; do
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
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$binary" 2>/dev/null || true
done

codesign --force --sign - --deep "$app"

mkdir -p "$root/dist"
archive="ccleste-macos-$(uname -m).zip"
(
    cd "$root/dist"
    rm -f "$archive"
    ditto -c -k --keepParent "Celeste Classic.app" "$archive"
)
