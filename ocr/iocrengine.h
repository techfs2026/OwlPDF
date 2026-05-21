// iocrengine.h
#ifndef IOCRENGINE_H
#define IOCRENGINE_H

#include <QObject>
#include <QImage>
#include <QString>
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
// OCR 识别结果（引擎无关的通用契约）
//
// 重要：boxes 的坐标约定为 —— 相对输入 QImage 的像素坐标，
//      原点在左上角，每个 box 为 4 个角点 (cv::Point2f)。
//      任何引擎实现都必须把自己的结果归一化到这个约定，
//      上层 ChineseTokenizer 等才能保持不变。
// ============================================================
struct OCRResult {
    bool success = false;
    QString text;
    float confidence = 0.0f;
    QString error;

    std::vector<std::vector<cv::Point2f>> boxes;  // 文本框（像素坐标，左上原点）
    std::vector<std::string> texts;               // 各区域文本
    std::vector<float> scores;                    // 各区域置信度
    float elapsedTime = 0.0f;                      // 耗时(秒)
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

    // 初始化（modelDir 对 Vision 引擎可忽略）
    virtual bool initializeSync(const QString& modelDir) = 0;
    virtual bool initializeAsync(const QString& modelDir) = 0;

    // 识别
    virtual OCRResult recognize(const QImage& image) = 0;
    virtual OCRResult recognizeDetailed(const QImage& image) = 0;

    // 状态
    virtual OCREngineState state() const = 0;
    virtual bool isReady() const = 0;
    virtual QString lastError() const = 0;

signals:
    void initialized(bool success, const QString& error);
    void stateChanged(OCREngineState state);
    void recognitionCompleted(const OCRResult& result);
};

// ============================================================
// 工厂：按平台返回合适的引擎实现
// ============================================================
IOCREngine* createOCREngine(QObject* parent = nullptr);

#endif // IOCRENGINE_H