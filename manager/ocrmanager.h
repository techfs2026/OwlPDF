#ifndef OCRMANAGER_H
#define OCRMANAGER_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QTimer>
#include <QRect>
#include <memory>
#include "ocrengine.h"

class OCRManager : public QObject
{
    Q_OBJECT

public:
    static OCRManager& instance();

    bool initialize(const QString& modelDir);

    void shutdown();

    bool isEngineRunning() const;

    bool isReady() const;

    OCREngineState engineState() const;

    void setOCRHoverEnabled(bool enabled);

    bool isOCRHoverEnabled() const { return m_ocrHoverEnabled; }

    void requestOCR(const QImage& image, const QRect& regionRect, const QPoint& lastHoverPos);

    void cancelPending();

    void setDebounceDelay(int delay);

    QString lastError() const;

signals:
    void ocrCompleted(const OCRResult& result, const QRect& regionRect, const QPoint& lastHoverPos);

    void ocrFailed(const QString& error);

    void engineStateChanged(OCREngineState state);

    void ocrHoverEnabledChanged(bool enabled);

private:
    OCRManager();
    ~OCRManager();
    OCRManager(const OCRManager&) = delete;
    OCRManager& operator=(const OCRManager&) = delete;

private slots:
    void performOCR();
    void onEngineStateChanged(OCREngineState state);

private:
    std::unique_ptr<OCREngine> m_engine;
    QTimer m_debounceTimer;

    struct PendingRequest {
        bool valid;
        QImage image;
        QRect regionRect;
        QPoint lastHoverPos;

        PendingRequest() : valid(false) {}
    } m_pending;

    int m_debounceDelay;
    bool m_ocrHoverEnabled;
};

#endif // OCRMANAGER_H
