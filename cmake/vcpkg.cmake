# cmake/vcpkg.cmake
# 当 BUILD_WITH_VCPKG=ON 时自动引导 vcpkg toolchain。
#
# !! 仅支持 Windows !!
# macOS 请使用 Homebrew 安装依赖，不要传入 BUILD_WITH_VCPKG=ON。
#
# 使用方式（Windows）：
#   cmake -DBUILD_WITH_VCPKG=ON -G "Visual Studio 17 2022" -A x64 -B build
#
# vcpkg 会被自动 clone 到 <项目根>/vcpkg，
# 也可以通过 VCPKG_ROOT 环境变量指向已有的 vcpkg 安装。

if(APPLE)
    message(FATAL_ERROR
        "BUILD_WITH_VCPKG=ON is not supported on macOS.\n"
        "Please use Homebrew to install dependencies:\n"
        "  brew install mupdf opencv brotli jbig2dec openjpeg gumbo-parser dylibbundler\n"
        "Then build without the flag:\n"
        "  cmake -B build -DCMAKE_BUILD_TYPE=Release"
    )
endif()

# ── 1. 确定 vcpkg 根目录 ────────────────────────────────────
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    message(STATUS "Using VCPKG_ROOT from environment: ${VCPKG_ROOT}")
elseif(DEFINED VCPKG_ROOT)
    message(STATUS "Using VCPKG_ROOT from cmake variable: ${VCPKG_ROOT}")
else()
    set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg")
    message(STATUS "VCPKG_ROOT not set, defaulting to: ${VCPKG_ROOT}")
endif()

# ── 2. 如果 vcpkg 目录不存在则自动 clone ────────────────────
if(NOT EXISTS "${VCPKG_ROOT}/bootstrap-vcpkg.bat")
    message(STATUS "vcpkg not found at ${VCPKG_ROOT}, cloning...")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone
            --depth 1
            https://github.com/microsoft/vcpkg.git
            "${VCPKG_ROOT}"
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to clone vcpkg.\n"
            "Check internet connection or set VCPKG_ROOT to an existing vcpkg installation."
        )
    endif()
endif()

# ── 3. 引导 vcpkg（生成可执行文件）────────────────────────────
if(NOT EXISTS "${VCPKG_ROOT}/vcpkg.exe")
    message(STATUS "Bootstrapping vcpkg...")
    execute_process(
        COMMAND "${VCPKG_ROOT}/bootstrap-vcpkg.bat" -disableMetrics
        WORKING_DIRECTORY "${VCPKG_ROOT}"
        RESULT_VARIABLE BOOTSTRAP_RESULT
    )
    if(NOT BOOTSTRAP_RESULT EQUAL 0)
        message(FATAL_ERROR "vcpkg bootstrap failed.")
    endif()
endif()

# ── 4. 在 toolchain 加载前保护 Qt 路径 ────────────────────────
# vcpkg toolchain 会接管 CMAKE_PREFIX_PATH，提前写入 CACHE 保证优先级。
if(DEFINED ENV{Qt6_DIR})
    set(_QT6_DIR_VAL "$ENV{Qt6_DIR}")
elseif(DEFINED Qt6_DIR)
    set(_QT6_DIR_VAL "${Qt6_DIR}")
endif()

if(DEFINED _QT6_DIR_VAL)
    set(Qt6_DIR "${_QT6_DIR_VAL}" CACHE PATH "Qt6 cmake dir" FORCE)
    get_filename_component(_QT_ROOT "${_QT6_DIR_VAL}/../../.." ABSOLUTE)
    set(CMAKE_PREFIX_PATH "${_QT_ROOT}" CACHE STRING "CMake prefix path" FORCE)
    message(STATUS "Protecting Qt path from vcpkg toolchain: ${_QT_ROOT}")
endif()

# ── 5. 设置 toolchain ─────────────────────────────────────────
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file" FORCE)

message(STATUS "vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")