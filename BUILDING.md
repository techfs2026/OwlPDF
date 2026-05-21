# MuQt 开发环境搭建指南

## 平台与依赖管理方式

| 平台 | 依赖管理 | 说明 |
|------|---------|------|
| **macOS** | Homebrew | 唯一支持方式，稳定可靠 |
| **Windows** | 手动路径（默认） | 适合已有本地库的开发者 |
| **Windows** | vcpkg（可选） | 适合新成员，自动下载编译 |

> macOS 不支持 vcpkg 管理依赖，原因是 mupdf 等库的源码包从 GitHub
> 下载在国内网络环境下不稳定，且 Homebrew 已提供现成二进制，无需重复编译。

---

## macOS

### 1. 安装 Qt

从 https://www.qt.io/download-qt-installer 下载安装，然后设置环境变量：

```bash
# 加入 ~/.zshrc，路径精确到 lib/cmake/Qt6 这一层
echo 'export Qt6_DIR=/Users/yourname/Qt/6.11.1/macos/lib/cmake/Qt6' >> ~/.zshrc
source ~/.zshrc
```

### 2. 安装依赖库

```bash
brew install mupdf opencv brotli jbig2dec openjpeg gumbo-parser
```

打包时还需要：
```bash
brew install dylibbundler
```

### 3. 构建

```bash
git clone https://github.com/yourorg/MuQt.git
cd MuQt
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

### 4. 打包（生成可分发的 .app）

```bash
cmake --build build --target deploy
# 产物：build/MuQt.app（已内嵌所有依赖，用户无需安装任何库）
```

可进一步打成 .dmg：
```bash
brew install create-dmg
create-dmg \
  --volname "MuQt" \
  --window-size 500 300 \
  --app-drop-link 350 150 \
  "MuQt.dmg" \
  "build/MuQt.app"
```

---

## Windows（手动路径，默认）

适合已有本地库的开发者。

### 1. 安装 Qt

从 https://www.qt.io/download-qt-installer 下载安装，然后设置环境变量（PowerShell）：

```powershell
[System.Environment]::SetEnvironmentVariable("Qt6_DIR", "D:\Qt6\6.8.3\msvc2022_64\lib\cmake\Qt6", "User")
```

### 2. 下载依赖库并设置环境变量

手动下载并解压各依赖库：
- MuPDF: https://mupdf.com/downloads/
- OpenCV: https://opencv.org/releases/
- ONNX Runtime: https://github.com/microsoft/onnxruntime/releases

设置环境变量（永久生效）：
```powershell
[System.Environment]::SetEnvironmentVariable("MUPDF_DIR",       "D:\Ext-Lib\mupdf-win-x64",       "User")
[System.Environment]::SetEnvironmentVariable("OPENCV_DIR",      "D:\Ext-Lib\opencv-win-x64",      "User")
[System.Environment]::SetEnvironmentVariable("ONNXRUNTIME_DIR", "D:\Ext-Lib\onnxruntime-win-x64", "User")
```

### 3. 构建

```powershell
git clone https://github.com/yourorg/MuQt.git
cd MuQt
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

## Windows（vcpkg，推荐新成员）

无需手动下载任何第三方库，vcpkg 自动处理。

### 1. 安装 Qt 并设置 Qt6_DIR（同上）

### 2. 构建

```powershell
git clone https://github.com/yourorg/MuQt.git
cd MuQt
cmake -B build -DBUILD_WITH_VCPKG=ON -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

首次运行时 vcpkg 会自动 clone 并编译所有依赖（约 20-40 分钟），后续构建使用缓存，速度正常。

也可以提前设置 `VCPKG_ROOT` 指向团队共享的 vcpkg 安装，避免重复编译：
```powershell
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", "D:\vcpkg", "User")
```

---

## Qt Creator 配置

Qt Creator 不读取系统环境变量，需要在项目配置里手动指定 Qt 路径。

打开项目后，进入 **Projects → Build → CMake → Initial CMake parameters**，添加：

```
-DQt6_DIR=/Users/yourname/Qt/6.11.1/macos/lib/cmake/Qt6
```

Windows 示例：
```
-DQt6_DIR=D:\Qt6\6.8.3\msvc2022_64\lib\cmake\Qt6
```

注意路径需精确到 `lib/cmake/Qt6` 这一层。

---

## 常见问题

**Q: cmake 报错 `Could not find Qt6`**
A: 检查 `Qt6_DIR` 是否精确到 `lib/cmake/Qt6` 这一层，而不是 Qt 安装根目录。

**Q: macOS 报错 `mupdf/fitz.h not found`**
A: 运行 `brew install mupdf`。

**Q: macOS 可以用 `-DBUILD_WITH_VCPKG=ON` 吗？**
A: 不可以，macOS 只支持 Homebrew 方式。`BUILD_WITH_VCPKG=ON` 仅用于 Windows。

**Q: Windows vcpkg 编译失败**
A: 确保已安装 Visual Studio Build Tools（含 C++ 工作负载）。