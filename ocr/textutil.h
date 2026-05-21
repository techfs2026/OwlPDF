#ifndef TEXTUTIL_H
#define TEXTUTIL_H

#include <string>
#include <cstdint>

namespace TextUtil {

/**
 * @brief 判断 UTF-8 字符串中是否包含中日韩(CJK)字符
 *
 * 从 RapidOCR utils.h 中独立出来，使 ChineseTokenizer / VisionOcrEngine
 * 不再依赖 rapidocr-cpp 的任何头文件。
 *
 * 覆盖区间：
 *   U+4E00–U+9FFF   CJK 统一表意文字（最常用）
 *   U+3400–U+4DBF   CJK 扩展A
 *   U+F900–U+FAFF   CJK 兼容表意文字
 *   U+3000–U+303F   CJK 符号和标点
 */
inline bool hasChineseChar(const std::string& utf8)
{
    size_t i = 0;
    const size_t n = utf8.size();

    while (i < n) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = 0;
        int len = 1;

        if (c < 0x80) {                 // 1-byte ASCII
            cp = c;
            len = 1;
        } else if ((c >> 5) == 0x06) {  // 2-byte
            cp = c & 0x1F;
            len = 2;
        } else if ((c >> 4) == 0x0E) {  // 3-byte（中文绝大多数在这里）
            cp = c & 0x0F;
            len = 3;
        } else if ((c >> 3) == 0x1E) {  // 4-byte
            cp = c & 0x07;
            len = 4;
        } else {                        // 非法起始字节，跳过
            ++i;
            continue;
        }

        if (i + len > n) break;         // 截断保护

        for (int k = 1; k < len; ++k) {
            unsigned char cc = static_cast<unsigned char>(utf8[i + k]);
            if ((cc & 0xC0) != 0x80) {  // 不是合法续接字节
                cp = 0;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }

        if ((cp >= 0x4E00 && cp <= 0x9FFF) ||
            (cp >= 0x3400 && cp <= 0x4DBF) ||
            (cp >= 0xF900 && cp <= 0xFAFF) ||
            (cp >= 0x3000 && cp <= 0x303F)) {
            return true;
        }

        i += len;
    }

    return false;
}

} // namespace TextUtil

#endif // TEXTUTIL_H