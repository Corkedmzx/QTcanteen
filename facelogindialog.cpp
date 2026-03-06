#include "facelogindialog.h"
#include "jsonhelper.h"
#include <QDebug>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QTimer>
#include <QLabel>

FaceLoginDialog::FaceLoginDialog(QWidget *parent)
    : QDialog(parent), m_camera(nullptr), m_videoLabel(nullptr), m_imageCapture(nullptr), m_captureSession(nullptr),
      m_faceDetectionTimer(nullptr), m_videoSink(nullptr),
      m_trackerInitialized(false), m_trackerFailCount(0), m_processingFrame(false)
{
    setWindowTitle("人脸识别登录");
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
    if (!m_faceRecognizer->initialize()) {
        QMessageBox::warning(this, "警告", "人脸识别器初始化失败，可能没有已录入的人脸数据！");
    }
#else
    // 没有可用的库，显示错误并关闭对话框
    m_faceRecognizer = nullptr;
    QMessageBox::warning(this, "错误", 
        "人脸识别功能不可用！\n\n"
        "请安装 OpenCV 或 SeetaFace6Open 库以启用人脸识别功能。\n"
        "当前可以使用账号密码登录。");
    reject();  // 关闭对话框
    return;
#endif

    setupUI();
    startCamera();
}

FaceLoginDialog::~FaceLoginDialog()
{
    stopCamera();
    // 清理人脸识别器
    if (m_faceRecognizer) {
        delete m_faceRecognizer;
        m_faceRecognizer = nullptr;
    }
}

void FaceLoginDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel("人脸识别登录", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
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
    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(0);
    
    m_videoLabel = new QLabel(videoContainer);
    m_videoLabel->setMinimumSize(480, 360);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 5px; background-color: #000;");
    videoLayout->addWidget(m_videoLabel);
    
    mainLayout->addWidget(videoContainer, 1);
    
    // 创建定时器用于实时人脸检测（较低频率，不影响视频流显示）
    m_faceDetectionTimer = new QTimer(this);
    m_faceDetectionTimer->setInterval(100);  // 检测间隔100ms，与视频流显示分离
    connect(m_faceDetectionTimer, &QTimer::timeout, this, &FaceLoginDialog::onFaceDetectionTimer);

    // 提示文字
    QLabel *hintLabel = new QLabel("请将面部对准摄像头，然后点击识别", this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #666; font-size: 12px;");
    mainLayout->addWidget(hintLabel);

    // 按钮区域（添加间距，避免与显示区域重叠）
    mainLayout->addSpacing(10);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_recognizeBtn = new QPushButton("开始识别", this);
    m_recognizeBtn->setFixedSize(120, 40);
    m_recognizeBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; font-size: 14px; }"
                                  "QPushButton:hover { background-color: #45a049; }");
    connect(m_recognizeBtn, &QPushButton::clicked, this, &FaceLoginDialog::onRecognizeClicked);

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setFixedSize(100, 40);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addStretch();
    btnLayout->addWidget(m_recognizeBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);
}

void FaceLoginDialog::startCamera()
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
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &FaceLoginDialog::onVideoFrameChanged);

    connect(m_imageCapture, &QImageCapture::imageCaptured, this, &FaceLoginDialog::onImageCaptured);
    connect(m_camera, &QCamera::errorOccurred, this, &FaceLoginDialog::onCameraError);

    m_camera->start();
    
    // 启动实时人脸检测定时器
    if (m_faceDetectionTimer) {
        m_faceDetectionTimer->start();
    }
}

void FaceLoginDialog::stopCamera()
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

void FaceLoginDialog::onRecognizeClicked()
{
    if (!m_imageCapture || !m_camera) {
        QMessageBox::warning(this, "错误", "摄像头未就绪，请稍候再试！");
        return;
    }

    m_recognizeBtn->setEnabled(false);
    m_recognizeBtn->setText("识别中...");
    m_imageCapture->capture();
}

void FaceLoginDialog::onImageCaptured(int id, const QImage &image)
{
    Q_UNUSED(id);
    
    m_recognizeBtn->setEnabled(true);
    m_recognizeBtn->setText("开始识别");
    
    if (image.isNull()) {
        QMessageBox::warning(this, "错误", "图像捕获失败！");
        return;
    }

    // 进行人脸识别
    QString username = recognizeFace(image);
    if (!username.isEmpty()) {
        QMessageBox::information(this, "识别成功", QString("欢迎，%1！").arg(username));
        emit faceLoginSuccess(username);
        accept();
    } else {
        // 识别失败后，重置 Tracker
        m_trackerInitialized = false;
        m_trackerFailCount = 0;
#ifndef SEETAFACE_AVAILABLE
        if (m_faceRecognizer) {
            m_faceRecognizer->resetTracker();
        }
#endif
        QMessageBox::warning(this, "识别失败", "未找到匹配的人脸，请重试或使用账号密码登录！");
    }
}

void FaceLoginDialog::onCameraError(QCamera::Error error)
{
    Q_UNUSED(error);
    QMessageBox::critical(this, "摄像头错误", QString("摄像头错误：%1").arg(m_camera->errorString()));
}

QString FaceLoginDialog::recognizeFace(const QImage &capturedImage)
{
#ifdef SEETAFACE_AVAILABLE
    // 使用 SeetaFace 进行人脸识别
    if (!m_faceRecognizer) {
        return QString();
    }
    
    float similarity = 0.0f;
    QString username = m_faceRecognizer->recognize(capturedImage, similarity);
    
    // similarity 是相似度（0-1），值越大越相似
    // 调整阈值：降低到 0.55 以提高识别率，同时保持准确性
    // 实际阈值已在 recognize() 函数中设置（0.55），这里只做最终验证
    if (!username.isEmpty() && similarity >= 0.50f) {
        qDebug() << "识别成功：" << username << "，相似度：" << similarity;
        return username;
    } else if (!username.isEmpty()) {
        qDebug() << "识别相似度不足：" << username << "，相似度：" << similarity << "（阈值：0.50）";
    }
    
    return QString();
#else
    // 使用 OpenCV 的 LBPHFaceRecognizer 进行人脸识别
    if (!m_faceRecognizer) {
        return QString();
    }
    
    double confidence = 0.0;
    QString username = m_faceRecognizer->recognize(capturedImage, confidence);
    
    // confidence 是相似度（0-1），值越大越相似
    // 设置阈值 0.6（60%相似度），可以根据需要调整
    if (!username.isEmpty() && confidence >= 0.6) {
        qDebug() << "识别成功：" << username << "，相似度：" << confidence;
        return username;
    }
    
    return QString();
#endif
}

void FaceLoginDialog::onVideoFrameChanged()
{
    // 快速更新视频流显示，不进行检测处理（保证流畅度）
    if (!m_videoSink || !m_videoLabel) {
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

void FaceLoginDialog::onFaceDetectionTimer()
{
    // 如果正在识别，不进行实时检测
    if (!m_recognizeBtn->isEnabled()) {
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
