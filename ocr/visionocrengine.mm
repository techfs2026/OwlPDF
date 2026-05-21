#include <QtGlobal>

#ifdef Q_OS_MACOS
#include "visionocrengine.h"

#include <QElapsedTimer>
#include <QDebug>

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

// ------------------------------------------------------------
// QImage -> CGImageRef
// 转成 ARGB32 后构造 CGImage。调用方负责 CGImageRelease。
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Vision 归一化 box(左下原点) -> 像素四角点(左上原点)
// Vision 的 boundingBox: origin 在左下，x/y/width/height ∈ [0,1]
// 输出顺序：左上, 右上, 右下, 左下（与常见 OCR det 输出一致）
// ------------------------------------------------------------
static std::vector<cv::Point2f> normalizedRectToPixelQuad(
    CGRect bbox, int imgW, int imgH)
{
    float x = bbox.origin.x * imgW;
    float y_bottom = bbox.origin.y * imgH;          // 距底部
    float wpx = bbox.size.width  * imgW;
    float hpx = bbox.size.height * imgH;

    // 翻转 Y：像素坐标系原点在左上
    float top    = imgH - (y_bottom + hpx);
    float bottom = imgH - y_bottom;
    float left   = x;
    float right  = x + wpx;

    std::vector<cv::Point2f> quad;
    quad.reserve(4);
    quad.emplace_back(left,  top);      // 左上
    quad.emplace_back(right, top);      // 右上
    quad.emplace_back(right, bottom);   // 右下
    quad.emplace_back(left,  bottom);   // 左下
    return quad;
}

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

// Vision 无需加载模型文件，初始化只是校验系统可用性
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
    // Vision 初始化是瞬时的，直接同步完成后发信号即可
    bool ok = initializeSync(modelDir);
    emit initialized(ok, ok ? QString() : m_lastError);
    return ok;
}

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

            // 中英文优先。macOS 13+ 中文支持成熟。
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
                    VNRecognizedText* top =
                        [[obs topCandidates:1] firstObject];
                    if (!top) continue;

                    NSString* s = top.string;
                    if (s.length == 0) continue;

                    QString line = QString::fromNSString(s);
                    lines << line;
                    result.texts.push_back(line.toStdString());
                    result.scores.push_back(top.confidence);
                    scoreSum += top.confidence;
                    ++scoreCnt;

                    // 行级 boundingBox -> 像素四角点（左上原点）
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

#endif