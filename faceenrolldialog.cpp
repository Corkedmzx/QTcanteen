#include "faceenrolldialog.h"
#include "jsonhelper.h"
#include <QDir>
#include <QDebug>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QPainter>
#include <QPixmap>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QTimer>

FaceEnrollDialog::FaceEnrollDialog(const QString &username, QWidget *parent)
    : QDialog(parent), m_username(username), m_camera(nullptr), m_videoLabel(nullptr), m_imageCapture(nullptr),
      m_captureSession(nullptr), m_hasCaptured(false), m_faceDetectionTimer(nullptr), m_videoSink(nullptr),
      m_trackerInitialized(false), m_trackerFailCount(0), m_processingFrame(false)
{
    setWindowTitle(QString("录入人脸 - %1").arg(username));
    setFixedSize(600, 600);  // 增加窗口高度，避免按钮与显示区域重叠
    setModal(true);

    // 初始化人脸识别器（延迟初始化，避免在构造函数中崩溃）
#ifdef SEETAFACE_AVAILABLE
    m_faceRecognizer = new SeetaFaceRecognizer();
    // 使用 QTimer 延迟初始化，避免在构造函数中立即初始化导致崩溃
    QTimer::singleShot(100, this, [this]() {
        // 尝试从环境变量获取 SeetaFace 模型路径，否则使用默认路径
        QString seetaFaceRoot = qEnvironmentVariable("SEETAFACE_ROOT", "E:/share/SeetaFace");
        QString modelPath = seetaFaceRoot + "/bin/model";
        
        // 如果环境变量路径不存在，尝试默认路径
        if (!QFile::exists(modelPath + "/face_detector.csta") && 
            !QFile::exists(modelPath + "/fd_2_00.dat")) {
            modelPath = "E:/share/SeetaFace/bin/model";
        }
        
        if (!m_faceRecognizer->initialize(modelPath)) {
            QMessageBox::warning(this, "警告", 
                QString("SeetaFace 人脸识别器初始化失败！\n请确保模型文件在以下目录之一：\n"
                       "1. %1\n"
                       "2. E:/share/SeetaFace/bin/model\n"
                       "3. 可执行文件目录下的 models 子目录").arg(modelPath));
        }
    });
#elif defined(OPENCV_AVAILABLE)
    m_faceRecognizer = new OpenCVFaceRecognizer();
    m_faceRecognizer->initialize(); // 加载已有模型
#else
    // 没有可用的库，显示错误并关闭对话框
    m_faceRecognizer = nullptr;
    QMessageBox::warning(this, "错误", 
        "人脸识别功能不可用！\n\n"
        "请安装 OpenCV 或 SeetaFace6Open 库以启用人脸识别功能。");
    reject();  // 关闭对话框
    return;
#endif

    setupUI();
    startCamera();
}

FaceEnrollDialog::~FaceEnrollDialog()
{
    stopCamera();
    // 清理人脸识别器
    if (m_faceRecognizer) {
        delete m_faceRecognizer;
        m_faceRecognizer = nullptr;
    }
}

void FaceEnrollDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel(QString("为账户 %1 录入人脸").arg(m_username), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 摄像头选择
    QHBoxLayout *cameraLayout = new QHBoxLayout();
    QLabel *cameraLabel = new QLabel("选择摄像头：", this);
    m_cameraCombo = new QComboBox(this);
    m_cameraCombo->setMinimumHeight(30);
    
    // 获取可用摄像头
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : cameras) {
        m_cameraCombo->addItem(cameraDevice.description(), QVariant::fromValue(cameraDevice));
    }
    
    if (m_cameraCombo->count() == 0) {
        QLabel *noCameraLabel = new QLabel("未检测到摄像头设备", this);
        noCameraLabel->setStyleSheet("color: red;");
        mainLayout->addWidget(noCameraLabel);
    }
    
    cameraLayout->addWidget(cameraLabel);
    cameraLayout->addWidget(m_cameraCombo);
    cameraLayout->addStretch();
    mainLayout->addLayout(cameraLayout);

    // 视频预览区域（使用 QLabel 显示视频和检测框）
    QWidget *videoContainer = new QWidget(this);
    videoContainer->setMinimumSize(480, 360);
    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    
    m_videoLabel = new QLabel(videoContainer);
    m_videoLabel->setMinimumSize(480, 360);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 5px; background-color: #000;");
    videoLayout->addWidget(m_videoLabel);

    // 预览标签（用于显示捕获的图像，覆盖在视频窗口上）
    m_previewLabel = new QLabel(videoContainer);
    m_previewLabel->setMinimumSize(480, 360);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 5px; background-color: #000;");
    m_previewLabel->setScaledContents(true);
    m_previewLabel->hide();
    videoLayout->addWidget(m_previewLabel);

    // 确保正确的 Z-order：视频标签 < 预览标签
    m_previewLabel->raise();   // 预览标签在最上层
    
    mainLayout->addWidget(videoContainer, 1);
    
    // 创建定时器用于实时人脸检测（较低频率，不影响视频流显示）
    m_faceDetectionTimer = new QTimer(this);
    m_faceDetectionTimer->setInterval(100);  // 检测间隔100ms，与视频流显示分离
    connect(m_faceDetectionTimer, &QTimer::timeout, this, &FaceEnrollDialog::onFaceDetectionTimer);

    // 按钮区域（添加间距，避免与显示区域重叠）
    mainLayout->addSpacing(10);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_captureBtn = new QPushButton("拍照", this);
    m_captureBtn->setFixedSize(100, 40);
    m_captureBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #0b7dda; }");
    connect(m_captureBtn, &QPushButton::clicked, this, &FaceEnrollDialog::onCaptureClicked);

    m_saveBtn = new QPushButton("保存", this);
    m_saveBtn->setFixedSize(100, 40);
    m_saveBtn->setEnabled(false);
    m_saveBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                            "QPushButton:hover { background-color: #45a049; }"
                            "QPushButton:disabled { background-color: #ccc; }");
    connect(m_saveBtn, &QPushButton::clicked, this, &FaceEnrollDialog::onSaveClicked);

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setFixedSize(100, 40);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FaceEnrollDialog::onCancelClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(m_captureBtn);
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);
}

void FaceEnrollDialog::startCamera()
{
    if (m_cameraCombo->count() == 0) {
        return;
    }

    stopCamera();

    QCameraDevice cameraDevice = m_cameraCombo->currentData().value<QCameraDevice>();
    m_camera = new QCamera(cameraDevice, this);
    
    m_imageCapture = new QImageCapture(this);
    m_captureSession = new QMediaCaptureSession(this);
    
    m_captureSession->setCamera(m_camera);
    m_captureSession->setImageCapture(m_imageCapture);
    
    // 创建 QVideoSink 用于获取视频帧
    m_videoSink = new QVideoSink(this);
    m_captureSession->setVideoOutput(m_videoSink);
    
    // 连接视频帧变化信号，用于快速更新视频流显示
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &FaceEnrollDialog::onVideoFrameChanged);

    connect(m_imageCapture, &QImageCapture::imageCaptured, this, &FaceEnrollDialog::onImageCaptured);
    connect(m_camera, &QCamera::errorOccurred, this, &FaceEnrollDialog::onCameraError);

    m_camera->start();
    
    // 启动实时人脸检测定时器
    if (m_faceDetectionTimer) {
        m_faceDetectionTimer->start();
    }
}

void FaceEnrollDialog::stopCamera()
{
    // 停止实时人脸检测
    if (m_faceDetectionTimer) {
        m_faceDetectionTimer->stop();
    }
    
    // 重置 Tracker
    m_trackerInitialized = false;
    m_trackerFailCount = 0;
#ifndef SEETAFACE_AVAILABLE
    if (m_faceRecognizer) {
        m_faceRecognizer->resetTracker();
    }
#endif
    
    if (m_camera) {
        m_camera->stop();
        m_camera->deleteLater();
        m_camera = nullptr;
    }
    if (m_imageCapture) {
        m_imageCapture->deleteLater();
        m_imageCapture = nullptr;
    }
    if (m_captureSession) {
        m_captureSession->deleteLater();
        m_captureSession = nullptr;
    }
    if (m_videoSink) {
        m_videoSink->deleteLater();
        m_videoSink = nullptr;
    }
}

void FaceEnrollDialog::onCaptureClicked()
{
    // 如果已经捕获过图像，点击"重新拍照"时需要重新启动摄像头
    if (m_hasCaptured) {
        // 隐藏预览标签，显示视频预览
        m_previewLabel->hide();
        m_videoLabel->show();
        
        // 重置状态
        m_hasCaptured = false;
        m_capturedImage = QImage();
        m_saveBtn->setEnabled(false);
        m_captureBtn->setText("拍照");
        
        // 重置 Tracker
        m_trackerInitialized = false;
        m_trackerFailCount = 0;
#ifndef SEETAFACE_AVAILABLE
        if (m_faceRecognizer) {
            m_faceRecognizer->resetTracker();
        }
#endif
        
        // 重新启动实时人脸检测
        if (m_faceDetectionTimer) {
            m_faceDetectionTimer->start();
        }
        
        // 重新启动摄像头
        startCamera();
        return;
    }
    
    // 首次拍照
    if (!m_imageCapture || !m_camera) {
        QMessageBox::warning(this, "错误", "摄像头未就绪，请稍候再试！");
        return;
    }

    m_imageCapture->capture();
}

void FaceEnrollDialog::onImageCaptured(int id, const QImage &image)
{
    Q_UNUSED(id);
    
    if (image.isNull()) {
        QMessageBox::warning(this, "错误", "图像捕获失败！");
        return;
    }

    m_capturedImage = image;
    m_hasCaptured = true;

    // 检测人脸并在图像上绘制人脸框（提高优先级，确保显示）
    QImage displayImage = image.copy();
    bool faceDetected = false;
    QRect faceRect;
    
    if (m_faceRecognizer) {
        faceDetected = m_faceRecognizer->detectFace(image, faceRect);
        if (faceDetected && !faceRect.isEmpty() && 
            faceRect.width() < image.width() && faceRect.height() < image.height()) {
            // 在图像上绘制人脸框（使用更粗的线条，确保可见）
            QPainter painter(&displayImage);
            painter.setRenderHint(QPainter::Antialiasing);
            
            // 绘制粗绿色边框
            QPen pen(QColor(0, 255, 0), 4);  // 4像素宽，更明显
            painter.setPen(pen);
            painter.drawRect(faceRect);
            
            // 绘制文字背景（提高可读性）
            QRect textRect(faceRect.x(), faceRect.y() - 25, 120, 20);
            painter.fillRect(textRect, QColor(0, 0, 0, 180));  // 半透明黑色背景
            
            // 添加文字说明
            painter.setPen(QPen(QColor(0, 255, 0), 2));
            painter.setFont(QFont("Arial", 12, QFont::Bold));
            painter.drawText(textRect, Qt::AlignCenter, "检测到人脸");
        }
    }

    // 显示捕获的图像（带人脸框，确保正确显示）
    if (m_videoLabel) {
        m_videoLabel->hide();
    }
    if (m_previewLabel) {
        m_previewLabel->setPixmap(QPixmap::fromImage(displayImage));
    m_previewLabel->show();
        m_previewLabel->raise();  // 确保预览标签在最上层
    }

    // 停止摄像头
    if (m_camera) {
        m_camera->stop();
    }

    m_saveBtn->setEnabled(true);
    m_captureBtn->setText("重新拍照");
    
    // 停止实时人脸检测
    if (m_faceDetectionTimer) {
        m_faceDetectionTimer->stop();
    }
    
    QMessageBox::information(this, "提示", "图像已捕获，请确认后点击保存！");
}

void FaceEnrollDialog::onVideoFrameChanged()
{
    // 快速更新视频流显示，不进行检测处理（保证流畅度）
    if (!m_videoSink || !m_videoLabel) {
        return;
    }
    
    // 如果已经捕获图像或预览标签显示，不更新视频流
    if (m_hasCaptured || (m_previewLabel && m_previewLabel->isVisible())) {
        return;
    }
    
    // 从 QVideoSink 获取当前视频帧
    QVideoFrame frame = m_videoSink->videoFrame();
    if (!frame.isValid()) {
        return;
    }
    
    // 将 QVideoFrame 转换为 QImage（快速转换，不进行格式优化）
    QImage image = frame.toImage();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return;
    }
    
    // 如果有缓存的带检测框的pixmap，使用它；否则使用原始视频帧
    QPixmap pixmap;
    if (!m_cachedPixmap.isNull() && !m_lastDrawnFaceRect.isEmpty()) {
        // 使用缓存的带检测框的图像
        pixmap = m_cachedPixmap;
    } else {
        // 使用原始视频帧
        pixmap = QPixmap::fromImage(image);
    }
    
    // 缓存标签尺寸，避免每次都获取
    QSize labelSize = m_videoLabel->size();
    if (labelSize != m_cachedLabelSize) {
        m_cachedLabelSize = labelSize;
        // 尺寸变化时，清除缓存
        m_cachedPixmap = QPixmap();
    }
    
    // 只在尺寸有效时进行缩放
    if (m_cachedLabelSize.width() > 0 && m_cachedLabelSize.height() > 0) {
        // 使用快速缩放算法，保证流畅度
        if (pixmap.size() != m_cachedLabelSize) {
            pixmap = pixmap.scaled(m_cachedLabelSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        }
    }
    
    // 更新显示（这是唯一会阻塞UI的地方，但已经优化到最快）
    m_videoLabel->setPixmap(pixmap);
}

void FaceEnrollDialog::onFaceDetectionTimer()
{
    // 如果已经捕获图像或预览标签显示，不进行实时检测
    if (m_hasCaptured || (m_previewLabel && m_previewLabel->isVisible())) {
        return;
    }
    
    // 如果正在处理上一帧，跳过当前帧（避免重叠处理导致卡顿）
    if (m_processingFrame) {
        return;
    }
    
    // 检查必要的组件
    if (!m_faceRecognizer || !m_videoLabel || !m_videoSink) {
        return;
    }
    
    // 从 QVideoSink 获取当前视频帧
    QVideoFrame frame = m_videoSink->videoFrame();
    if (!frame.isValid()) {
        return;
    }
    
    // 标记正在处理
    m_processingFrame = true;
    
    // 将 QVideoFrame 转换为 QImage
    QImage image = frame.toImage();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        m_processingFrame = false;
        return;
    }
    
    QRect faceRect;
    bool faceDetected = false;
    bool needDraw = false;  // 是否需要绘制检测框
    
#ifndef SEETAFACE_AVAILABLE
    // 使用 OpenCV Tracker 进行实时跟踪
    if (m_trackerInitialized && m_faceRecognizer) {
        // 尝试使用 Tracker 更新位置（跟踪比检测快，不需要格式转换）
        if (m_faceRecognizer->updateTracker(image, faceRect)) {
            faceDetected = true;
            m_trackerFailCount = 0;
            needDraw = true;
            // 跟踪成功，使用更短的间隔（50ms）以获得更流畅的体验，提高跟踪响应速度
            if (m_faceDetectionTimer->interval() != 50) {
                m_faceDetectionTimer->setInterval(50);
            }
        } else {
            // Tracker 更新失败，增加失败计数
            m_trackerFailCount++;
            m_trackerInitialized = false;
            
            // 如果连续失败次数过多，重新检测（降低阈值从3到2，更快响应）
            if (m_trackerFailCount >= 2) {
                m_trackerFailCount = 0;
                // 重新进行人脸检测（需要格式转换）
                if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32) {
                    image = image.convertToFormat(QImage::Format_RGB32);
                }
                faceDetected = m_faceRecognizer->detectFace(image, faceRect);
                if (faceDetected && !faceRect.isEmpty() && 
                    faceRect.width() < image.width() && faceRect.height() < image.height()) {
                    // 重新初始化 Tracker
                    m_trackerInitialized = m_faceRecognizer->initTracker(image, faceRect);
                    needDraw = true;
                }
                // 检测时使用中等间隔（200ms），平衡性能和响应速度
                if (m_faceDetectionTimer->interval() != 200) {
                    m_faceDetectionTimer->setInterval(200);
                }
            } else {
                // 暂时使用上一次的位置
                faceRect = m_currentFaceRect;
                faceDetected = !faceRect.isEmpty();
                needDraw = faceDetected;
            }
        }
    } else {
        // Tracker 未初始化，进行人脸检测（需要格式转换）
        if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32) {
            image = image.convertToFormat(QImage::Format_RGB32);
        }
        faceDetected = m_faceRecognizer->detectFace(image, faceRect);
        if (faceDetected && !faceRect.isEmpty() && 
            faceRect.width() < image.width() && faceRect.height() < image.height()) {
            // 初始化 Tracker
            m_trackerInitialized = m_faceRecognizer->initTracker(image, faceRect);
            m_trackerFailCount = 0;
            needDraw = true;
        }
        // 检测时使用中等间隔（200ms），平衡性能和响应速度
        if (m_faceDetectionTimer->interval() != 200) {
            m_faceDetectionTimer->setInterval(200);
        }
    }
#else
    // SeetaFace 暂不支持 Tracker，使用检测
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }
    faceDetected = m_faceRecognizer->detectFace(image, faceRect);
    needDraw = faceDetected;
    // SeetaFace 检测较慢，使用更长的间隔（400ms）
    if (m_faceDetectionTimer->interval() != 400) {
        m_faceDetectionTimer->setInterval(400);
    }
#endif
    
    if (needDraw && faceDetected && !faceRect.isEmpty() && 
        faceRect.width() < image.width() && faceRect.height() < image.height()) {
        
        // 保存当前检测到的人脸区域（图像坐标）
        m_currentFaceRect = faceRect;
        
        // 只在需要绘制时才转换格式
        if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32) {
            image = image.convertToFormat(QImage::Format_RGB32);
        }
        
        // 在视频帧上绘制检测框（创建带检测框的图像）
        QImage imageWithBox = image.copy();
        QPainter painter(&imageWithBox);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 绘制粗绿色边框（4像素宽，更明显）
        QPen pen(QColor(0, 255, 0), 4);
        painter.setPen(pen);
        painter.drawRect(faceRect);
        
        // 绘制文字背景（半透明黑色）
        QRect textRect(faceRect.x(), faceRect.y() - 25, 120, 20);
        if (textRect.y() < 0) {
            textRect.moveTop(faceRect.bottom() + 5);
        }
        painter.fillRect(textRect, QColor(0, 0, 0, 180));
        
        // 添加文字说明
        painter.setPen(QPen(QColor(0, 255, 0), 2));
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(textRect, Qt::AlignCenter, "检测到人脸");
        
        // 缓存带检测框的pixmap，供onVideoFrameChanged使用
        m_cachedPixmap = QPixmap::fromImage(imageWithBox);
        m_lastDrawnFaceRect = faceRect;
    } else {
        // 未检测到人脸，清除缓存
        m_currentFaceRect = QRect();
        m_cachedPixmap = QPixmap();
        m_lastDrawnFaceRect = QRect();
        m_trackerInitialized = false;
        m_trackerFailCount = 0;
#ifndef SEETAFACE_AVAILABLE
        if (m_faceRecognizer) {
            m_faceRecognizer->resetTracker();
        }
#endif
    }
    
    // 处理完成（不再在这里更新显示，由onVideoFrameChanged负责）
    m_processingFrame = false;
}

void FaceEnrollDialog::onSaveClicked()
{
    if (!m_hasCaptured || m_capturedImage.isNull()) {
        QMessageBox::warning(this, "错误", "请先拍照！");
        return;
    }

    if (saveFaceImage(m_capturedImage)) {
        QMessageBox::information(this, "成功", "人脸录入成功！");
        emit faceEnrolled(m_username, JsonHelper::loadFaceData(m_username));
        accept();
    } else {
        QMessageBox::critical(this, "错误", "保存失败，请重试！");
    }
}

void FaceEnrollDialog::onCancelClicked()
{
    reject();
}

void FaceEnrollDialog::onCameraError(QCamera::Error error)
{
    Q_UNUSED(error);
    QMessageBox::critical(this, "摄像头错误", QString("摄像头错误：%1").arg(m_camera->errorString()));
}

bool FaceEnrollDialog::saveFaceImage(const QImage &image)
{
    // 检查是否检测到人脸
    if (m_faceRecognizer) {
        QRect faceRect;
        if (!m_faceRecognizer->detectFace(image, faceRect)) {
            QMessageBox::warning(this, "警告", "未检测到人脸，请确保面部清晰可见！");
            return false;
        }
    }

    // 创建faces目录
    QDir dir;
    if (!dir.exists("faces")) {
        if (!dir.mkdir("faces")) {
            qDebug() << "无法创建faces目录";
            return false;
        }
    }

    // 保存图像文件
    QString imagePath = QString("faces/%1.jpg").arg(m_username);
    if (!image.save(imagePath, "JPG", 90)) {
        qDebug() << "保存图像失败：" << imagePath;
        return false;
    }

    // 保存到JSON
    if (!JsonHelper::saveFaceData(m_username, imagePath)) {
        return false;
    }

    // 训练人脸识别器
    if (m_faceRecognizer) {
        if (m_faceRecognizer->train(m_username, image)) {
#ifdef SEETAFACE_AVAILABLE
            qDebug() << "SeetaFace 训练成功：" << m_username;
#else
            qDebug() << "OpenCV 训练成功：" << m_username;
#endif
        } else {
#ifdef SEETAFACE_AVAILABLE
            qWarning() << "SeetaFace 训练失败：" << m_username;
#else
            qWarning() << "OpenCV 训练失败：" << m_username;
#endif
            // 训练失败不影响保存，只记录警告
        }
    }

    return true;
}
