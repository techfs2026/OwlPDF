// iocrengine.h
#ifndef IOCRENGINE_H
#define IOCRENGINE_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>
#include <QRect>
#include <vector>
#include <string>
#include <opencv2/core/types.hpp>   // cv::Point2f

// ============================================================
// 引擎状态
// ============================================================
enum class OCREngineState {
    Uninitialized,
    Loading,
    Ready,
    Processing,
    Error
};

// ============================================================
// OCR 识别结果（整段纯文本场景：复制、全文搜索等）
//   boxes 约定：相对输入 QImage 的像素坐标，原点左上，每 box 4 角点。
// ============================================================
struct OCRResult {
    bool success = false;
    QString text;
    float confidence = 0.0f;
    QString error;

    std::vector<std::vector<cv::Point2f>> boxes;  // 行级文本框
    std::vector<std::string> texts;               // 各行文本
    std::vector<float> scores;                    // 各行置信度
    float elapsedTime = 0.0f;
};

// ============================================================
// 带位置的词（取词场景）
//   —— 从 chinesetokenizer.h 上移为公共契约。
//   estimatedRect：词在输入图像上的像素矩形。
//     · Windows(RapidOCR)：按字符比例估算（固有限制）
//     · macOS(Vision)：Vision 子串真实坐标（精确）
// ============================================================
struct TokenWithPosition {
    QString word;
    int startIndex = -1;
    int endIndex   = -1;
    QRect estimatedRect;
    int lineIndex  = -1;
    float confidence = 0.0f;

    bool isValid() const { return !word.isEmpty(); }
};

// ============================================================
// 引擎接口
// ============================================================
class IOCREngine : public QObject
{
    Q_OBJECT
public:
    explicit IOCREngine(QObject* parent = nullptr) : QObject(parent) {}
    ~IOCREngine() override = default;

    virtual bool initializeSync(const QString& modelDir) = 0;
    virtual bool initializeAsync(const QString& modelDir) = 0;

    // 整段纯文本（复制、全文搜索）
    virtual OCRResult recognize(const QImage& image) = 0;
    virtual OCRResult recognizeDetailed(const QImage& image) = 0;

    // 带位置的词列表（取词）——各引擎用自己最优方式产出
    virtual QVector<TokenWithPosition> recognizeTokens(const QImage& image) = 0;

    virtual OCREngineState state() const = 0;
    virtual bool isReady() const = 0;
    virtual QString lastError() const = 0;

signals:
    void initialized(bool success, const QString& error);
    void stateChanged(OCREngineState state);
    void recognitionCompleted(const OCRResult& result);
};

IOCREngine* createOCREngine(QObject* parent = nullptr);

#endif // IOCRENGINE_H