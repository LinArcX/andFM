#!/bin/sh

set -eu

APP_NAME="andFM"
PACKAGE_NAME="org.linarcx.andFM"

API_LEVEL="30"
ABI="x86_64"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
WORK_DIR="$BUILD_DIR/work"

SRC_DIR="$ROOT_DIR/src"
THIRD_PARTY="$ROOT_DIR/third_party"

RAWDRAW_DIR="$THIRD_PARTY/rawdrawandroid"
NANOVG_DIR="$THIRD_PARTY/nanovg"
OUI_DIR="$THIRD_PARTY/oui-blendish"

ANDROID_HOME="${ANDROID_HOME:-$HOME/android-sdk}"

SDK_PLATFORM="$ANDROID_HOME/platforms/android-$API_LEVEL/android.jar"
BUILD_TOOLS_DIR="$ANDROID_HOME/build-tools/35.0.0"
NDK_DIR="$ANDROID_HOME/ndk/27.2.12479018"

HOST_TAG="linux-x86_64"

CC="$NDK_DIR/toolchains/llvm/prebuilt/$HOST_TAG/bin/clang"
CXX="$NDK_DIR/toolchains/llvm/prebuilt/$HOST_TAG/bin/clang++"

TARGET="$ABI-linux-android$API_LEVEL"

AAPT="$BUILD_TOOLS_DIR/aapt"
ZIPALIGN="$BUILD_TOOLS_DIR/zipalign"
APKSIGNER="$BUILD_TOOLS_DIR/apksigner"

KEYTOOL="${JAVA_HOME:-}/bin/keytool"

if [ ! -x "$KEYTOOL" ]; then
  KEYTOOL="$(command -v keytool || true)"
fi

KEYSTORE="$BUILD_DIR/debug.keystore"

die()
{
  echo "Error: $*" >&2
  exit 1
}

echo "== andFM build =="
echo "Root:       $ROOT_DIR"
echo "ABI:        $ABI"
echo "API:        $API_LEVEL"
echo "NDK:        $NDK_DIR"
echo

#
# Validate tools
#

[ -f "$SDK_PLATFORM" ] ||
  die "Android platform not found: $SDK_PLATFORM"

[ -x "$CC" ] ||
  die "Clang not found: $CC"

[ -x "$CXX" ] ||
  die "Clang++ not found: $CXX"

[ -x "$AAPT" ] ||
  die "aapt not found: $AAPT"

[ -x "$ZIPALIGN" ] ||
  die "zipalign not found: $ZIPALIGN"

[ -x "$APKSIGNER" ] ||
  die "apksigner not found: $APKSIGNER"

[ -n "$KEYTOOL" ] ||
  die "keytool not found"

#
# Validate source tree
#

[ -f "$SRC_DIR/main.cpp" ] ||
  die "Missing source: $SRC_DIR/main.cpp"

[ -f "$THIRD_PARTY/impl.c" ] ||
  die "Missing implementation source: $THIRD_PARTY/impl.c"

[ -f "$RAWDRAW_DIR/android_native_app_glue.c" ] ||
  die "Missing rawdrawandroid glue"

[ -f "$RAWDRAW_DIR/android_native_app_glue.h" ] ||
  die "Missing rawdrawandroid glue header"

[ -f "$NANOVG_DIR/nanovg.h" ] ||
  die "Missing NanoVG"

[ -f "$NANOVG_DIR/nanovg_gl.h" ] ||
  die "Missing NanoVG GL backend"

[ -f "$OUI_DIR/oui.h" ] ||
  die "Missing OUI"

[ -f "$OUI_DIR/blendish.h" ] ||
  die "Missing Blendish"

#
# Clean build
#

rm -rf "$BUILD_DIR"

mkdir -p \
  "$WORK_DIR/lib/$ABI" \
  "$WORK_DIR/assets"

#
# Copy application font
#

FONT="$ROOT_DIR/assets/DejaVuSans.ttf"

[ -f "$FONT" ] ||
  die "Missing font: $FONT"

cp "$FONT" "$WORK_DIR/assets/DejaVuSans.ttf"

#
# Common include paths
#

INCLUDES="
  -I$ROOT_DIR
  -I$SRC_DIR
  -I$THIRD_PARTY
  -I$RAWDRAW_DIR
  -I$NANOVG_DIR
  -I$OUI_DIR
"

#
# Warnings for our C++ application.
#

APP_WARNINGS="
  -Wall
  -Wextra
  -Wpedantic
"

#
# Warnings for third-party C sources.
#
# rawdrawandroid contains intentionally old C constructs and
# unused callback parameters. Keep those warnings suppressed
# so the build output remains useful.
#

THIRD_PARTY_WARNINGS="
  -Wall
  -Wextra
  -Wno-strict-prototypes
  -Wno-unused-parameter
  -Wno-format-pedantic
  -Wno-gnu-anonymous-struct
  -Wno-nested-anon-types
"

#
# Common Android compiler flags.
#

ANDROID_CFLAGS="
  --target=$TARGET
  -fPIC
  -ffunction-sections
  -fdata-sections
"

#
# Compile rawdrawandroid glue
#

echo "Compiling rawdrawandroid glue..."

"$CC" \
  $ANDROID_CFLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -DAPPNAME='"andFM"' \
  -c \
  "$RAWDRAW_DIR/android_native_app_glue.c" \
  -o "$WORK_DIR/android_native_app_glue.o"

#
# Compile NanoVG / OUI / Blendish implementations.
#
# impl.c contains:
#
#   NANOVG_GL2_IMPLEMENTATION
#   OUI_IMPLEMENTATION
#   BLENDISH_IMPLEMENTATION
#
# Do not define these implementation macros in main.cpp.
#

echo "Compiling NanoVG/OUI implementations..."

"$CC" \
  $ANDROID_CFLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -c \
  "$ROOT_DIR/third_party/nanovg/nanovg.c" \
  -o "$WORK_DIR/nanovg.o"

"$CC" \
  $ANDROID_CFLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -c \
  "$ROOT_DIR/third_party/impl.c" \
  -o "$WORK_DIR/impl.o"

#echo "Compiling NanoVG/OUI implementations..."
#
#"$CC" \
#  $ANDROID_CFLAGS \
#  $THIRD_PARTY_WARNINGS \
#  $INCLUDES \
#  -c \
#  "$THIRD_PARTY/impl.c" \
#  -o "$WORK_DIR/impl.o"

#
# Compile application
#

echo "Compiling application..."

"$CXX" \
  $ANDROID_CFLAGS \
  $APP_WARNINGS \
  $INCLUDES \
  -std=c++17 \
  -fno-exceptions \
  -fno-rtti \
  -c \
  "$SRC_DIR/main.cpp" \
  -o "$WORK_DIR/main.o"

#
# Link native library
#

echo "Linking..."

"$CXX" \
  --target="$TARGET" \
  -shared \
  -static-libstdc++ \
  -Wl,--gc-sections \
  -Wl,-soname,lib"$APP_NAME".so \
  "$WORK_DIR/main.o" \
  "$WORK_DIR/android_native_app_glue.o" \
  "$WORK_DIR/nanovg.o" \
  "$WORK_DIR/impl.o" \
  -llog \
  -landroid \
  -lGLESv2 \
  -lEGL \
  -o "$WORK_DIR/lib/$ABI/lib$APP_NAME.so"

#
# Verify native library
#

echo "Checking native library..."

echo
echo "Dynamic dependencies:"
readelf -d "$WORK_DIR/lib/$ABI/lib$APP_NAME.so" |
  grep NEEDED || true

echo

if readelf -d "$WORK_DIR/lib/$ABI/lib$APP_NAME.so" |
  grep -q 'libc++_shared.so'; then
  echo "Warning: libc++_shared.so is still required."
else
  echo "C++ runtime: statically linked"
fi

#
# Verify NanoVG implementation.
#

if nm -D "$WORK_DIR/lib/$ABI/lib$APP_NAME.so" |
  grep -q ' T nvgCreateInternal$'; then
  echo "NanoVG implementation: OK"
else
  echo
  echo "ERROR: nvgCreateInternal is missing from native library."
  echo
  nm -D "$WORK_DIR/lib/$ABI/lib$APP_NAME.so" |
    grep nvgCreateInternal || true
  exit 1
fi

#
# Generate AndroidManifest.xml
#

echo
echo "Creating manifest..."

cat > "$WORK_DIR/AndroidManifest.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<manifest
    xmlns:android="http://schemas.android.com/apk/res/android"
    package="$PACKAGE_NAME">

    <uses-sdk
        android:minSdkVersion="23"
        android:targetSdkVersion="$API_LEVEL" />

    <application
        android:allowBackup="false"
        android:debuggable="false"
        android:hasCode="false"
        android:label="$APP_NAME"
        android:supportsRtl="false">

        <activity
            android:name="android.app.NativeActivity"
            android:exported="true"
            android:configChanges="keyboardHidden|orientation|screenSize">

            <meta-data
                android:name="android.app.lib_name"
                android:value="$APP_NAME" />

            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>

        </activity>

    </application>

</manifest>
EOF

#
# Package base APK
#

UNSIGNED_APK="$WORK_DIR/$APP_NAME-unsigned.apk"
ALIGNED_APK="$WORK_DIR/$APP_NAME-aligned.apk"
FINAL_APK="$BUILD_DIR/$APP_NAME.apk"

echo "Packaging APK..."

"$AAPT" package \
  -f \
  -M "$WORK_DIR/AndroidManifest.xml" \
  -I "$SDK_PLATFORM" \
  -A "$WORK_DIR/assets" \
  --target-sdk-version "$API_LEVEL" \
  -F "$UNSIGNED_APK"

#
# Add native library.
#

echo "Adding native library..."

(
  cd "$WORK_DIR"

  zip -q \
    "$UNSIGNED_APK" \
    "lib/$ABI/lib$APP_NAME.so"
)

#
# Align APK.
#

echo "Aligning APK..."

"$ZIPALIGN" \
  -f \
  -p \
  4 \
  "$UNSIGNED_APK" \
  "$ALIGNED_APK"

#
# Create local debug signing key.
#

if [ ! -f "$KEYSTORE" ]; then
  echo "Creating local signing key..."

  "$KEYTOOL" \
    -genkeypair \
    -v \
    -keystore "$KEYSTORE" \
    -storepass android \
    -keypass android \
    -alias androiddebugkey \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"
fi

#
# Sign APK.
#

echo "Signing APK..."

"$APKSIGNER" sign \
  --ks "$KEYSTORE" \
  --ks-pass pass:android \
  --key-pass pass:android \
  --ks-key-alias androiddebugkey \
  --out "$FINAL_APK" \
  "$ALIGNED_APK"

#
# Verify APK.
#

echo "Verifying APK..."

"$APKSIGNER" verify \
  --verbose \
  "$FINAL_APK"

#
# Show final APK contents.
#

echo
echo "APK contents:"
unzip -l "$FINAL_APK" |
  grep -E 'AndroidManifest|lib/|DejaVu'

#
# Final information
#

echo
echo "========================================"
echo "Build successful"
echo "========================================"
echo
echo "APK:"
echo "  $FINAL_APK"
echo
echo "ABI:"
echo "  $ABI"
echo
echo "Package:"
echo "  $PACKAGE_NAME"
echo
echo "Native library:"
echo "  $WORK_DIR/lib/$ABI/lib$APP_NAME.so"
echo
echo "Install:"
echo "  adb -s 127.0.0.1:5555 install -r \"$FINAL_APK\""
echo
echo "Run:"
echo "  adb -s 127.0.0.1:5555 shell am start -n $PACKAGE_NAME/android.app.NativeActivity"
echo
echo "Log:"
echo "  adb -s 127.0.0.1:5555 logcat | grep -E 'andFM|AndroidRuntime|DEBUG|FATAL'"
echo
