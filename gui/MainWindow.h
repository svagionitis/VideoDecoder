/**
 * @file MainWindow.h
 * @brief Header for the Qt GUI Main Window interface.
 */

#pragma once

#include "VideoWidget.h"
#include "VideoWorker.h"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>

/**
 * @class MainWindow
 * @brief The main GUI layout containing video rendering workspace and control panel.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /**
     * @brief Constructor.
     * @param parent Optional Qt parent.
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Destructor. Closes threads.
     */
    ~MainWindow() override;

private slots:
    /**
     * @brief Triggered when the user clicks 'Browse'. Opens QFileDialog.
     */
    void onBrowseClicked();

    /**
     * @brief Triggered when the user clicks 'Play'.
     */
    void onPlayClicked();

    /**
     * @brief Triggered when the user clicks 'Pause'.
     */
    void onPauseClicked();

    /**
     * @brief Triggered when the user clicks 'Stop'.
     */
    void onStopClicked();

    /**
     * @brief Slot receiving frames decoded by the background worker thread.
     */
    void onFrameDecoded(const QByteArray& rgbData, int width, int height, double pts);

    /**
     * @brief Slot triggered when the background thread completes playback.
     */
    void onFinishedDecoding();

    /**
     * @brief Slot receiving error messages from the background thread.
     */
    void onErrorOccurred(const QString& message);

private:
    QLineEdit* m_filePathEdit = nullptr; ///< Input file path editor
    QPushButton* m_browseButton = nullptr; ///< File browse button
    QComboBox* m_backendCombo = nullptr; ///< ComboBox to select FFmpeg vs GStreamer

    QPushButton* m_playButton = nullptr; ///< Play button
    QPushButton* m_pauseButton = nullptr; ///< Pause button
    QPushButton* m_stopButton = nullptr; ///< Stop button

    QLabel* m_statusLabel = nullptr; ///< Status label showing frames and PTS
    VideoWidget* m_videoWidget = nullptr; ///< Custom rendering area

    VideoWorker* m_worker = nullptr; ///< Background decoder thread worker
    int m_frameCount = 0; ///< Track count of frames processed
};
