cmake_minimum_required(VERSION 3.19)
project(MuQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "D:/Qt6/6.8.3/msvc2022_64")
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/deploy")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools)

if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

set(MUPDF_DIR "D:/Ext-Lib/mupdf-win-x64")
set(MUPDF_INCLUDE_DIR "${MUPDF_DIR}/include")
set(MUPDF_LIB_DEBUG   "${MUPDF_DIR}/lib/Debug")
set(MUPDF_LIB_RELEASE "${MUPDF_DIR}/lib/Release")

set(OPENCV_DIR "D:/Ext-Lib/opencv-win-x64")
set(OPENCV_INCLUDE_DIR "${OPENCV_DIR}/include")
set(OPENCV_LIB_DEBUG   "${OPENCV_DIR}/lib/Debug")
set(OPENCV_LIB_RELEASE "${OPENCV_DIR}/lib/Release")
set(OPENCV_BIN_DEBUG   "${OPENCV_DIR}/bin/Debug")
set(OPENCV_BIN_RELEASE "${OPENCV_DIR}/bin/Release")

set(ONNXRUNTIME_DIR "D:/Ext-Lib/onnxruntime-win-x64")
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")
set(ONNXRUNTIME_BIN_DIR "${ONNXRUNTIME_DIR}/lib")

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
)

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_BINARY_DIR}")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*_autogen.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")

set(TS_FILES
    translations/muqt_zh_CN.ts
    translations/muqt_en_US.ts
)

qt_standard_project_setup()

qt_add_executable(MuQt
    WIN32 MACOSX_BUNDLE
    ${PROJECT_SOURCES}
    ${APP_ICON_RESOURCE_WINDOWS}
)

qt_add_translations(MuQt
    TS_FILES ${TS_FILES}
    RESOURCE_PREFIX "/translations"
)

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
    Qt::Core
    Qt::Widgets
    Qt6::Concurrent

    libmupdf.lib
    libthirdparty.lib

    "$<$<CONFIG:Debug>:opencv_world4120d.lib>"
    "$<$<CONFIG:Release>:opencv_world4120.lib>"

    onnxruntime.lib
)

add_custom_command(TARGET MuQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenCV DLL for $<CONFIG> configuration..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<IF:$<CONFIG:Debug>,${OPENCV_BIN_DEBUG}/opencv_world4120d.dll,${OPENCV_BIN_RELEASE}/opencv_world4120.dll>"
        "$<TARGET_FILE_DIR:MuQt>/"
    COMMENT "Copying OpenCV DLL"
    VERBATIM
)

add_custom_command(TARGET MuQt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying ONNX Runtime DLLs..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_BIN_DIR}/onnxruntime.dll"
        "$<TARGET_FILE_DIR:MuQt>/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_BIN_DIR}/onnxruntime_providers_shared.dll"
        "$<TARGET_FILE_DIR:MuQt>/"
    COMMENT "Copying ONNX Runtime DLLs"
    VERBATIM
)

set(OCR_MODELS_SRC "${CMAKE_SOURCE_DIR}/ocr/models")
set(OCR_MODELS_DST "$<TARGET_FILE_DIR:MuQt>/ocr/models")

if(EXISTS "${OCR_MODELS_SRC}")
    add_custom_command(TARGET MuQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OCR_MODELS_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${OCR_MODELS_SRC}
            ${OCR_MODELS_DST}
        COMMENT "Copying OCR models to output directory"
    )
else()
    message(WARNING "OCR models directory not found: ${OCR_MODELS_SRC}")
endif()

set(JIEBA_DICT_SRC "${CMAKE_SOURCE_DIR}/ocr/dict")
set(JIEBA_DICT_DST "$<TARGET_FILE_DIR:MuQt>/ocr/dict")

if(EXISTS "${JIEBA_DICT_SRC}")
    add_custom_command(TARGET MuQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${JIEBA_DICT_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${JIEBA_DICT_SRC}
            ${JIEBA_DICT_DST}
        COMMENT "Copying jieba dict to output directory"
    )
else()
    message(WARNING "jieba dict directory not found: ${JIEBA_DICT_SRC}")
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MuQt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Deploying Qt dependencies..."
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-compiler-runtime
                "$<TARGET_FILE:MuQt>"
            COMMENT "Running windeployqt to deploy Qt DLLs"
            VERBATIM
        )
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "windeployqt not found. Qt DLLs will not be automatically copied.")
        message(WARNING "Please run windeployqt manually after build.")
    endif()
endif()

include(GNUInstallDirs)

install(TARGETS MuQt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${ONNX_DLLS}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL
)

install(DIRECTORY "${OCR_MODELS_SRC}"
    DESTINATION ${CMAKE_INSTALL_BINDIR}/ocr
    OPTIONAL
)

qt_generate_deploy_app_script(
    TARGET MuQt
    OUTPUT_SCRIPT deploy_script
)
install(SCRIPT ${deploy_script})
