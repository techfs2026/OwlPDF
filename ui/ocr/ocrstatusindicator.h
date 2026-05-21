#ifndef OCRSTATUSINDICATOR_H
#define OCRSTATUSINDICATOR_H

#include <QWidget>
#include "iocrengine.h"

class OCRStatusIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit OCRStatusIndicator(QWidget* parent = nullptr);

    void setState(OCREngineState state);
    OCREngineState state() const { return m_state; }

    void setEngineRunning(bool running);
    bool isEngineRunning() const { return m_engineRunning; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

signals:
    void doubleClicked();
    void clicked();
    void engineStartRequested();
    void engineStopRequested();

private:
    QString getStatusText() const;
    QString getTooltipText() const;
    QColor getLightColor() const;

    OCREngineState m_state;

    bool m_engineRunning;
    bool m_hovered;
    bool m_pressed;
};

#endif // OCRSTATUSINDICATOR_H
