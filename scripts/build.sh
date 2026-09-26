#!/bin/sh

set -eu

APP_NAME="mewFMAnd"
PACKAGE_NAME="org.linarcx.mewFMAnd"

API_LEVEL="30"

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD_ROOT="$ROOT_DIR/build"

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

AAPT="$BUILD_TOOLS_DIR/aapt"
ZIPALIGN="$BUILD_TOOLS_DIR/zipalign"
APKSIGNER="$BUILD_TOOLS_DIR/apksigner"

KEYTOOL="${JAVA_HOME:-}/bin/keytool"

if [ ! -x "$KEYTOOL" ]; then
  KEYTOOL="$(command -v keytool || true)"
fi

#
# Colors
#

if [ -t 1 ]; then
  ESC="$(printf '\033')"

  RESET="${ESC}[0m"
  BOLD="${ESC}[1m"
  DIM="${ESC}[2m"

  RED="${ESC}[31m"
  GREEN="${ESC}[32m"
  YELLOW="${ESC}[33m"
  BLUE="${ESC}[34m"
  MAGENTA="${ESC}[35m"
  CYAN="${ESC}[36m"
  WHITE="${ESC}[37m"
else
  RESET=""
  BOLD=""
  DIM=""

  RED=""
  GREEN=""
  YELLOW=""
  BLUE=""
  MAGENTA=""
  CYAN=""
  WHITE=""
fi


#
# Output helpers
#

die()
{
  printf "%s✗ Error:%s %s\n" "$RED" "$RESET" "$*" >&2
  exit 1
}

info()
{
  printf "  %s→%s %s\n" "$CYAN" "$RESET" "$*"
}

success()
{
  printf "  %s✓%s %s\n" "$GREEN" "$RESET" "$*"
}

warning()
{
  printf "  %s!%s %s\n" "$YELLOW" "$RESET" "$*"
}

section()
{
  printf "\n%s%s━━━ %s ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n\n" \
    "$BOLD" "$BLUE" "$*" "$RESET"
}

title()
{
  printf "\n"
  printf "%s%s╭──────────────────────────────────────╮%s\n" \
    "$BOLD" "$MAGENTA" "$RESET"
  printf "%s%s│             mewFMAnd Build              │%s\n" \
    "$BOLD" "$MAGENTA" "$RESET"
  printf "%s%s╰──────────────────────────────────────╯%s\n" \
    "$BOLD" "$MAGENTA" "$RESET"
  printf "\n"
}

#
# Parse arguments
#

MODE=""
CLEAN="false"
ABI=""

for ARG in "$@"; do
  case "$ARG" in
    --debug)
      if [ -n "$MODE" ]; then
        die "choose either --debug or --release"
      fi

      MODE="debug"
      ;;

    --release)
      if [ -n "$MODE" ]; then
        die "choose either --debug or --release"
      fi

      MODE="release"
      ;;

    --x86_64)
      if [ -n "$ABI" ]; then
        die "choose either --x86_64 or --arm64_v8a"
      fi

      ABI="x86_64"
      ;;

    --arm64_v8a)
      if [ -n "$ABI" ]; then
        die "choose either --x86_64 or --arm64_v8a"
      fi

      ABI="arm64_v8a"
      ;;

    --clean)
      CLEAN="true"
      ;;

    *)
      printf "%s\n" "Usage:"
      printf "  %s --debug --x86_64\n" "$0"
      printf "  %s --debug --arm64_v8a\n" "$0"
      printf "  %s --release --x86_64\n" "$0"
      printf "  %s --release --arm64_v8a\n" "$0"
      printf "  %s --clean --debug --x86_64\n" "$0"
      printf "  %s --clean --debug --arm64_v8a\n" "$0"
      printf "  %s --clean --release --x86_64\n" "$0"
      printf "  %s --clean --release --arm64_v8a\n" "$0"
      exit 1
      ;;
  esac
done

if [ -z "$MODE" ]; then
  die "build mode is required (--debug or --release)"
fi

if [ -z "$ABI" ]; then
  die "target ABI is required (--x86_64 or --arm64_v8a)"
fi

#
# Target ABI configuration
#

if [ "$ABI" = "x86_64" ]; then
  ANDROID_ABI="x86_64"
  TARGET="x86_64-linux-android$API_LEVEL"
else
  ANDROID_ABI="arm64-v8a"
  TARGET="aarch64-linux-android$API_LEVEL"
fi

#
# Mode-specific paths
#

BUILD_DIR="$BUILD_ROOT/$ABI/$MODE"
WORK_DIR="$BUILD_DIR/work"

if [ "$MODE" = "debug" ]; then
  KEYSTORE="$BUILD_DIR/debug.keystore"
  KEY_ALIAS="androiddebugkey"

  OPTIMIZATION_FLAGS="
    -O0
    -g
  "

  PREPROCESSOR_FLAGS=""

  LINKER_FLAGS="
    -Wl,--gc-sections
    -Wl,-z,max-page-size=16384
  "

  ANDROID_DEBUGGABLE="true"
else
  KEYSTORE="$BUILD_DIR/release.keystore"
  KEY_ALIAS="androidreleasekey"

  OPTIMIZATION_FLAGS="
    -O2
    -g0
  "

  PREPROCESSOR_FLAGS="
    -DNDEBUG
  "

  LINKER_FLAGS="
    -Wl,--gc-sections
    -Wl,--strip-debug
    -Wl,-z,max-page-size=16384
  "

  ANDROID_DEBUGGABLE="false"
fi

#
# Clean
#

if [ "$CLEAN" = "true" ]; then
  title

  printf "%sMode:%s %s\n" "$DIM" "$RESET" "$MODE"
  printf "%sPath:%s %s\n\n" "$DIM" "$RESET" "$BUILD_DIR"

  if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    success "Removed $BUILD_DIR"
  else
    info "$BUILD_DIR does not exist"
  fi

  printf "\n%sClean complete.%s\n\n" "$GREEN" "$RESET"
  exit 0
fi

title

printf "  %sMode:%s       %s%s%s\n" \
  "$DIM" "$RESET" "$BOLD" "$MODE" "$RESET"

printf "  %sOutput:%s     %s\n" \
  "$DIM" "$RESET" "$BUILD_DIR"

printf "  %sABI:%s        %s\n" \
  "$DIM" "$RESET" "$ANDROID_ABI"

printf "  %sAPI:%s        %s\n" \
  "$DIM" "$RESET" "$API_LEVEL"

printf "  %sNDK:%s        %s\n" \
  "$DIM" "$RESET" "$NDK_DIR"

#
# Validate tools
#

section "Checking environment"

[ -f "$SDK_PLATFORM" ] ||
  die "Android platform not found: $SDK_PLATFORM"
success "Android SDK"

[ -x "$CC" ] ||
  die "Clang not found: $CC"
success "Clang"

[ -x "$CXX" ] ||
  die "Clang++ not found: $CXX"
success "Clang++"

[ -x "$AAPT" ] ||
  die "aapt not found: $AAPT"
success "aapt"

[ -x "$ZIPALIGN" ] ||
  die "zipalign not found: $ZIPALIGN"
success "zipalign"

[ -x "$APKSIGNER" ] ||
  die "apksigner not found: $APKSIGNER"
success "apksigner"

[ -n "$KEYTOOL" ] ||
  die "keytool not found"
success "keytool"

#
# Validate source tree
#

section "Checking source tree"

[ -f "$SRC_DIR/main.cpp" ] ||
  die "Missing source: $SRC_DIR/main.cpp"
success "Application source"

[ -f "$THIRD_PARTY/impl.c" ] ||
  die "Missing implementation source: $THIRD_PARTY/impl.c"
success "Third-party implementation"

[ -f "$RAWDRAW_DIR/android_native_app_glue.c" ] ||
  die "Missing rawdrawandroid glue"
success "rawdrawandroid"

[ -f "$NANOVG_DIR/nanovg.h" ] ||
  die "Missing NanoVG"
success "NanoVG"

[ -f "$NANOVG_DIR/nanovg.c" ] ||
  die "Missing NanoVG implementation"
success "NanoVG implementation"

[ -f "$OUI_DIR/oui.h" ] ||
  die "Missing OUI"
success "OUI"

[ -f "$OUI_DIR/blendish.h" ] ||
  die "Missing Blendish"
success "Blendish"

#
# Prepare directories
#

rm -rf "$WORK_DIR"

mkdir -p \
  "$WORK_DIR/lib/$ANDROID_ABI" \
  "$WORK_DIR/assets"

FONT="$ROOT_DIR/assets/DejaVuSans.ttf"

[ -f "$FONT" ] ||
  die "Missing font: $FONT"

cp "$FONT" "$WORK_DIR/assets/DejaVuSans.ttf"

#
# Include paths
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
# Warnings
#

APP_WARNINGS="
  -Wall
  -Wextra
  -Wpedantic
"

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
# Common Android flags
#

ANDROID_CFLAGS="
  --target=$TARGET
  -fPIC
  -ffunction-sections
  -fdata-sections
"

#
# Compile
#

section "Compiling"

info "rawdrawandroid"

"$CC" \
  $ANDROID_CFLAGS \
  $OPTIMIZATION_FLAGS \
  $PREPROCESSOR_FLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -DAPPNAME='"mewFMAnd"' \
  -c \
  "$RAWDRAW_DIR/android_native_app_glue.c" \
  -o "$WORK_DIR/android_native_app_glue.o"

success "rawdrawandroid"

info "NanoVG"

"$CC" \
  $ANDROID_CFLAGS \
  $OPTIMIZATION_FLAGS \
  $PREPROCESSOR_FLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -c \
  "$NANOVG_DIR/nanovg.c" \
  -o "$WORK_DIR/nanovg.o"

success "NanoVG"

info "OUI / Blendish"

"$CC" \
  $ANDROID_CFLAGS \
  $OPTIMIZATION_FLAGS \
  $PREPROCESSOR_FLAGS \
  $THIRD_PARTY_WARNINGS \
  $INCLUDES \
  -c \
  "$THIRD_PARTY/impl.c" \
  -o "$WORK_DIR/impl.o"

success "OUI / Blendish"

info "Application"

"$CXX" \
  $ANDROID_CFLAGS \
  $OPTIMIZATION_FLAGS \
  $PREPROCESSOR_FLAGS \
  $APP_WARNINGS \
  $INCLUDES \
  -std=c++17 \
  -fno-exceptions \
  -fno-rtti \
  -c \
  "$SRC_DIR/main.cpp" \
  -o "$WORK_DIR/main.o"

success "Application"

#
# Link
#

section "Linking"

info "lib$APP_NAME.so"

"$CXX" \
  --target="$TARGET" \
  -shared \
  -static-libstdc++ \
  $LINKER_FLAGS \
  -Wl,-soname,lib"$APP_NAME".so \
  "$WORK_DIR/main.o" \
  "$WORK_DIR/android_native_app_glue.o" \
  "$WORK_DIR/nanovg.o" \
  "$WORK_DIR/impl.o" \
  -llog \
  -landroid \
  -lGLESv2 \
  -lEGL \
  -o "$WORK_DIR/lib/$ANDROID_ABI/lib$APP_NAME.so"

success "Native library created"

#
# Verify native library
#

section "Checking native library"

if readelf -d "$WORK_DIR/lib/$ANDROID_ABI/lib$APP_NAME.so" |
  grep -q 'libc++_shared.so'; then
  warning "libc++_shared.so is still required"
else
  success "C++ runtime statically linked"
fi

if nm -D "$WORK_DIR/lib/$ANDROID_ABI/lib$APP_NAME.so" |
  grep -q ' T nvgCreateInternal$'; then
  success "NanoVG implementation"
else
  die "nvgCreateInternal is missing from native library"
fi

#
# Generate manifest
#

section "Packaging"

info "AndroidManifest.xml"

cat > "$WORK_DIR/AndroidManifest.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<manifest
    xmlns:android="http://schemas.android.com/apk/res/android"
    package="$PACKAGE_NAME">

    <uses-sdk
        android:minSdkVersion="23"
        android:targetSdkVersion="$API_LEVEL" />

    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />

    <application
        android:allowBackup="false"
        android:debuggable="$ANDROID_DEBUGGABLE"
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

success "Manifest created"

#
# Package APK
#

UNSIGNED_APK="$WORK_DIR/$APP_NAME-unsigned.apk"
ALIGNED_APK="$WORK_DIR/$APP_NAME-aligned.apk"
FINAL_APK="$BUILD_DIR/$APP_NAME.apk"

info "Creating APK"

"$AAPT" package \
  -f \
  -M "$WORK_DIR/AndroidManifest.xml" \
  -I "$SDK_PLATFORM" \
  -A "$WORK_DIR/assets" \
  --target-sdk-version "$API_LEVEL" \
  -F "$UNSIGNED_APK"

success "Base APK"

info "Adding native library"

(
  cd "$WORK_DIR"

  zip -q \
    "$UNSIGNED_APK" \
    "lib/$ANDROID_ABI/lib$APP_NAME.so"
)

success "Native library added"

info "Aligning APK"

"$ZIPALIGN" \
  -f \
  -p \
  4 \
  "$UNSIGNED_APK" \
  "$ALIGNED_APK"

success "APK aligned"

#
# Signing key
#

if [ ! -f "$KEYSTORE" ]; then
  info "Creating $MODE signing key"

  if [ "$MODE" = "debug" ]; then
    KEY_NAME="Android Debug"
    ORGANIZATION="Android"
  else
    KEY_NAME="mewFMAnd Release"
    ORGANIZATION="mewFMAnd"
  fi

  "$KEYTOOL" \
    -genkeypair \
    -v \
    -keystore "$KEYSTORE" \
    -storepass android \
    -keypass android \
    -alias "$KEY_ALIAS" \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -dname "CN=$KEY_NAME,O=$ORGANIZATION,C=FR" \
    >/dev/null 2>&1

  success "Signing key created"
else
  success "Signing key already exists"
fi

#
# Sign
#

info "Signing APK"

"$APKSIGNER" sign \
  --ks "$KEYSTORE" \
  --ks-pass pass:android \
  --key-pass pass:android \
  --ks-key-alias "$KEY_ALIAS" \
  --out "$FINAL_APK" \
  "$ALIGNED_APK" \
  >/dev/null

success "APK signed"

#
# Verify
#

info "Verifying APK"

"$APKSIGNER" verify \
  --verbose \
  "$FINAL_APK" \
  >/dev/null

success "APK verified"

#
# Final output
#

printf "\n"

printf "%s%s╭──────────────────────────────────────╮%s\n" \
  "$BOLD" "$GREEN" "$RESET"

printf "%s%s│         Build successful ✓           │%s\n" \
  "$BOLD" "$GREEN" "$RESET"

printf "%s%s╰──────────────────────────────────────╯%s\n" \
  "$BOLD" "$GREEN" "$RESET"

printf "\n"

printf "  %sMode:%s        %s\n" \
  "$DIM" "$RESET" "$MODE"

printf "  %sAPK:%s         %s\n" \
  "$DIM" "$RESET" "$FINAL_APK"

printf "  %sNative:%s      %s\n" \
  "$DIM" "$RESET" "$WORK_DIR/lib/$ANDROID_ABI/lib$APP_NAME.so"

printf "  %sPackage:%s     %s\n" \
  "$DIM" "$RESET" "$PACKAGE_NAME"

printf "\n"

printf "%sInstall:%s\n" "$BOLD" "$RESET"
printf "  adb -s 127.0.0.1:5555 install -r \"%s\"\n" "$FINAL_APK"

printf "\n"

printf "%sRun:%s\n" "$BOLD" "$RESET"
printf "  adb -s 127.0.0.1:5555 shell am start -n %s/android.app.NativeActivity\n" \
  "$PACKAGE_NAME"

printf "\n"

printf "%sLog:%s\n" "$BOLD" "$RESET"
printf "  adb -s 127.0.0.1:5555 logcat | grep -E 'mewFMAnd|AndroidRuntime|DEBUG|FATAL'\n"

printf "\n"
