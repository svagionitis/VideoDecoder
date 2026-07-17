/**
 * @file VideoWidget.h
 * @brief Custom QWidget to render decoded video frames using QPainter.
 */

#pragma once

#include <QImage>
#include <QMutex>
#include <QWidget>

/**
 * @class VideoWidget
 * @brief Widget responsible for displaying the decoded frames.
 *
 * It converts raw RGB24 bytes to a QImage and paints it dynamically inside paintEvent.
 */
class VideoWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param parent Optional Qt parent.
     */
    explicit VideoWidget(QWidget* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~VideoWidget() override = default;

    /**
     * @brief Clears the current frame (renders black screen).
     */
    void clearFrame();

public slots:
    /**
     * @brief Receives new frame data and triggers a repaint.
     * @param rgbData The pixel buffer in RGB24 layout.
     * @param width Width in pixels.
     * @param height Height in pixels.
     */
    void updateFrame(const QByteArray& rgbData, int width, int height);

protected:
    /**
     * @brief Draws the cached image onto the widget.
     * @param event Paint event.
     */
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image; ///< The cached QImage to paint
    QMutex m_imageMutex; ///< Mutex protecting image reads/writes
};
