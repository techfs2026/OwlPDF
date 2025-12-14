#include "ocrfloatingwidget.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QMouseEvent>

OCRFloatingWidget::OCRFloatingWidget(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setupUI();
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    qApp->installEventFilter(this);
}

void OCRFloatingWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        "QLabel {"
        "  border: 1px solid #d0d0d0;"
        "  border-radius: 4px;"
        "  background: #fafafa;"
        "  padding: 4px;"
        "}"
        );
    m_imageLabel->setMaximumSize(300, 200);
    m_imageLabel->setScaledContents(false);
    mainLayout->addWidget(m_imageLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "  color: #666666;"
        "  font-size: 12px;"
        "  font-style: italic;"
        "  padding: 4px;"
        "}"
        );
    m_statusLabel->hide();
    mainLayout->addWidget(m_statusLabel);

    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setMaximumWidth(300);
    m_textLabel->setStyleSheet(
        "QLabel {"
        "  color: #333333;"
        "  font-size: 14px;"
        "  padding: 4px;"
        "}"
        );
    mainLayout->addWidget(m_textLabel);

    m_confidenceLabel = new QLabel(this);
    m_confidenceLabel->setStyleSheet(
        "QLabel {"
        "  color: #666666;"
        "  font-size: 11px;"
        "}"
        );
    mainLayout->addWidget(m_confidenceLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_lookupButton = new QPushButton(tr("Lookup"), this);
    m_lookupButton->setCursor(Qt::PointingHandCursor);
    m_lookupButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #0078d4;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #005a9e;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #004578;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #cccccc;"
        "  color: #888888;"
        "}"
        );
    connect(m_lookupButton, &QPushButton::clicked,
            this, [this]() {
                emit lookupRequested(m_currentText);
            });
    buttonLayout->addWidget(m_lookupButton);

    m_closeButton = new QPushButton(tr("Close"), this);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #f3f3f3;"
        "  color: #333333;"
        "  border: 1px solid #d0d0d0;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #e0e0e0;"
        "}"
        );
    connect(m_closeButton, &QPushButton::clicked,
            this, &OCRFloatingWidget::hideFloating);
    buttonLayout->addWidget(m_closeButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    m_isRecognizing = false;
}

void OCRFloatingWidget::showResult(const QString& text, float confidence, const QRect& regionRect, const QImage& sourceImage)
{
    m_currentText = text;
    m_isRecognizing = false;

    if (!sourceImage.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(sourceImage);
        QSize labelSize = m_imageLabel->maximumSize();
        if (pixmap.width() > labelSize.width() || pixmap.height() > labelSize.height()) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        m_imageLabel->setPixmap(pixmap);
        m_imageLabel->show();
    } else {
        m_imageLabel->hide();
    }

    m_statusLabel->hide();

    m_textLabel->setText(text);
    m_textLabel->setStyleSheet(
        "QLabel {"
        "  color: #333333;"
        "  font-size: 14px;"
        "  padding: 4px;"
        "}"
        );
    m_textLabel->show();

    m_confidenceLabel->setText(tr("Confidence: %1%").arg(qRound(confidence * 100)));
    m_confidenceLabel->show();

    m_lookupButton->setEnabled(!text.isEmpty());

    adjustSize();
    positionWidget(regionRect);
    show();
    raise();
    activateWindow();
}

void OCRFloatingWidget::hideFloating()
{
    hide();
    m_currentText.clear();
    m_imageLabel->clear();
    m_imageLabel->hide();
    m_statusLabel->hide();
    m_isRecognizing = false;
}

void OCRFloatingWidget::positionWidget(const QRect& regionRect)
{
    int x = regionRect.x();
    int y = regionRect.bottom() + 10;

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();

        if (x + width() > screenGeometry.right()) {
            x = screenGeometry.right() - width();
        }
        if (x < screenGeometry.left()) {
            x = screenGeometry.left();
        }

        if (y + height() > screenGeometry.bottom()) {
            y = regionRect.top() - height() - 10;
        }

        if (y < screenGeometry.top()) {
            y = screenGeometry.top();
        }
    }

    move(x, y);
}

void OCRFloatingWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawPath(path.translated(2, 2));

    painter.setBrush(QColor(255, 255, 255, 250));
    painter.drawPath(path);


    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    QWidget::paintEvent(event);
}

bool OCRFloatingWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!geometry().contains(mouseEvent->globalPosition().toPoint())) {
            hideFloating();
        }
    }

    return QWidget::eventFilter(obj, event);
}

void OCRFloatingWidget::showRecognizing(const QImage& sourceImage, const QRect& regionRect)
{
    m_currentText.clear();
    m_isRecognizing = true;

    if (!sourceImage.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(sourceImage);
        QSize labelSize = m_imageLabel->maximumSize();
        if (pixmap.width() > labelSize.width() || pixmap.height() > labelSize.height()) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        m_imageLabel->setPixmap(pixmap);
        m_imageLabel->show();
    }

    m_statusLabel->setText(tr("🔍 Recognizing..."));
    m_statusLabel->show();

    m_textLabel->hide();
    m_confidenceLabel->hide();

    m_lookupButton->setEnabled(false);

    adjustSize();
    positionWidget(regionRect);
    show();
    raise();
    activateWindow();
}

void OCRFloatingWidget::updateResult(const QString& text, float confidence)
{
    if (!m_isRecognizing) {
        return;
    }

    m_isRecognizing = false;
    m_currentText = text;

    m_statusLabel->hide();

    if (text.isEmpty()) {
        m_textLabel->setText(tr("No text recognized"));
        m_textLabel->setStyleSheet(
            "QLabel {"
            "  color: #999999;"
            "  font-size: 14px;"
            "  font-style: italic;"
            "  padding: 4px;"
            "}"
            );
        m_confidenceLabel->hide();
        m_lookupButton->setEnabled(false);
    } else {
        m_textLabel->setText(text);
        m_textLabel->setStyleSheet(
            "QLabel {"
            "  color: #333333;"
            "  font-size: 14px;"
            "  padding: 4px;"
            "}"
            );
        m_confidenceLabel->setText(tr("Confidence: %1%").arg(qRound(confidence * 100)));
        m_confidenceLabel->show();
        m_lookupButton->setEnabled(true);
    }

    m_textLabel->show();

    adjustSize();
}
