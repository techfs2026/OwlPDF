cmake_minimum_required(VERSION 3.19)
project(MuQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# macOS 上需要 Objective-C++ 来编译 .mm（Vision 桥接）
if(APPLE)
    enable_language(OBJCXX)
endif()

# ============================================================
# Qt 路径：优先使用环境变量 Qt6_DIR，其次 CMAKE_PREFIX_PATH
# 开发者在自己机器上设置一次环境变量即可，无需修改此文件：
#
#   macOS/Linux:  export Qt6_DIR=/Users/yourname/Qt/6.x.x/macos
#   Windows CMD:  set Qt6_DIR=D:\Qt6\6.x.x\msvc2022_64
#   Windows PS:   $env:Qt6_DIR="D:\Qt6\6.x.x\msvc2022_64"
#
# 也可以在 cmake 命令行传入：
#   cmake -DQt6_DIR=/path/to/qt/lib/cmake/Qt6 ..
# ============================================================
if(DEFINED ENV{Qt6_DIR})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
    message(STATUS "Qt6_DIR from environment: $ENV{Qt6_DIR}")
elseif(DEFINED Qt6_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${Qt6_DIR}")
    message(STATUS "Qt6_DIR from cmake variable: ${Qt6_DIR}")
else()
    message(WARNING
        "Qt6_DIR is not set. CMake will search system paths.\n"
        "If Qt is not found, set Qt6_DIR:\n"
        "  export Qt6_DIR=/path/to/Qt/6.x.x/macos   (macOS/Linux)\n"
        "  set Qt6_DIR=D:\\Qt6\\6.x.x\\msvc2022_64   (Windows)\n"
        "Or pass -DQt6_DIR=... to cmake."
    )
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/deploy")

# ============================================================
# 依赖管理方式选择
#   BUILD_WITH_VCPKG=ON  → 使用 vcpkg（推荐，适合 CI / 新成员）
#   BUILD_WITH_VCPKG=OFF → 使用系统库（Homebrew/手动安装，默认）
# ============================================================
option(BUILD_WITH_VCPKG "Use vcpkg for dependency management" OFF)

if(BUILD_WITH_VCPKG)
    include(cmake/vcpkg.cmake)
endif()

# ============================================================
# 平台相关：Homebrew 路径（仅 macOS 且未使用 vcpkg 时）
# ============================================================
if(APPLE AND NOT BUILD_WITH_VCPKG)
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT HOMEBREW_PREFIX)
        set(HOMEBREW_PREFIX "/opt/homebrew")
    endif()
    message(STATUS "Homebrew prefix: ${HOMEBREW_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
endif()

# ============================================================
# Windows：手动库路径（仍保留，供未使用 vcpkg 的 Windows 开发者）
# 可通过环境变量或 cmake -D 传入，不再写死路径
# ============================================================
if(WIN32 AND NOT BUILD_WITH_VCPKG)
    # MUPDF
    if(DEFINED ENV{MUPDF_DIR})
        set(MUPDF_DIR "$ENV{MUPDF_DIR}")
    elseif(NOT DEFINED MUPDF_DIR)
        set(MUPDF_DIR "D:/Ext-Lib/mupdf-win-x64")  # 回退默认值
    endif()

    # OpenCV
    if(DEFINED ENV{OPENCV_DIR})
        set(OPENCV_DIR "$ENV{OPENCV_DIR}")
    elseif(NOT DEFINED OPENCV_DIR)
        set(OPENCV_DIR "D:/Ext-Lib/opencv-win-x64")
    endif()

    # ONNX Runtime
    if(DEFINED ENV{ONNXRUNTIME_DIR})
        set(ONNXRUNTIME_DIR "$ENV{ONNXRUNTIME_DIR}")
    elseif(NOT DEFINED ONNXRUNTIME_DIR)
        set(ONNXRUNTIME_DIR "D:/Ext-Lib/onnxruntime-win-x64")
    endif()

    message(STATUS "MUPDF_DIR:       ${MUPDF_DIR}")
    message(STATUS "OPENCV_DIR:      ${OPENCV_DIR}")
    message(STATUS "ONNXRUNTIME_DIR: ${ONNXRUNTIME_DIR}")

    set(MUPDF_INCLUDE_DIR       "${MUPDF_DIR}/include")
    set(MUPDF_LIB_DEBUG         "${MUPDF_DIR}/lib/Debug")
    set(MUPDF_LIB_RELEASE       "${MUPDF_DIR}/lib/Release")

    set(OPENCV_INCLUDE_DIR      "${OPENCV_DIR}/include")
    set(OPENCV_LIB_DEBUG        "${OPENCV_DIR}/lib/Debug")
    set(OPENCV_LIB_RELEASE      "${OPENCV_DIR}/lib/Release")
    set(OPENCV_BIN_DEBUG        "${OPENCV_DIR}/bin/Debug")
    set(OPENCV_BIN_RELEASE      "${OPENCV_DIR}/bin/Release")

    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
    set(ONNXRUNTIME_LIB_DIR     "${ONNXRUNTIME_DIR}/lib")
    set(ONNXRUNTIME_BIN_DIR     "${ONNXRUNTIME_DIR}/lib")
endif()

# ============================================================
# Qt6
# ============================================================
if(WIN32)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools)
elseif(APPLE)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools)
else()
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools)
endif()

# ============================================================
# 应用图标
# ============================================================
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE "app.icns")
    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/app.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

# ============================================================
# 源文件收集
# ============================================================
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
)

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_BINARY_DIR}")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*_autogen.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/vcpkg_installed/.*")

if(APPLE)
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/rapidocr-cpp/.*")
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*rapidocrengine\\.cpp$")
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*rapidocrengine\\.h$")
else()
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*visionocrengine\\.mm$")
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*visionocrengine\\.h$")
endif()

# ============================================================
# 翻译文件
# ============================================================
set(TS_FILES
    translations/muqt_zh_CN.ts
    translations/muqt_en_US.ts
)

qt_standard_project_setup()

# ============================================================
# 可执行目标
# ============================================================
qt_add_executable(MuQt
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    $<$<BOOL:${WIN32}>:${APP_ICON_RESOURCE_WINDOWS}>
    $<$<BOOL:${APPLE}>:${APP_ICON_MACOS}>
)

qt_add_translations(MuQt
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

# ============================================================
# 依赖查找与链接（统一入口，根据平台和管理方式分支）
# ============================================================
include(cmake/dependencies.cmake)

# ============================================================
# 头文件目录（项目自身）
# ============================================================
target_include_directories(MuQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ocr
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/theme
    ${CMAKE_CURRENT_SOURCE_DIR}/session
    ${CMAKE_CURRENT_SOURCE_DIR}/model
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/util
    ${CMAKE_CURRENT_SOURCE_DIR}/tool
    ${CMAKE_CURRENT_SOURCE_DIR}/manager
    ${CMAKE_CURRENT_SOURCE_DIR}/handler
    ${CMAKE_CURRENT_SOURCE_DIR}/ocr
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

# ============================================================
# POST_BUILD：资源拷贝
# ============================================================
include(cmake/postbuild.cmake)

# ============================================================
# 部署目标（macdeployqt / windeployqt）
# ============================================================
include(cmake/deploy.cmake)

# ============================================================
# 安装规则
# ============================================================
include(GNUInstallDirs)

install(TARGETS MuQt
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(NOT APPLE)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/ocr/models"
        DESTINATION ${CMAKE_INSTALL_BINDIR}/ocr
        OPTIONAL
    )
endif()

qt_generate_deploy_app_script(
    TARGET MuQt
    OUTPUT_SCRIPT deploy_script
)
install(SCRIPT ${deploy_script})