#ifndef FACELOGINDIALOG_H
#define FACELOGINDIALOG_H

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
#include <QImage>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#ifdef SEETAFACE_AVAILABLE
#include "seetafacefacerecognizer.h"
#elif defined(OPENCV_AVAILABLE)
#include "opencvfacerecognizer.h"
#else
// 如果没有可用的库，定义空的前向声明
class FaceRecognizerBase {
public:
    virtual ~FaceRecognizerBase() {}
    virtual bool initialize() { return false; }
    virtual bool train(const QString &, const QImage &) { return false; }
    virtual QString recognize(const QImage &, double &) { return QString(); }
    virtual bool detectFace(const QImage &, QRect &) { return false; }
};
typedef FaceRecognizerBase OpenCVFaceRecognizer;
#endif

class FaceLoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FaceLoginDialog(QWidget *parent = nullptr);
    ~FaceLoginDialog();

signals:
    void faceLoginSuccess(const QString &username);

private slots:
    void onRecognizeClicked();
    void onImageCaptured(int id, const QImage &image);
    void onCameraError(QCamera::Error error);

private:
    void setupUI();
    void startCamera();
    void stopCamera();
    QString recognizeFace(const QImage &image);

    QCamera *m_camera;
    QLabel *m_videoLabel;  // 用于显示视频和检测框的标签
    QImageCapture *m_imageCapture;
    QMediaCaptureSession *m_captureSession;
    QComboBox *m_cameraCombo;
    QPushButton *m_recognizeBtn;
    QPushButton *m_cancelBtn;
    QTimer *m_faceDetectionTimer;  // 用于定时检测人脸
    QRect m_currentFaceRect;  // 当前检测到的人脸区域（图像坐标）
    QVideoSink *m_videoSink;  // 用于获取视频帧进行实时检测
    bool m_trackerInitialized;  // Tracker 是否已初始化
    int m_trackerFailCount;  // Tracker 连续失败次数
    bool m_processingFrame;  // 是否正在处理帧（避免重叠处理）
    QSize m_cachedLabelSize;  // 缓存的标签尺寸
    QPixmap m_cachedPixmap;  // 缓存的视频帧（用于流畅显示）
    QRect m_lastDrawnFaceRect;  // 上次绘制的人脸框位置
#ifdef SEETAFACE_AVAILABLE
    SeetaFaceRecognizer *m_faceRecognizer;
#elif defined(OPENCV_AVAILABLE)
    OpenCVFaceRecognizer *m_faceRecognizer;
#else
    FaceRecognizerBase *m_faceRecognizer;
#endif
private slots:
    void onVideoFrameChanged();  // 视频帧变化时快速更新显示
    void onFaceDetectionTimer();  // 定时检测人脸（较低频率）
};

#endif // FACELOGINDIALOG_H
