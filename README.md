# MuQt PDF Reader

MuQt Reader 是一款基于 Qt + MuPDF 构建的现代化 PDF 阅读器。

目标不是"功能最多"，不追求成为全功能 PDF 编辑器，而是让你在通过 PDF 高效获取信息的过程中，获得稳定、流畅、专注的阅读体验。

受到 Apple 平台 PDF Expert 的设计理念启发，MuQt PDF Reader 基于开源技术，专注于 Windows 与 macOS 平台。

## 界面展示

<img src="docs/images/mainwindow1.png" alt="主窗口-缩略图-连续滚动"/>

<img src="docs/images/mainwindow2.png" alt="主窗口-大纲-连续滚动"/>

## 主要特性

### 阅读
- **多种显示模式**：单页、双页、连续滚动
- **灵活缩放**：适应页面、适应宽度、自定义缩放（25% - 400%）
- **多标签页**：同时打开多个 PDF 文档
- **导航面板**：大纲、缩略图预览

### 交互
- **全文搜索**：支持大小写敏感、全字匹配
- **文本选择**：字符级、单词、整行、自由方式多种选择方式，可复制文本（仅限非扫描版 PDF）
- **大纲编辑**：添加、删除、重命名目录项

### 界面
- **现代化设计**：参考 PDF Expert 的简洁风格，黑白主色调、纸质感、现代优雅
- **响应式布局**：自适应窗口大小变化
- **快捷键支持**：完整的键盘导航

### 实验功能
- **护眼纸质感**：基于 OpenCV 实现扫描版 PDF 护眼纸质感体验
- **OCR 悬停取词**：基于 PaddleOCR(Windows)/Vision(MacOS) 实现的悬停取词，方便与外部词典工具（如 GoldenDict-NG）互动

## 架构

详细说明见 [ARCHITECTURE.md](ARCHITECTURE.md)，以下是快速上手步骤。

### macOS

```bash
# 1. 安装依赖
brew install mupdf opencv brotli jbig2dec openjpeg gumbo-parser dylibbundler

# 2. 安装 Qt6（https://www.qt.io/download-qt-installer），然后设置路径
export Qt6_DIR=/Users/yourname/Qt/6.11.1/macos/lib/cmake/Qt6

# 3. 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)

# 4. 打包（生成可分发 .app）
cmake --build build --target deploy
```

### Windows

**手动配置（已有本地库，已验证）**

设置以下环境变量后直接构建：
```
Qt6_DIR       = D:\Qt6\6.x.x\msvc2022_64\lib\cmake\Qt6
MUPDF_DIR     = D:\Ext-Lib\mupdf-win-x64
OPENCV_DIR    = D:\Ext-Lib\opencv-win-x64
ONNXRUNTIME_DIR = D:\Ext-Lib\onnxruntime-win-x64
```
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## 依赖库

| 库 | 版本 | 用途 |
|----|------|------|
| Qt6 | 6.11.1 (macOS) / 6.8.3 (Windows) | UI 框架 |
| MuPDF | 1.26.11 | PDF 渲染引擎 |
| OpenCV | 4.13.0 | 护眼纸质感处理 |
| onnxruntime | 1.23.2 | OCR 推理（Windows）|
| PaddleOCR | v5 模型 | OCR 识别模型 |
| RapidOCR | 3.4.2 | C++ OCR 实现参考 |
| Clipper2 | 1.5.4 | 多边形裁剪 |
| Jieba | 5.6.0 | 中文分词 |

macOS OCR 使用系统 Vision 框架，不依赖 onnxruntime / PaddleOCR。

## RoadMap

- **批注功能**
  - 高亮、划线
  - 可收纳评论窗
  - 批注管理页面（列表查看、导出）
- **OCR 功能**
  - 框选 OCR（轻量、按需）

## 致谢

- [MuPDF](https://github.com/ArtifexSoftware/mupdf)
- [Qt Framework](https://www.qt.io/)
- [OpenCV](https://github.com/opencv/opencv)
- [PaddleOCR](https://github.com/PaddlePaddle/PaddleOCR)
- [RapidOCR](https://github.com/RapidAI/RapidOCR)
- [Jieba](https://github.com/yanyiwu/cppjieba)
- PDF Expert — UI 设计灵感来源

## 联系方式

项目主页：https://github.com/techfs2026/MuQt

如果这个项目对你有帮助，欢迎 Star！