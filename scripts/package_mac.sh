#!/usr/bin/env bash
#
# OwlPDF macOS 一键打包：Release 构建 → macdeployqt → 修复依赖路径 → 自检 → 签名 → dmg
#
# 用法：
#   scripts/package_mac.sh                                          # adhoc 签名
#   scripts/package_mac.sh --sign "Developer ID Application: 名字 (TEAMID)"
#   QT_PATH=/path/to/Qt/6.11.1/macos scripts/package_mac.sh         # 指定 Qt（默认从 CMakeCache 推断）
#
# 可选环境变量：QT_PATH、BUILD_DIR（默认 build-release）、APP_NAME（默认 OwlPDF）
#
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="${APP_NAME:-OwlPDF}"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build-release}"
SIGN_IDENTITY="-"   # adhoc；--sign 覆盖

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sign) SIGN_IDENTITY="$2"; shift 2 ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "未知参数：$1" >&2; exit 1 ;;
    esac
done

echo "==> 项目根目录 : ${PROJECT_ROOT}"
echo "==> 构建目录   : ${BUILD_DIR}"
echo "==> 签名身份   : ${SIGN_IDENTITY} ($([[ "${SIGN_IDENTITY}" == "-" ]] && echo adhoc || echo Developer-ID))"
echo

# ---- 1. Release 构建 -----------------------------------------------------
echo "==> [1/5] 配置 + 构建 Release ..."
CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
[[ -n "${QT_PATH:-}" ]] && CMAKE_ARGS+=(-DCMAKE_PREFIX_PATH="${QT_PATH}")
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" "${CMAKE_ARGS[@]}" >/dev/null
cmake --build "${BUILD_DIR}" --config Release --parallel

APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
[[ -d "${APP_BUNDLE}" ]] || { echo "✗ 没找到 ${APP_BUNDLE}" >&2; exit 1; }

# macdeployqt 必须取自构建实际链接的那套 Qt（Qt6_DIR=<qt>/lib/cmake/Qt6，回退三级即根），
# 否则本机双 Qt（官方 6.11.1 + Homebrew 6.11.0）会导致框架/插件版本错配、libqcocoa 加载失败。
QT6_DIR="$(awk -F= '/^Qt6_DIR(:[^=]*)?=/{print $2}' "${BUILD_DIR}/CMakeCache.txt" | head -1)"
[[ -n "${QT6_DIR}" ]] || { echo "✗ CMakeCache 无 Qt6_DIR，请用 QT_PATH 指定 Qt 根" >&2; exit 1; }
QT_PREFIX="$(cd "${QT6_DIR}/../../.." && pwd)"
MACDEPLOYQT="${QT_PREFIX}/bin/macdeployqt"
[[ -x "${MACDEPLOYQT}" ]] || { echo "✗ 未找到 ${MACDEPLOYQT}" >&2; exit 1; }
echo "==> 链接的 Qt   : ${QT_PREFIX}"

# ---- 2. macdeployqt + 修复残留路径 ---------------------------------------
DMG_PATH="${BUILD_DIR}/${APP_NAME}.dmg"
rm -f "${DMG_PATH}"   # macdeployqt 不覆盖旧 dmg

echo "==> [2/5] 运行 macdeployqt ..."
# 过滤 macdeployqt 收尾签名的预期噪音（步骤 4 会重签）
"${MACDEPLOYQT}" "${APP_BUNDLE}" -verbose=1 2>&1 \
    | grep -v -E "codesign verification error|invalid resource directory|In subcomponent:" || true

FRAMEWORKS="${APP_BUNDLE}/Contents/Frameworks"
echo "==> 修复深层第三方依赖（macdeployqt 对 opencv→libgcc 等偶有遗漏）..."

# bundle 内 dylib 自身 id 若仍指向本机路径，改 @rpath
for lib in "${FRAMEWORKS}"/*.dylib; do
    [[ -f "${lib}" ]] || continue
    case "$(otool -D "${lib}" 2>/dev/null | tail -n +2)" in
        /opt/homebrew*|/usr/local*)
            chmod u+w "${lib}"
            install_name_tool -id "@rpath/$(basename "${lib}")" "${lib}" 2>/dev/null || true ;;
    esac
done

# 把残留的 /opt/homebrew、/usr/local 依赖拷进 bundle 并改写成 @rpath，循环到干净
for pass in 1 2 3 4 5; do
    fixed=0
    while IFS= read -r -d '' macho; do
        while IFS= read -r dep; do
            [[ -z "${dep}" ]] && continue
            base="$(basename "${dep}")"
            if [[ ! -f "${FRAMEWORKS}/${base}" ]]; then
                cp -L "${dep}" "${FRAMEWORKS}/${base}" 2>/dev/null || continue
                chmod u+w "${FRAMEWORKS}/${base}"
                install_name_tool -id "@rpath/${base}" "${FRAMEWORKS}/${base}" 2>/dev/null || true
                fixed=1
            fi
            install_name_tool -change "${dep}" "@rpath/${base}" "${macho}" 2>/dev/null && fixed=1
        done < <(otool -L "${macho}" 2>/dev/null | grep -E '^[[:space:]]' \
                    | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local)' || true)
    done < <(find "${APP_BUNDLE}" -type f \( -name '*.dylib' -o -perm +111 \) -print0)
    [[ ${fixed} -eq 0 ]] && break
done

# 删除指向 /opt/homebrew、/usr/local 的 build 时残留 rpath（避免命中外部 Qt）
while IFS= read -r -d '' macho; do
    while IFS= read -r rp; do
        [[ -z "${rp}" ]] && continue
        install_name_tool -delete_rpath "${rp}" "${macho}" 2>/dev/null || true
    done < <(otool -l "${macho}" 2>/dev/null | awk '/LC_RPATH/{f=1} f&&/path /{print $2; f=0}' \
                | grep -E '^(/opt/homebrew|/usr/local)' || true)
done < <(find "${APP_BUNDLE}" -type f \( -name '*.dylib' -o -perm +111 \) -print0)

# ---- 3. 自检：bundle 内不得残留本机绝对路径 ------------------------------
echo "==> [3/5] 自检依赖路径 ..."
LEAK=0
# 只取 otool -L 的缩进依赖行首字段（顶格的是文件自身路径，须排除）；
# 合法依赖为 @rpath/@loader_path/@executable_path 或 /usr/lib、/System
while IFS= read -r -d '' macho; do
    leaks="$(otool -L "${macho}" 2>/dev/null \
        | grep -E '^[[:space:]]' | awk '{print $1}' \
        | grep -E '^(/opt/homebrew|/usr/local|/Users/)' || true)"
    if [[ -n "${leaks}" ]]; then
        echo "  ✗ 残留外部依赖：${macho#${APP_BUNDLE}/}"
        echo "${leaks}" | sed 's/^/      /'
        LEAK=1
    fi
done < <(find "${APP_BUNDLE}" -type f \( -name '*.dylib' -o -perm +111 \) -print0)

if [[ ${LEAK} -ne 0 ]]; then
    echo "✗ 自检未通过：上述库仍指向本机路径，拷到别的 Mac 会启动失败。"
    exit 1
fi
echo "  ✓ bundle 自包含。"

# ---- 4. 代码签名（由内而外）---------------------------------------------
# 必须先签嵌套 dylib/framework/插件，最后才签 .app；否则嵌套框架仍带原签名，
# 与主程序 Team ID 不一致，dyld 会以 "different Team IDs" 拒绝加载（--deep 不可靠）
echo "==> [4/5] 代码签名（${SIGN_IDENTITY}）..."
SIGN_OPTS=(--force --sign "${SIGN_IDENTITY}")
[[ "${SIGN_IDENTITY}" != "-" ]] && SIGN_OPTS+=(--options runtime --timestamp)   # 正式签名启用 hardened runtime

while IFS= read -r -d '' item; do
    codesign "${SIGN_OPTS[@]}" "${item}" 2>/dev/null || true
done < <(find "${APP_BUNDLE}/Contents/Frameworks" "${APP_BUNDLE}/Contents/PlugIns" \
            -type f \( -name '*.dylib' -o -perm +111 \) -print0 2>/dev/null)
for fw in "${APP_BUNDLE}/Contents/Frameworks"/*.framework; do
    [[ -d "${fw}" ]] && codesign "${SIGN_OPTS[@]}" "${fw}" 2>/dev/null || true
done
codesign "${SIGN_OPTS[@]}" "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}" 2>/dev/null || true
codesign "${SIGN_OPTS[@]}" "${APP_BUNDLE}"
codesign --verify --deep --strict "${APP_BUNDLE}" && echo "  ✓ 签名校验通过"

# ---- 5. 生成 dmg ---------------------------------------------------------
echo "==> [5/5] 生成 dmg ..."
STAGING="$(mktemp -d)"
cp -R "${APP_BUNDLE}" "${STAGING}/"
ln -s /Applications "${STAGING}/Applications"   # 拖拽安装
hdiutil create -volname "${APP_NAME}" -srcfolder "${STAGING}" \
    -ov -format UDZO "${DMG_PATH}" >/dev/null
rm -rf "${STAGING}"

echo
echo "✅ 完成：${DMG_PATH}"
echo "   $(du -h "${DMG_PATH}" | cut -f1) · 架构 $(lipo -archs "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}")"
if [[ "${SIGN_IDENTITY}" == "-" ]]; then
    echo
    echo "⚠️  adhoc 签名、未公证：首次打开需【右键 → 打开】，或 xattr -cr /Applications/${APP_NAME}.app"
fi
