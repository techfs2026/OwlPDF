#ifndef VISIONOCRENGINE_H
#define VISIONOCRENGINE_H


#include "iocrengine.h"

/**
 * @brief 基于 macOS Vision (VNRecognizeTextRequest) 的 OCR 引擎。
 *
 * 仅在 APPLE 平台编译。实现位于 visionocrengine.mm。
 * 不需要任何模型文件、不依赖 onnxruntime / RapidOCR。
 *
 * 坐标转换：Vision 返回归一化(0~1)、左下原点坐标；
 *           本实现内部还原为像素并翻转 Y 轴，
 *           填入 OCRResult.boxes，与 RapidOCR 引擎格式一致。
 */
class VisionOcrEngine : public IOCREngine
{
    Q_OBJECT
public:
    explicit VisionOcrEngine(QObject* parent = nullptr);
    ~VisionOcrEngine() override;

    bool initializeSync(const QString& modelDir) override;
    bool initializeAsync(const QString& modelDir) override;

    OCRResult recognize(const QImage& image) override;
    OCRResult recognizeDetailed(const QImage& image) override;

    OCREngineState state() const override { return m_state; }
    bool isReady() const override { return m_state == OCREngineState::Ready; }
    QString lastError() const override { return m_lastError; }

private:
    void setState(OCREngineState s);
    // 真正的识别实现（在 .mm 中调用 Vision）
    OCRResult runVision(const QImage& image, bool detailed);

    OCREngineState m_state = OCREngineState::Uninitialized;
    QString m_lastError;
};


#endif // VISIONOCRENGINE_H