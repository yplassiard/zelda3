#!/bin/bash
set -eu

APP_NAME="Zelda3"
BUNDLE="${APP_NAME}.app"
INI="zelda3.ini"

echo "=== Building zelda3 (universal binary) ==="

# Use bundled SDL2.framework instead of Homebrew (universal: arm64 + x86_64)
# Symlink lets internal SDL2 headers find each other via <SDL2/...> paths
ln -sfn SDL2.framework/Headers SDL2
SDL2_CFLAGS="-I SDL2.framework/Headers -D_THREAD_SAFE"
SDL2_LIBS="-F . -framework SDL2 -Wl,-rpath,@executable_path/../Frameworks -lm -framework AVFoundation -framework Foundation -framework Cocoa -framework UniformTypeIdentifiers -lobjc"

build_arch() {
  make clean_obj
  make CC="cc -arch $1" \
    CFLAGS="-O2 -Werror -I . ${SDL2_CFLAGS} -DSYSTEM_VOLUME_MIXER_AVAILABLE=0" \
    SDLFLAGS="${SDL2_LIBS}"
  mv zelda3 "zelda3-$1"
}

build_arch arm64
build_arch x86_64

lipo -create zelda3-arm64 zelda3-x86_64 -output zelda3
rm zelda3-arm64 zelda3-x86_64
echo "Architectures: $(lipo -archs zelda3)"

echo "=== Creating ${BUNDLE} ==="
rm -rf "${BUNDLE}"
mkdir -p "${BUNDLE}/Contents/MacOS"
mkdir -p "${BUNDLE}/Contents/Resources"

# Copy binary
cp zelda3 "${BUNDLE}/Contents/MacOS/zelda3"

# Embed SDL2 framework so the app is self-contained
mkdir -p "${BUNDLE}/Contents/Frameworks"
cp -R SDL2.framework "${BUNDLE}/Contents/Frameworks/"

# Copy config if present
if [ -f "${INI}" ]; then
  cp "${INI}" "${BUNDLE}/Contents/Resources/"
fi

# Copy accessibility string files
for f in a11y_*.ini; do
  [ -f "$f" ] && cp "$f" "${BUNDLE}/Contents/Resources/"
done

# Copy accessibility settings if present
if [ -f "zelda3_a11y.ini" ]; then
  cp "zelda3_a11y.ini" "${BUNDLE}/Contents/Resources/"
fi

# Create Info.plist
cat > "${BUNDLE}/Contents/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>Zelda3</string>
  <key>CFBundleDisplayName</key>
  <string>The Legend of Zelda: A Link to the Past</string>
  <key>CFBundleIdentifier</key>
  <string>com.zelda3.app</string>
  <key>CFBundleVersion</key>
  <string>1.0</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleExecutable</key>
  <string>zelda3-launcher</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
PLIST

# Create launcher script that chdirs to Resources before running the binary.
# The game's SwitchDirectory() looks for zelda3.ini relative to cwd.
cat > "${BUNDLE}/Contents/MacOS/zelda3-launcher" << 'LAUNCHER'
#!/bin/bash
DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"
cd "${DIR}"
exec "$(dirname "$0")/zelda3" "$@"
LAUNCHER
chmod +x "${BUNDLE}/Contents/MacOS/zelda3-launcher"

echo "=== Done ==="
echo "Built ${BUNDLE} ($(du -sh "${BUNDLE}" | cut -f1))"
echo "Run with: open ${BUNDLE}"
echo "Or with accessibility: open ${BUNDLE} --args --accessibility"
