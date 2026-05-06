# SDL3 Hello Triangle - Multi-Platform

Cross-platform triangle rendering with software rasterization.

**Tested:** Linux✅, Windows✅, macOS(M1+)✅, iOS

## Linux

### Setup (run once)
```bash
chmod +x setup_linux.sh
./setup_linux.sh
```

### Build (every time you modify code)
```bash
cmake --build build
cmake --build build --target 00_init_SDL3
cmake --build build --target 01_window
cmake --build build --target 02_vk_instance
cmake --build build --target 03_minimal_triangle
```

### Run
```bash
./build/Part_00_init_SDL3/00_init_SDL3
./build/Part_01_window/01_window
./build/Part_02_vk_instance/02_vk_instance
./build/Part_03_minimal_triangle/03_minimal_triangle
```

### Clean
```bash
rm -rf build
rm -rf third_party
```

---
## Windows

### Setup (run once)
Download MSVC cpp build tools if you haven't [MSVC Compiler](https://visualstudio.microsoft.com/visual-cpp-build-tools/)

In the Windows search bar, look for "x64 Native Tools Command Prompt for VS 2022"
Verify compiler exists:
```bash
cl
```
Run once for initialization
```bash
powershell -ExecutionPolicy Bypass -File setup.ps1
```
### Build (every time you modify code)
```bash
cmake --build build --config Debug
cmake --build build --target 00_init_SDL3
cmake --build build --target 01_window
cmake --build build --target 02_vk_instance
cmake --build build --target 03_minimal_triangle
```
### Copy .dll to .exe folder
```bash
powershell -ExecutionPolicy Bypass -File copy-dlls.ps1
```
### Run
```bash
build\Part_00_init_SDL3\Debug\00_init_SDL3.exe
build\Part_01_window\Debug\01_window.exe
build\Part_02_vk_instance\Debug\02_vk_instance.exe
build\Part_03_minimal_triangle\Debug\03_minimal_triangle.exe
```
### Clean
```bash
rmdir /s /q build
rmdir /s /q third_party
```
---
## macOS(M1+)

### Setup (run once)
```bash
chmod +x setup_macos.sh
./setup_macos.sh
```

### Build (every time you modify code)
```bash
cmake --build build
cmake --build build --target 00_init_SDL3
cmake --build build --target 01_window
cmake --build build --target 02_vk_instance
```

### Run
```bash
./build/Part_00_init_SDL3/00_init_SDL3
./build/Part_01_window/01_window
./build/Part_02_vk_instance/02_vk_instance
```

### Clean
```bash
rm -rf build
rm -rf third_party
```
---
## iOS

### Setup (run once)
```bash
chmod +x setup_ios.sh
./setup_ios.sh
```

### Build (every time you modify code)
```bash
rm -rf build
./setup_ios.sh
open build/SDL3HelloTriangle.xcodeproj
```
---