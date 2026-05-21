# cmake/dependencies.cmake
# 统一管理所有第三方库的查找和链接。
#
# 平台 + 管理方式矩阵：
#   macOS  + Homebrew（默认）：所有库走 Homebrew
#   macOS  + vcpkg           ：mupdf/opencv 仍走 Homebrew（vcpkg 无法稳定下载）
#   Windows + 手动路径（默认）：走环境变量指定的路径
#   Windows + vcpkg           ：opencv / onnxruntime / libmupdf 走 vcpkg

# ============================================================
# Qt（所有平台公共）
# ============================================================
target_link_libraries(MuQt PRIVATE
    Qt::Core
    Qt::Widgets
    Qt6::Concurrent
)

# ============================================================
# macOS：mupdf + opencv 始终走 Homebrew
# （vcpkg 在 macOS 下载 mupdf 源码包网络不稳定，且 Homebrew 已有现成二进制）
# ============================================================
if(APPLE)

    # ── OpenCV ──────────────────────────────────────────────
    find_package(OpenCV REQUIRED)
    target_include_directories(MuQt PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(MuQt PRIVATE ${OpenCV_LIBS})

    # ── MuPDF（Homebrew）────────────────────────────────────
    find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h
        HINTS "${HOMEBREW_PREFIX}/opt/mupdf/include"
        REQUIRED
    )
    find_library(MUPDF_LIB mupdf
        HINTS "${HOMEBREW_PREFIX}/opt/mupdf/lib"
        REQUIRED
    )
    find_library(MUPDF_THIRD_LIB mupdf-third
        HINTS "${HOMEBREW_PREFIX}/opt/mupdf/lib"
        REQUIRED
    )
    target_include_directories(MuQt PRIVATE ${MUPDF_INCLUDE_DIR})

    # ── MuPDF 传递依赖 ───────────────────────────────────────
    find_package(ZLIB     REQUIRED)
    find_package(JPEG     REQUIRED)
    find_package(Freetype REQUIRED)

    find_library(BROTLI_ENC_LIB    brotlienc
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)
    find_library(BROTLI_DEC_LIB    brotlidec
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)
    find_library(BROTLI_COMMON_LIB brotlicommon
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)
    find_library(JBIG2_LIB   jbig2dec
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)
    find_library(OPENJP2_LIB openjp2
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)
    find_library(GUMBO_LIB   gumbo
        HINTS "${HOMEBREW_PREFIX}/lib" REQUIRED)

    target_link_libraries(MuQt PRIVATE
        ${MUPDF_LIB}
        ${MUPDF_THIRD_LIB}
        ${BROTLI_ENC_LIB}
        ${BROTLI_DEC_LIB}
        ${BROTLI_COMMON_LIB}
        ${JBIG2_LIB}
        ${OPENJP2_LIB}
        ${GUMBO_LIB}
        Freetype::Freetype
        JPEG::JPEG
        ZLIB::ZLIB

        # macOS 系统框架
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework Foundation"
        "-framework ImageIO"
        "-framework Vision"
    )

# ============================================================
# Windows
# ============================================================
elseif(WIN32)

    if(BUILD_WITH_VCPKG)
        # vcpkg 提供完整的 cmake config
        find_package(unofficial-mupdf CONFIG REQUIRED)
        find_package(OpenCV            REQUIRED)
        find_package(onnxruntime       CONFIG REQUIRED)

        target_include_directories(MuQt PRIVATE ${OpenCV_INCLUDE_DIRS})

        target_link_libraries(MuQt PRIVATE
            unofficial::mupdf::mupdf
            ${OpenCV_LIBS}
            onnxruntime::onnxruntime
        )

    else()
        # 手动路径（环境变量 MUPDF_DIR / OPENCV_DIR / ONNXRUNTIME_DIR）
        target_include_directories(MuQt PRIVATE
            ${MUPDF_INCLUDE_DIR}
            ${OPENCV_INCLUDE_DIR}
            ${ONNXRUNTIME_INCLUDE_DIR}
        )

        target_link_directories(MuQt PRIVATE
            "$<$<CONFIG:Debug>:${MUPDF_LIB_DEBUG}>"
            "$<$<CONFIG:Release>:${MUPDF_LIB_RELEASE}>"
            "$<$<CONFIG:Debug>:${OPENCV_LIB_DEBUG}>"
            "$<$<CONFIG:Release>:${OPENCV_LIB_RELEASE}>"
            ${ONNXRUNTIME_LIB_DIR}
        )

        target_link_libraries(MuQt PRIVATE
            libmupdf.lib
            libthirdparty.lib
            "$<$<CONFIG:Debug>:opencv_world4120d.lib>"
            "$<$<CONFIG:Release>:opencv_world4120.lib>"
            onnxruntime.lib
        )
    endif()

endif()