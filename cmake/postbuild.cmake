# cmake/postbuild.cmake
# POST_BUILD 步骤：拷贝运行时动态库、OCR 模型、Jieba 词典。

set(OCR_MODELS_SRC "${CMAKE_SOURCE_DIR}/ocr/models")
set(JIEBA_DICT_SRC "${CMAKE_SOURCE_DIR}/ocr/dict")

# ============================================================
# Windows：拷贝 DLL
# ============================================================
if(WIN32 AND NOT BUILD_WITH_VCPKG)
    # OpenCV DLL
    add_custom_command(TARGET MuQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenCV DLL for $<CONFIG>..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Debug>,${OPENCV_BIN_DEBUG}/opencv_world4120d.dll,${OPENCV_BIN_RELEASE}/opencv_world4120.dll>"
            "$<TARGET_FILE_DIR:MuQt>/"
        VERBATIM
    )

    # ONNX Runtime DLL
    add_custom_command(TARGET MuQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying ONNX Runtime DLLs..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_BIN_DIR}/onnxruntime.dll"
            "$<TARGET_FILE_DIR:MuQt>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_BIN_DIR}/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:MuQt>/"
        VERBATIM
    )
    # vcpkg 模式下 DLL 由 windeployqt / cmake install 自动处理
endif()

# ============================================================
# OCR 目标路径（平台差异）
# ============================================================
if(WIN32)
    set(OCR_MODELS_DST "$<TARGET_FILE_DIR:MuQt>/ocr/models")
    set(JIEBA_DICT_DST "$<TARGET_FILE_DIR:MuQt>/ocr/dict")
elseif(APPLE)
    # macOS bundle：资源放在 Contents/Resources
    set(JIEBA_DICT_DST "$<TARGET_BUNDLE_CONTENT_DIR:MuQt>/Resources/ocr/dict")
endif()

# ============================================================
# OCR 模型：仅 Windows（macOS 用 Vision 框架，不需要模型文件）
# ============================================================
if(WIN32)
    if(EXISTS "${OCR_MODELS_SRC}")
        add_custom_command(TARGET MuQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OCR_MODELS_DST}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${OCR_MODELS_SRC}" "${OCR_MODELS_DST}"
            COMMENT "Copying OCR models"
        )
    else()
        message(WARNING "OCR models directory not found: ${OCR_MODELS_SRC}")
    endif()
endif()

# ============================================================
# Jieba 词典：所有平台
# ============================================================
if(EXISTS "${JIEBA_DICT_SRC}")
    add_custom_command(TARGET MuQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${JIEBA_DICT_DST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${JIEBA_DICT_SRC}" "${JIEBA_DICT_DST}"
        COMMENT "Copying Jieba dict"
    )
else()
    message(WARNING "Jieba dict directory not found: ${JIEBA_DICT_SRC}")
endif()