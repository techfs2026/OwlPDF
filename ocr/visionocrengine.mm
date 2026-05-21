// visionocrengine.mm  (macOS only)
//
// 完整实现：recognize / recognizeDetailed（整段纯文本）
//          + recognizeTokens（取词，Vision 子串真实坐标 + jieba 分词）
//
// 头文件 visionocrengine.h 中需声明：
//     QVector<TokenWithPosition> recognizeTokens(const QImage& image) override;

#include <QtGlobal>          // 必须在 #ifdef Q_OS_MACOS 之前，宏才有定义
#ifdef Q_OS_MACOS

#include "visionocrengine.h"
#include "chinesetokenizer.h"   // 纯分词 segmentLine

#include <QElapsedTimer>
#include <QDebug>

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

// ============================================================
// QImage -> CGImageRef
// 转成 ARGB32 后构造 CGImage。调用方负责 CGImageRelease。
// ============================================================
static CGImageRef QImageToCGImage(const QImage& src)
{
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    if (img.isNull()) return nullptr;

    const int w = img.width();
    const int h = img.height();

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    // QImage ARGB32 在小端机器上内存布局是 BGRA，
    // 用 kCGImageAlphaPremultipliedFirst + ByteOrder32Little 对应。
    CGBitmapInfo bmp = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;

    CGContextRef ctx = CGBitmapContextCreate(
        (void*)img.bits(),
        w, h, 8,
        img.bytesPerLine(),
        cs, bmp);

    CGImageRef cgImage = nullptr;
    if (ctx) {
        cgImage = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
    }
    CGColorSpaceRelease(cs);
    return cgImage;
}

// ============================================================
// Vision 归一化 box(左下原点) -> 像素四角点(左上原点)
// 输出顺序：左上, 右上, 右下, 左下
// ============================================================
static std::vector<cv::Point2f> normalizedRectToPixelQuad(
    CGRect bbox, int imgW, int imgH)
{
    float x = bbox.origin.x * imgW;
    float y_bottom = bbox.origin.y * imgH;
    float wpx = bbox.size.width  * imgW;
    float hpx = bbox.size.height * imgH;

    float top    = imgH - (y_bottom + hpx);   // 翻转 Y
    float bottom = imgH - y_bottom;
    float left   = x;
    float right  = x + wpx;

    std::vector<cv::Point2f> quad;
    quad.reserve(4);
    quad.emplace_back(left,  top);
    quad.emplace_back(right, top);
    quad.emplace_back(right, bottom);
    quad.emplace_back(left,  bottom);
    return quad;
}

// ============================================================
// Vision 归一化 box(左下原点) -> 像素 QRect(左上原点)
// 用于 token 的矩形位置
// ============================================================
static QRect normalizedRectToPixelQRect(CGRect bbox, int imgW, int imgH)
{
    float left    = bbox.origin.x * imgW;
    float wpx     = bbox.size.width  * imgW;
    float hpx     = bbox.size.height * imgH;
    float yBottom = bbox.origin.y * imgH;
    float top     = imgH - (yBottom + hpx);   // 翻转 Y

    return QRect(static_cast<int>(left),
                 static_cast<int>(top),
                 static_cast<int>(wpx),
                 static_cast<int>(hpx));
}

// ============================================================
// 构造 / 析构 / 状态
// ============================================================
VisionOcrEngine::VisionOcrEngine(QObject* parent)
    : IOCREngine(parent)
{
}

VisionOcrEngine::~VisionOcrEngine() = default;

void VisionOcrEngine::setState(OCREngineState s)
{
    if (m_state != s) {
        m_state = s;
        emit stateChanged(s);
    }
}

// ============================================================
// 初始化（Vision 无需模型文件）
// ============================================================
bool VisionOcrEngine::initializeSync(const QString& /*modelDir*/)
{
    setState(OCREngineState::Loading);

    if (@available(macOS 10.15, *)) {
        setState(OCREngineState::Ready);
        qInfo() << "VisionOcrEngine: ready (system Vision)";
        return true;
    } else {
        m_lastError = QStringLiteral("当前 macOS 版本不支持 Vision OCR");
        setState(OCREngineState::Error);
        return false;
    }
}

bool VisionOcrEngine::initializeAsync(const QString& modelDir)
{
    bool ok = initializeSync(modelDir);
    emit initialized(ok, ok ? QString() : m_lastError);
    return ok;
}

// ============================================================
// recognize / recognizeDetailed（整段纯文本：复制、全文搜索）
// ============================================================
OCRResult VisionOcrEngine::recognize(const QImage& image)
{
    return runVision(image, /*detailed*/false);
}

OCRResult VisionOcrEngine::recognizeDetailed(const QImage& image)
{
    return runVision(image, /*detailed*/true);
}

OCRResult VisionOcrEngine::runVision(const QImage& image, bool /*detailed*/)
{
    OCRResult result;

    if (m_state != OCREngineState::Ready) {
        result.error = QStringLiteral("OCR引擎未就绪");
        return result;
    }
    if (image.isNull() || image.width() < 10 || image.height() < 10) {
        result.error = QStringLiteral("输入图像无效");
        return result;
    }

    setState(OCREngineState::Processing);
    QElapsedTimer timer;
    timer.start();

    const int imgW = image.width();
    const int imgH = image.height();

    CGImageRef cgImage = QImageToCGImage(image);
    if (!cgImage) {
        result.error = QStringLiteral("图像转换失败");
        setState(OCREngineState::Ready);
        return result;
    }

    @autoreleasepool {
        if (@available(macOS 10.15, *)) {
            VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];
            request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
            request.usesLanguageCorrection = YES;

            if (@available(macOS 13.0, *)) {
                request.recognitionLanguages = @[ @"zh-Hans", @"zh-Hant", @"en-US" ];
            } else {
                request.recognitionLanguages = @[ @"en-US" ];
            }

            VNImageRequestHandler* handler =
                [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];

            NSError* err = nil;
            BOOL ok = [handler performRequests:@[ request ] error:&err];

            if (!ok || err) {
                result.error = QString::fromNSString(
                    err ? err.localizedDescription : @"Vision 识别失败");
            } else {
                QStringList lines;
                float scoreSum = 0.0f;
                int   scoreCnt = 0;

                for (VNRecognizedTextObservation* obs in request.results) {
                    VNRecognizedText* top = [[obs topCandidates:1] firstObject];
                    if (!top) continue;

                    NSString* s = top.string;
                    if (s.length == 0) continue;

                    QString line = QString::fromNSString(s);
                    lines << line;
                    result.texts.push_back(line.toStdString());
                    result.scores.push_back(top.confidence);
                    scoreSum += top.confidence;
                    ++scoreCnt;

                    result.boxes.push_back(
                        normalizedRectToPixelQuad(obs.boundingBox, imgW, imgH));
                }

                if (scoreCnt > 0) {
                    result.success = true;
                    result.text = lines.join("\n");
                    result.confidence = scoreSum / scoreCnt;
                } else {
                    result.error = QStringLiteral("未识别到文本");
                }
            }
        } else {
            result.error = QStringLiteral("当前 macOS 版本不支持 Vision OCR");
        }
    }

    CGImageRelease(cgImage);

    result.elapsedTime = timer.elapsed() / 1000.0f;
    setState(OCREngineState::Ready);

    if (result.success) {
        emit recognitionCompleted(result);
        qInfo() << "VisionOcrEngine: recognized" << result.texts.size()
                << "lines in" << result.elapsedTime << "s";
    }
    return result;
}

// ============================================================
// recognizeTokens（取词：Vision 子串真实坐标 + jieba 分词）
// 根治方案，不使用比例估算
// ============================================================
QVector<TokenWithPosition> VisionOcrEngine::recognizeTokens(const QImage& image)
{
    QVector<TokenWithPosition> tokens;

    if (m_state != OCREngineState::Ready) {
        m_lastError = QStringLiteral("OCR引擎未就绪");
        return tokens;
    }
    if (image.isNull() || image.width() < 10 || image.height() < 10) {
        m_lastError = QStringLiteral("输入图像无效");
        return tokens;
    }

    const int imgW = image.width();
    const int imgH = image.height();

    CGImageRef cgImage = QImageToCGImage(image);
    if (!cgImage) {
        m_lastError = QStringLiteral("图像转换失败");
        return tokens;
    }

    setState(OCREngineState::Processing);

    @autoreleasepool {
        if (@available(macOS 10.15, *)) {
            VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc] init];
            request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
            request.usesLanguageCorrection = YES;
            if (@available(macOS 13.0, *)) {
                request.recognitionLanguages = @[ @"zh-Hans", @"zh-Hant", @"en-US" ];
            } else {
                request.recognitionLanguages = @[ @"en-US" ];
            }

            VNImageRequestHandler* handler =
                [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];

            NSError* err = nil;
            BOOL ok = [handler performRequests:@[ request ] error:&err];

            if (ok && !err) {
                int lineIndex = 0;
                for (VNRecognizedTextObservation* obs in request.results) {
                    VNRecognizedText* top = [[obs topCandidates:1] firstObject];
                    if (!top) { ++lineIndex; continue; }

                    NSString* nsLine = top.string;
                    if (nsLine.length == 0) { ++lineIndex; continue; }

                    QString line = QString::fromNSString(nsLine);

                    // 1) 纯分词（jieba 处理中文 + 合并英文单字母）
                    QVector<WordSpan> spans =
                        ChineseTokenizer::instance().segmentLine(line);

                    const std::string lineUtf8 = line.toStdString();

                    for (const WordSpan& ws : spans) {
                        // UTF-8 字节偏移 → QString(UTF-16) 字符索引
                        QString prefixStart =
                            QString::fromStdString(lineUtf8.substr(0, ws.byteStart));
                        QString prefixWord =
                            QString::fromStdString(lineUtf8.substr(0, ws.byteEnd));
                        int u16Start = prefixStart.length();
                        int u16Len   = prefixWord.length() - u16Start;
                        if (u16Len <= 0) continue;

                        NSRange range = NSMakeRange(static_cast<NSUInteger>(u16Start),
                                                    static_cast<NSUInteger>(u16Len));

                        QRect rect;
                        if (@available(macOS 11.0, *)) {
                            VNRectangleObservation* box =
                                [top boundingBoxForRange:range error:nil];
                            if (box) {
                                rect = normalizedRectToPixelQRect(
                                    box.boundingBox, imgW, imgH);
                            }
                        }
                        if (rect.isNull() || rect.width() <= 0) {
                            // 回退：用整行框（macOS 11+ 实际不会触发）
                            rect = normalizedRectToPixelQRect(
                                obs.boundingBox, imgW, imgH);
                        }

                        TokenWithPosition tk;
                        tk.word          = ws.word;
                        tk.startIndex    = ws.byteStart;
                        tk.endIndex      = ws.byteEnd;
                        tk.lineIndex     = lineIndex;
                        tk.estimatedRect = rect;
                        tk.confidence    = top.confidence;
                        tokens.append(tk);
                    }
                    ++lineIndex;
                }
            } else {
                m_lastError = QString::fromNSString(
                    err ? err.localizedDescription : @"Vision 识别失败");
            }
        } else {
            m_lastError = QStringLiteral("当前 macOS 版本不支持 Vision OCR");
        }
    }

    CGImageRelease(cgImage);
    setState(OCREngineState::Ready);

    qInfo() << "VisionOcrEngine: tokenized" << tokens.size() << "words";
    return tokens;
}

#endif // Q_OS_MACOS