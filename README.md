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
# Debug build
./scripts/build.sh --debug

# Release build
./scripts/build.sh --release

# Clean debug
./scripts/build.sh --clean --debug
```

The APK will be at `build/debug/andFM.apk` or `build/release/andFM.apk`.

## Running

Install on an Android device or emulator (arm64-v8a):

```bash
adb install -r build/debug/andFM.apk
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
