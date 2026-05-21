#ifndef CHINESETOKENIZER_H
#define CHINESETOKENIZER_H

#include <QString>
#include <QStringList>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <memory>
#include "cppjieba/Jieba.hpp"
#include "iocrengine.h"   // OCRResult / TokenWithPosition 公共契约

/**
 * @brief 一个分词单元：词 + 在原 UTF-8 行内的字节偏移
 *   纯分词结果，不含像素位置。位置由调用方（引擎）决定如何赋予：
 *     · Windows：比例估算
 *     · macOS：Vision 子串真实坐标
 */
struct WordSpan {
    QString word;
    int byteStart = 0;   // UTF-8 字节偏移（jieba 的 offset 即字节）
    int byteEnd   = 0;
};

/**
 * @brief 中文分词器（cppjieba 封装）
 *
 * 跨平台共用。两类能力：
 *   1) 纯分词：segmentLine() —— 给一行文本，返回 WordSpan 列表（含合并英文单字母）
 *   2) Windows 专属：tokenizeWithPosition() —— 比例估算位置（RapidOCR 只有行框）
 */
class ChineseTokenizer
{
public:
    static ChineseTokenizer& instance();

    bool initialize(const QString& dictDir);
    bool isInitialized() const { return m_initialized; }

    QStringList tokenize(const QString& text);
    QStringList tokenizeEnglish(const QString& text);

    /**
     * @brief 纯分词：对一行文本分词，返回带字节偏移的词列表。
     *   自动处理中英混排：jieba 切中文，连续单字母拉丁合并为英文单词。
     *   不含像素坐标——由调用方赋予。两平台共用。
     */
    QVector<WordSpan> segmentLine(const QString& lineText);

    /**
     * @brief Windows 专属：对 OCRResult 分词并用比例估算位置。
     *   （RapidOCR 只能给行框，故只能估算词位置）
     */
    QVector<TokenWithPosition> tokenizeWithPosition(const OCRResult& ocr);

    QRect boundingRectFromBox(const std::vector<cv::Point2f>& box);
    QRect estimateWordRectInLine(int startIndex, int endIndex,
                                 int totalLength, const QRect& lineRect);

    TokenWithPosition findClosestToken(const QVector<TokenWithPosition>& tokens,
                                       const QPoint& mousePos);

    QString lastError() const { return m_lastError; }

private:
    ChineseTokenizer();
    ~ChineseTokenizer();
    ChineseTokenizer(const ChineseTokenizer&) = delete;
    ChineseTokenizer& operator=(const ChineseTokenizer&) = delete;

    double distanceToRect(const QPoint& point, const QRect& rect);
    QVector<TokenWithPosition> tokenizeEnglishLine(const QString& text,
                                                   const QRect& lineRect,
                                                   int lineIndex);

    std::unique_ptr<cppjieba::Jieba> m_jieba;
    bool m_initialized;
    QString m_lastError;
    QString m_dictDir;
};

#endif // CHINESETOKENIZER_H