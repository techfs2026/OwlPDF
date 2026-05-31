#include "ocrmanager.h"
#include <QDebug>
#include <QtConcurrent>

OCRManager::OCRManager()
    : m_debounceDelay(300)
    , m_ocrHoverEnabled(false)
{
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer, &QTimer::timeout,
            this, &OCRManager::performOCR);
}

OCRManager::~OCRManager()
{
    cancelPending();
}

OCRManager& OCRManager::instance()
{
    static OCRManager instance;
    return instance;
}

bool OCRManager::initialize(const QString& modelDir)
{
    if (m_engine) {
        qWarning() << "OCRManager: Already initialized";
        return false;
    }

    qInfo() << "OCRManager: Initializing with model dir:" << modelDir;

    m_engine.reset(createOCREngine());

    connect(m_engine.get(), &IOCREngine::stateChanged,
            this, &OCRManager::onEngineStateChanged);

    connect(m_engine.get(), &IOCREngine::initialized,
            this, [](bool success, const QString& error) {
                if (success) {
                    qInfo() << "OCRManager: Engine initialized successfully";
                } else {
                    qWarning() << "OCRManager: Engine initialization failed:" << error;
                }
            });

    return m_engine->initializeAsync(modelDir);
}

void OCRManager::shutdown()
{
    qInfo() << "OCRManager: Shutting down...";

    if (m_ocrHoverEnabled) {
        setOCRHoverEnabled(false);
    }

    cancelPending();

    if (m_engine) {
        disconnect(m_engine.get(), nullptr, this, nullptr);

        m_engine.reset();
        m_engine = nullptr;

        qInfo() << "OCRManager: Engine released";
    }

    emit engineStateChanged(OCREngineState::Uninitialized);

    qInfo() << "OCRManager: Shutdown complete";
}

bool OCRManager::isEngineRunning() const
{
    return m_engine != nullptr;
}

bool OCRManager::isReady() const
{
    return m_engine && m_engine->state() == OCREngineState::Ready;
}

OCREngineState OCRManager::engineState() const
{
    return m_engine ? m_engine->state() : OCREngineState::Uninitialized;
}

void OCRManager::setOCRHoverEnabled(bool enabled)
{
    if (m_ocrHoverEnabled == enabled) {
        return;
    }

    if (enabled && !isReady()) {
        qWarning() << "Cannot enable OCR hover: Engine not ready";
        return;
    }

    m_ocrHoverEnabled = enabled;

    if (!enabled) {
        cancelPending();
    }

    emit ocrHoverEnabledChanged(enabled);
    qInfo() << "OCR hover enabled changed to:" << enabled;
}

void OCRManager::requestOCR(const QImage& image, const QRect& regionRect, const QPoint& lastHoverPos)
{
    if (!m_ocrHoverEnabled) {
        qDebug() << "OCR hover is disabled, ignoring request";
        return;
    }

    if (!m_engine) {
        emit ocrFailed(tr("OCR engine not initialized"));
        return;
    }

    if (m_engine->state() != OCREngineState::Ready) {
        emit ocrFailed(tr("OCR engine not ready"));
        return;
    }

    if (image.isNull()) {
        emit ocrFailed(tr("Invalid image"));
        return;
    }

    m_debounceTimer.stop();

    m_pending.valid = true;
    m_pending.image = image;
    m_pending.regionRect = regionRect;
    m_pending.lastHoverPos = lastHoverPos;

    m_debounceTimer.start(m_debounceDelay);
}

void OCRManager::cancelPending()
{
    m_debounceTimer.stop();
    m_pending.valid = false;
}

void OCRManager::performOCR()
{
    if (!m_pending.valid) {
        return;
    }

    QImage image = m_pending.image;
    QRect regionRect = m_pending.regionRect;
    QPoint lastHoverPos = m_pending.lastHoverPos;
    m_pending.valid = false;

    QFuture<QVector<TokenWithPosition>> future = QtConcurrent::run([this, image]() {
        return m_engine->recognizeTokens(image);
    });

    QFutureWatcher<QVector<TokenWithPosition>>* watcher =
        new QFutureWatcher<QVector<TokenWithPosition>>(this);

    connect(watcher, &QFutureWatcher<QVector<TokenWithPosition>>::finished,
            this, [this, watcher, regionRect, lastHoverPos]() {
                QVector<TokenWithPosition> tokens = watcher->result();

                if (!tokens.isEmpty()) {
                    emit ocrCompleted(tokens, regionRect, lastHoverPos);
                } else {
                    emit ocrFailed(tr("No text recognized"));
                }

                watcher->deleteLater();
            });

    watcher->setFuture(future);
}

void OCRManager::onEngineStateChanged(OCREngineState state)
{
    emit engineStateChanged(state);
}

void OCRManager::setDebounceDelay(int delay)
{
    if (delay >= 0 && delay <= 2000) {
        m_debounceDelay = delay;
    }
}

QString OCRManager::lastError() const
{
    return m_engine ? m_engine->lastError() : tr("Engine not initialized");
}
