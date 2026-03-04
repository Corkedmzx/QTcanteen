#ifndef FACEENROLLDIALOG_H
#define FACEENROLLDIALOG_H

#include <QDialog>
#include <QCamera>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QImageCapture>
#include <QMediaCaptureSession>
#include <QMessageBox>
#include <QComboBox>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#ifdef SEETAFACE_AVAILABLE
#include "seetafacefacerecognizer.h"
#else
#include "opencvfacerecognizer.h"
#endif

class FaceEnrollDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FaceEnrollDialog(const QString &username, QWidget *parent = nullptr);
    ~FaceEnrollDialog();

signals:
    void faceEnrolled(const QString &username, const QString &faceImagePath);

private slots:
    void onCaptureClicked();
    void onSaveClicked();
    void onCancelClicked();
    void onImageCaptured(int id, const QImage &image);
    void onCameraError(QCamera::Error error);

private:
    void setupUI();
    void startCamera();
    void stopCamera();
    bool saveFaceImage(const QImage &image);

    QString m_username;
    QCamera *m_camera;
    QLabel *m_videoLabel;  // 用于显示视频和检测框的标签
    QImageCapture *m_imageCapture;
    QMediaCaptureSession *m_captureSession;
    QComboBox *m_cameraCombo;
    QPushButton *m_captureBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_previewLabel;
    QImage m_capturedImage;
    bool m_hasCaptured;
    QTimer *m_faceDetectionTimer;  // 用于定时检测人脸
    QRect m_currentFaceRect;  // 当前检测到的人脸区域（图像坐标）
    QVideoSink *m_videoSink;  // 用于获取视频帧进行实时检测
    bool m_trackerInitialized;  // Tracker 是否已初始化
    int m_trackerFailCount;  // Tracker 连续失败次数
    bool m_processingFrame;  // 是否正在处理帧（避免重叠处理）
    QSize m_cachedLabelSize;  // 缓存的标签尺寸
#ifdef SEETAFACE_AVAILABLE
    SeetaFaceRecognizer *m_faceRecognizer;
#else
    OpenCVFaceRecognizer *m_faceRecognizer;
#endif
private slots:
    void onFaceDetectionTimer();
};

#endif // FACEENROLLDIALOG_H
