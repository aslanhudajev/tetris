#!/usr/bin/env bash
# Builds Metris.app as a self-contained universal binary and zips it.
#
# The result runs on both Intel and Apple Silicon Macs with nothing installed:
# raylib is compiled from source and linked statically, and the assets live
# inside the bundle.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build-bundle"
DIST_DIR="$ROOT/dist"
APP="$BUILD_DIR/Metris.app"

echo "==> Configuring universal build"
# Drop the cache first. raylib's SUPPORT_* switches are cached options, so a
# stale cache would keep applying settings we have since removed, silently
# rebuilding the engine with the wrong behaviour.
rm -f "$BUILD_DIR/CMakeCache.txt"

cmake -B "$BUILD_DIR" -S "$ROOT" \
    -DMETRIS_BUNDLE=ON \
    -DCMAKE_BUILD_TYPE=Release

# SUPPORT_CUSTOM_FRAME_CONTROL strips SwapScreenBuffer() and PollInputEvents()
# out of EndDrawing(), which produces an app that opens a window, never paints
# it and ignores input. It only turns on when raylib generates its own config,
# and it is invisible until someone launches the result, so assert it here.
if grep -qE '^(CUSTOMIZE_BUILD|SUPPORT_CUSTOM_FRAME_CONTROL):BOOL=ON' "$BUILD_DIR/CMakeCache.txt"; then
    echo "!! raylib is configured with custom frame control; the app would never draw" >&2
    exit 1
fi

echo "==> Building (raylib is compiled from source, this takes a few minutes)"
cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"

BINARY="$APP/Contents/MacOS/Metris"

if [ ! -f "$BINARY" ]; then
    echo "!! Expected $BINARY but it is missing" >&2
    exit 1
fi

echo "==> Verifying architectures"
ARCHS="$(lipo -archs "$BINARY")"
echo "    $ARCHS"

for want in x86_64 arm64; do
    case " $ARCHS " in
        *" $want "*) ;;
        *) echo "!! $want slice missing, this will not run everywhere" >&2; exit 1 ;;
    esac
done

echo "==> Checking for external dependencies"
# Anything outside /System or /usr/lib would have to exist on her Mac too.
# Only the indented lines are dependencies; otool prints a header per slice.
FOREIGN="$(otool -L "$BINARY" \
    | grep -E '^[[:space:]]+/' \
    | awk '{print $1}' \
    | sort -u \
    | grep -vE '^/(System/|usr/lib/)' || true)"

if [ -n "$FOREIGN" ]; then
    echo "!! Links libraries that will not exist on another Mac:" >&2
    echo "$FOREIGN" >&2
    exit 1
fi
echo "    only system frameworks, good"

echo "==> Signing"
# Ad-hoc: enough for the app to launch once Gatekeeper is satisfied, and
# required on Apple Silicon, which refuses unsigned binaries outright.
codesign --force --deep --sign - --timestamp=none "$APP"
codesign --verify --deep --strict "$APP"

echo "==> Packaging"
mkdir -p "$DIST_DIR"
rm -f "$DIST_DIR/Metris.zip"
# ditto rather than zip: it preserves the bundle structure and metadata.
ditto -c -k --keepParent "$APP" "$DIST_DIR/Metris.zip"

echo
echo "Done: $DIST_DIR/Metris.zip ($(du -h "$DIST_DIR/Metris.zip" | cut -f1))"
echo
echo "The app is signed ad-hoc, not notarised, so macOS will quarantine it"
echo "after transfer. On her Mac, either:"
echo "  - right-click Metris.app > Open, then confirm, or"
echo "  - run: xattr -dr com.apple.quarantine /path/to/Metris.app"
