// ocrengine.h
#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QtGlobal>
#ifndef Q_OS_MACOS

#include <QObject>
#include <QImage>
#include <QString>
#include <memory>
#include "iocrengine.h"
#include "rapidocr-cpp/rapidocr.h"


class RapidOcrEngine : public IOCREngine
{
    Q_OBJECT

public:
    explicit RapidOcrEngine(QObject* parent = nullptr);
    ~RapidOcrEngine() override;

    // 初始化
    bool initializeSync(const QString& modelDir) override;
    bool initializeAsync(const QString& modelDir) override;

    // 识别
    OCRResult recognize(const QImage& image) override;
    OCRResult recognizeDetailed(const QImage& image) override;  // 返回详细结果

    // 参数设置
    void setTextScore(float score);
    void setUseDet(bool use);
    void setUseCls(bool use);
    void setUseRec(bool use);
    void setReturnWordBox(bool enable);

    // 状态查询
    OCREngineState state()  const override { return m_state; }
    QString lastError() const override{ return m_lastError; }
    bool isReady() const override{ return m_state == OCREngineState::Ready; }

private:
    bool initializeInternal(const QString& modelDir);
    void setState(OCREngineState state);
    void setError(const QString& error);

    // 将RapidOCROutput转换为OCRResult
    OCRResult convertToOCRResult(const RapidOCR::RapidOCROutput& output);

private:
    std::unique_ptr<RapidOCR::RapidOCR> m_rapidOCR;
    OCREngineState m_state;
    QString m_lastError;
    QString m_modelDir;

    // 配置参数
    float m_textScore = 0.5f;
    bool m_useDet = true;
    bool m_useCls = true;
    bool m_useRec = true;
    bool m_returnWordBox = false;

    bool m_isProcessing = false;
};

#endif
#endif // OCRENGINE_H
