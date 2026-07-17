/**
 * @file VideoWidget.cpp
 * @brief Implementation of the VideoWidget class.
 */

#include "VideoWidget.h"
#include <QMutexLocker>
#include <QPaintEvent>
#include <QPainter>

VideoWidget::VideoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Set widget background role to prevent background flashing
    setAttribute(Qt::WA_OpaquePaintEvent);
    clearFrame();
}

void VideoWidget::clearFrame()
{
    QMutexLocker locker(&m_imageMutex);
    m_image = QImage();
    update(); // Schedule repaint
}

void VideoWidget::updateFrame(const QByteArray& rgbData, int width, int height)
{
    {
        QMutexLocker locker(&m_imageMutex);

        // Construct QImage pointing to raw data, then call .copy() to clone it safely
        // away from the QByteArray scope. Use Format_RGB888 for packed RGB24.
        m_image
            = QImage(reinterpret_cast<const uchar*>(rgbData.constData()), width, height, QImage::Format_RGB888).copy();
    }

    update(); // Triggers paintEvent in main thread
}

void VideoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // Render a clean black background first
    painter.fillRect(rect(), Qt::black);

    QMutexLocker locker(&m_imageMutex);
    if (!m_image.isNull()) {
        // Scale image to fill widget while maintaining original video aspect ratio
        QImage scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Center the scaled image in the widget workspace
        int xOffset = (width() - scaled.width()) / 2;
        int yOffset = (height() - scaled.height()) / 2;
        painter.drawImage(xOffset, yOffset, scaled);
    }
}
