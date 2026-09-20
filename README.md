# andFM

A minimal Android native application template using C++17, [rawdrawandroid](https://github.com/cntools/rawdraw), [NanoVG](https://github.com/memononen/nanovg), and [OUI/Blendish](https://github.com/ouiliame/oui-blendish). It demonstrates a simple UI with a button and text, built as a native activity.

## Features

- Native Android activity (no Java/Kotlin code)
- C++17 with minimal runtime
- Immediate-mode UI with NanoVG and OUI/Blendish
- Build system using shell script and Android NDK
- APK signing and alignment
- Debug and release build variants
- Extensive helper menu (`./p`) for building, debugging, profiling, static analysis, etc.

## Prerequisites

- Android SDK (with platform android-30, build-tools 35.0.0)
- Android NDK (27.2.12479018)
- Clang (from NDK)
- Java (for keytool and apksigner)
- Bash, zip, zipalign, etc.

Set `ANDROID_HOME` to your SDK path. Default is `$HOME/android-sdk`.

## Building

```bash
# Debug build (x86_64)
./scripts/build.sh --debug --x86_64

# Debug build (arm64-v8a)
./scripts/build.sh --debug --arm64_v8a

# Release build (x86_64)
./scripts/build.sh --release --x86_64

# Release build (arm64-v8a)
./scripts/build.sh --release --arm64_v8a

# Clean debug (x86_64)
./scripts/build.sh --clean --debug --x86_64

# Clean debug (arm64-v8a)
./scripts/build.sh --clean --debug --arm64_v8a

# Clean release (x86_64)
./scripts/build.sh --clean --release --x86_64

# Clean release (arm64-v8a)
./scripts/build.sh --clean --release --arm64_v8a
```

The APK will be at `build/<abi>/<mode>/andFM.apk`, for example `build/x86_64/debug/andFM.apk` or `build/arm64_v8a/release/andFM.apk`.

## Running

Install on an Android device or emulator:

```bash
adb install -r build/arm64_v8a/debug/andFM.apk
adb shell am start -n org.linarcx.andFM/android.app.NativeActivity
```

## Project Structure

- `src/main.cpp` – Entry point and UI code
- `third_party/` – External libraries (rawdrawandroid, nanovg, oui-blendish)
- `scripts/build.sh` – Build script
- `p` – Interactive menu for various tasks
- `assets/` – Fonts and other assets

## License

This project is provided as-is. Third-party libraries have their own licenses.
