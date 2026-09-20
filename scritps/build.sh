#!/usr/bin/env bash

set -euo pipefail

APP_NAME="andFM"
PACKAGE_NAME="org.linarcx.andFM"

ANDROID_API="${ANDROID_API:-30}"
ABI="${ABI:-x86_64}"

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"

SRC_DIR="$ROOT/src"
ASSET_DIR="$ROOT/assets"
TP_DIR="$ROOT/third_party"
BUILD_DIR="$ROOT/build"

RAW_DIR="$TP_DIR/rawdrawandroid"
OUI_DIR="$TP_DIR/oui-blendish"
NVG_DIR="$TP_DIR/nanovg"

APK="$BUILD_DIR/${APP_NAME}.apk"
UNSIGNED_APK="$BUILD_DIR/${APP_NAME}-unsigned.apk"
MANIFEST="$BUILD_DIR/AndroidManifest.xml"

mkdir -p "$TP_DIR" "$BUILD_DIR" "$ASSET_DIR"

die()
{
  echo "error: $*" >&2
  exit 1
}

find_sdk()
{
  if [ -n "${ANDROID_HOME:-}" ] && [ -d "$ANDROID_HOME" ]; then
    printf '%s\n' "$ANDROID_HOME"
    return
  fi

  if [ -n "${ANDROID_SDK_ROOT:-}" ] && [ -d "$ANDROID_SDK_ROOT" ]; then
    printf '%s\n' "$ANDROID_SDK_ROOT"
    return
  fi

  if [ -d "$HOME/Android/Sdk" ]; then
    printf '%s\n' "$HOME/Android/Sdk"
    return
  fi

  die "Android SDK not found. Set ANDROID_HOME."
}

SDK="$(find_sdk)"

NDK="${ANDROID_NDK:-${ANDROID_NDK_HOME:-}}"

if [ -z "$NDK" ]; then
  for candidate in "$SDK"/ndk/*; do
    if [ -d "$candidate" ]; then
      NDK="$candidate"
    fi
  done
fi

[ -n "$NDK" ] || die "Android NDK not found."

BUILD_TOOLS=""

for candidate in "$SDK"/build-tools/*; do
  if [ -d "$candidate" ]; then
    BUILD_TOOLS="$candidate"
  fi
done

[ -n "$BUILD_TOOLS" ] || die "Android build-tools not found."

PLATFORM="$SDK/platforms/android-$ANDROID_API"

[ -d "$PLATFORM" ] ||
  die "Android platform android-$ANDROID_API not found."

AAPT="$BUILD_TOOLS/aapt"
ZIPALIGN="$BUILD_TOOLS/zipalign"
APKSIGNER="$BUILD_TOOLS/apksigner"

[ -x "$AAPT" ] || die "aapt not found."
[ -x "$ZIPALIGN" ] || die "zipalign not found."
[ -x "$APKSIGNER" ] || die "apksigner not found."

HOST_TAG="linux-x86_64"

case "$(uname -s)" in
  Darwin)
    HOST_TAG="darwin-x86_64"
    ;;
esac

TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/$HOST_TAG/bin"

CXX="$TOOLCHAIN/x86_64-linux-android${ANDROID_API}-clang++"
CC="$TOOLCHAIN/x86_64-linux-android${ANDROID_API}-clang"

[ -x "$CXX" ] || die "Android C++ compiler not found: $CXX"
[ -x "$CC" ] || die "Android C compiler not found: $CC"

if [ ! -f "$ASSET_DIR/DejaVuSans.ttf" ]; then
  echo "Downloading DejaVu Sans..."

  curl -L \
    --fail \
    --silent \
    --show-error \
    "https://raw.githubusercontent.com/geetrepo/oui-blendish/master/DejaVuSans.ttf" \
    -o "$ASSET_DIR/DejaVuSans.ttf"
fi

rm -rf "$BUILD_DIR/work"

mkdir -p \
  "$BUILD_DIR/work/lib/x86_64" \
  "$BUILD_DIR/work/assets"

cp \
  "$ASSET_DIR/DejaVuSans.ttf" \
  "$BUILD_DIR/work/assets/DejaVuSans.ttf"

echo "Compiling rawdrawandroid glue..."

"$CC" \
  -std=c11 \
  -DANDROID \
  -DAPPNAME=\"andFM\" \
  -I"$RAW_DIR" \
  -I"$NDK/sysroot/usr/include" \
  -fPIC \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -Os \
  -c \
  "$RAW_DIR/android_native_app_glue.c" \
  -o "$BUILD_DIR/work/android_native_app_glue.o"

echo "Compiling application..."

"$CXX" \
  -std=c++17 \
  -DANDROID \
  -DAPPNAME=\"andFM\" \
  -DNANOVG_GLES2_IMPLEMENTATION \
  -I"$RAW_DIR" \
  -I"$OUI_DIR" \
  -I"$NVG_DIR/src" \
  -I"$NDK/sysroot/usr/include" \
  -fPIC \
  -ffunction-sections \
  -fdata-sections \
  -fvisibility=hidden \
  -fno-exceptions \
  -fno-rtti \
  -Os \
  -Wall \
  -Wextra \
  -Wpedantic \
  -c \
  "$SRC_DIR/main.cpp" \
  -o "$BUILD_DIR/work/main.o"

echo "Linking..."

"$CXX" \
  "$BUILD_DIR/work/main.o" \
  "$BUILD_DIR/work/android_native_app_glue.o" \
  -shared \
  -static-libstdc++ \
  -Wl,--gc-sections \
  -Wl,-z,relro \
  -Wl,-z,now \
  -Wl,-z,noexecstack \
  -Wl,-s \
  -Wl,-u,ANativeActivity_onCreate \
  -o "$BUILD_DIR/work/lib/x86_64/libandFM.so" \
  -landroid \
  -llog \
  -lEGL \
  -lGLESv2 \
  -lm

echo "Creating manifest..."

cat > "$MANIFEST" <<EOF
<?xml version="1.0" encoding="utf-8"?>

<manifest
    xmlns:android="http://schemas.android.com/apk/res/android"
    package="$PACKAGE_NAME">

    <uses-sdk
        android:minSdkVersion="23"
        android:targetSdkVersion="$ANDROID_API" />

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
                android:value="andFM" />

            <intent-filter>
                <action android:name="android.intent.action.MAIN" />

                <category
                    android:name="android.intent.category.LAUNCHER" />
            </intent-filter>

        </activity>

    </application>

</manifest>
EOF

echo "Packaging APK..."

rm -f "$UNSIGNED_APK"
rm -f "$APK"

"$AAPT" package \
  -f \
  -M "$MANIFEST" \
  -I "$PLATFORM/android.jar" \
  -A "$BUILD_DIR/work/assets" \
  -F "$UNSIGNED_APK" \
  --target-sdk-version "$ANDROID_API"

echo "Adding native library..."

(
  cd "$BUILD_DIR/work"

  zip \
    -q \
    "$UNSIGNED_APK" \
    "lib/x86_64/libandFM.so"
)

echo "Aligning APK..."

"$ZIPALIGN" \
  -f \
  4 \
  "$UNSIGNED_APK" \
  "$APK"

KEYSTORE="$BUILD_DIR/debug.keystore"

if [ ! -f "$KEYSTORE" ]; then
  echo "Creating local signing key..."

  keytool \
    -genkeypair \
    -keystore "$KEYSTORE" \
    -storepass android \
    -keypass android \
    -alias androiddebugkey \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"
fi

echo "Signing APK..."

"$APKSIGNER" sign \
  --ks "$KEYSTORE" \
  --ks-pass pass:android \
  --key-pass pass:android \
  --ks-key-alias androiddebugkey \
  "$APK"

rm -f "$UNSIGNED_APK"

echo
echo "========================================"
echo "Build successful"
echo "========================================"
echo
echo "APK:"
echo "  $APK"
echo
echo "ABI:"
echo "  $ABI"
echo
echo "Package:"
echo "  $PACKAGE_NAME"
echo
echo "Install:"
echo "  adb -s 127.0.0.1:5555 install -r \"$APK\""
echo
echo "Run:"
echo "  adb -s 127.0.0.1:5555 shell am start -n $PACKAGE_NAME/android.app.NativeActivity"
echo
echo "Log:"
echo "  adb -s 127.0.0.1:5555 logcat | grep andFM"
echo
