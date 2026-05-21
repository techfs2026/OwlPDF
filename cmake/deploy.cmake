# cmake/deploy.cmake
# 打包部署目标。
#
# macOS：cmake --build build --target deploy
# Windows：cmake --build build --target deploy（或 POST_BUILD 自动运行）

# ============================================================
# macOS：macdeployqt + dylibbundler
# ============================================================
if(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${CMAKE_PREFIX_PATH}/bin"
    )
    find_program(DYLIBBUNDLER_EXECUTABLE dylibbundler
        HINTS "${HOMEBREW_PREFIX}/bin" "/usr/local/bin" "/opt/homebrew/bin"
    )

    if(MACDEPLOYQT_EXECUTABLE)
        if(DYLIBBUNDLER_EXECUTABLE)
            # 完整打包：Qt 库 + 所有第三方 dylib 全部打入 .app
            add_custom_target(deploy
                DEPENDS MuQt
                VERBATIM
                COMMENT "Packaging MuQt.app (macdeployqt + dylibbundler)..."

                # 步骤1：macdeployqt 处理 Qt 框架和插件
                COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_BUNDLE_DIR:MuQt>"

                # 步骤2：dylibbundler 递归打包所有第三方 dylib
                # -od：覆盖已有文件
                # -b ：修改二进制文件的 rpath
                # -x ：要处理的可执行文件
                # -d ：dylib 目标目录（打入 .app/Contents/Frameworks）
                # -p ：运行时搜索路径（相对于可执行文件）
                COMMAND "${DYLIBBUNDLER_EXECUTABLE}"
                    -od -b
                    -x "$<TARGET_BUNDLE_DIR:MuQt>/Contents/MacOS/MuQt"
                    -d "$<TARGET_BUNDLE_DIR:MuQt>/Contents/Frameworks"
                    -p "@executable_path/../Frameworks"
            )
            message(STATUS "deploy target: macdeployqt + dylibbundler")
            message(STATUS "  -> Run: cmake --build . --target deploy")
        else()
            # 仅 macdeployqt（不含第三方 dylib，需要用户安装 Homebrew 库）
            add_custom_target(deploy
                DEPENDS MuQt
                VERBATIM
                COMMENT "Packaging MuQt.app (macdeployqt only)..."
                COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                    "$<TARGET_BUNDLE_DIR:MuQt>"
            )
            message(WARNING
                "dylibbundler not found. Third-party dylibs (mupdf, opencv, etc.) "
                "will NOT be bundled into the .app.\n"
                "Install it with: brew install dylibbundler\n"
                "Then re-run cmake."
            )
        endif()
    else()
        message(WARNING "macdeployqt not found. Set Qt6_DIR to the Qt installation path.")
    endif()

# ============================================================
# Windows：windeployqt（自动在 POST_BUILD 运行）
# ============================================================
elseif(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${CMAKE_PREFIX_PATH}/bin"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        # Windows 选择在 POST_BUILD 自动运行，无需手动调用 target
        add_custom_command(TARGET MuQt POST_BUILD
            COMMENT "Running windeployqt..."
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-compiler-runtime
                "$<TARGET_FILE:MuQt>"
            VERBATIM
        )
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "windeployqt not found. Run it manually after build.")
    endif()

endif()