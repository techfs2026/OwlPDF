#include "ocrstatusindicator.h"
#include <QPainter>
#include <QMouseEvent>

OCRStatusIndicator::OCRStatusIndicator(QWidget* parent)
    : QWidget(parent)
    , m_state(OCREngineState::Uninitialized)
    , m_engineRunning(false)
    , m_hovered(false)
    , m_pressed(false)
{
    setMinimumSize(90, 24);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    setToolTip(getTooltipText());
}

void OCRStatusIndicator::setState(OCREngineState state)
{
    if (m_state != state) {
        m_state = state;
        setToolTip(getTooltipText());
        updateGeometry();
        update();
    }
}

void OCRStatusIndicator::setEngineRunning(bool running)
{
    if (m_engineRunning != running) {
        m_engineRunning = running;
        setToolTip(getTooltipText());
        updateGeometry();
        update();
    }
}

QString OCRStatusIndicator::getStatusText() const
{
    if (!m_engineRunning) {
        return tr("Start OCR");
    }

    switch (m_state) {
    case OCREngineState::Uninitialized:
        return tr("Uninitialized");
    case OCREngineState::Loading:
        return tr("Loading...");
    case OCREngineState::Ready:
        return tr("OCR Ready");
    case OCREngineState::Error:
        return tr("Init Failed");
    default:
        return tr("Unknown State");
    }
}

QString OCRStatusIndicator::getTooltipText() const
{
    if (!m_engineRunning) {
        return tr("Click to start OCR engine\n"
                  "Once started, OCR lookup will be available in toolbar");
    }

    switch (m_state) {
    case OCREngineState::Loading:
        return tr("OCR engine loading...\n"
                  "Please wait, OCR lookup will be available after loading\n"
                  "Double-click to stop engine");
    case OCREngineState::Ready:
        return tr("OCR engine ready ✓\n"
                  "OCR lookup available in toolbar\n"
                  "Double-click to stop engine");
    case OCREngineState::Error:
        return tr("OCR engine initialization failed\n"
                  "Please check model files and configuration\n"
                  "Double-click to restart");
    case OCREngineState::Uninitialized:
        return tr("OCR engine not initialized\n"
                  "Click to start engine");
    default:
        return tr("OCR engine state unknown");
    }
}

QColor OCRStatusIndicator::getLightColor() const
{
    if (!m_engineRunning) {
        return QColor(160, 160, 160);
    }

    switch (m_state) {
    case OCREngineState::Uninitialized:
        return QColor(160, 160, 160);
    case OCREngineState::Loading:
        return QColor(255, 193, 7);
    case OCREngineState::Ready:
        return QColor(76, 175, 80);
    case OCREngineState::Error:
        return QColor(244, 67, 54);
    default:
        return QColor(160, 160, 160);
    }
}

QSize OCRStatusIndicator::sizeHint() const
{
    QString text = getStatusText();
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text);

    int totalWidth = 8 + 14 + 6 + textWidth + 10;

    totalWidth = qMax(totalWidth, 90);

    return QSize(totalWidth, 24);
}

void OCRStatusIndicator::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor;
    if (m_pressed) {
        bgColor = QColor(220, 220, 220);
    } else if (m_hovered) {
        if (m_engineRunning && m_state == OCREngineState::Ready) {
            bgColor = QColor(232, 245, 233);
        } else {
            bgColor = QColor(235, 235, 235);
        }
    } else {
        if (m_engineRunning && m_state == OCREngineState::Ready) {
            bgColor = QColor(240, 248, 240);
        } else {
            bgColor = QColor(245, 245, 245);
        }
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    if (m_hovered || m_pressed) {
        painter.setPen(QPen(QColor(200, 200, 200), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
    }

    int indicatorSize = 14;
    int indicatorX = 8;
    int indicatorY = (height() - indicatorSize) / 2;

    QColor lightColor = getLightColor();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 30));
    painter.drawEllipse(indicatorX + 1, indicatorY + 1, indicatorSize, indicatorSize);

    QRadialGradient gradient(indicatorX + indicatorSize/2, indicatorY + indicatorSize/2,
                             indicatorSize/2);
    gradient.setColorAt(0, lightColor.lighter(130));
    gradient.setColorAt(0.7, lightColor);
    gradient.setColorAt(1, lightColor.darker(110));
    painter.setBrush(gradient);
    painter.drawEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);

    painter.setBrush(QColor(255, 255, 255, 120));
    painter.drawEllipse(indicatorX + 3, indicatorY + 3, indicatorSize/3, indicatorSize/3);

    if (m_engineRunning && m_state == OCREngineState::Loading) {
        painter.setPen(QPen(lightColor.darker(120), 2));
        painter.drawArc(indicatorX + 1, indicatorY + 1,
                        indicatorSize - 2, indicatorSize - 2,
                        0, 270 * 16);
    }

    QString statusText = getStatusText();

    QColor textColor;
    if (!m_engineRunning) {
        textColor = QColor(100, 100, 100);
    } else if (m_state == OCREngineState::Ready) {
        textColor = QColor(46, 125, 50);
    } else if (m_state == OCREngineState::Error) {
        textColor = QColor(198, 40, 40);
    } else {
        textColor = QColor(70, 70, 70);
    }

    painter.setPen(textColor);
    QFont font = painter.font();
    font.setPointSize(9);

    if (m_engineRunning && m_state == OCREngineState::Ready) {
        font.setBold(true);
    }

    painter.setFont(font);

    QRect textRect(indicatorX + indicatorSize + 6, 0,
                   width() - indicatorX - indicatorSize - 10, height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, statusText);
}

void OCRStatusIndicator::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void OCRStatusIndicator::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;

        if (rect().contains(event->pos())) {
            if (!m_engineRunning || m_state == OCREngineState::Error) {
                emit engineStartRequested();
            }
            emit clicked();
        }

        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void OCRStatusIndicator::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_engineRunning) {
            emit engineStopRequested();
        }

        emit doubleClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void OCRStatusIndicator::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hovered = true;
    update();
}

void OCRStatusIndicator::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hovered = false;
    m_pressed = false;
    update();
}
