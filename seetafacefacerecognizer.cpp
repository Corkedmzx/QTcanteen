#include "seetafacefacerecognizer.h"
#include "jsonhelper.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>
#include <locale>

#ifdef SEETAFACE_ENABLED
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/common_alignment.h>
#include <seeta/Common/CStruct.h>
#include <seeta/Common/Struct.h>
// TenniS 初始化函数（必须在创建 FaceDetector 之前调用）
#include <api/setup.h>
#include <vector>
#include <string>
#include <algorithm>
#endif

const QString SeetaFaceRecognizer::DEFAULT_MODEL_PATH = "models";

// 静态变量用于确保 TenniS 只初始化一次（线程安全）
static bool g_tennis_initialized = false;
static QMutex g_tennis_init_mutex;

SeetaFaceRecognizer::SeetaFaceRecognizer()
    : m_initialized(false)
{
#ifdef SEETAFACE_ENABLED
    m_detector = nullptr;
    m_landmarker = nullptr;
    m_recognizer = nullptr;
    
    // 默认模型文件路径查找顺序：
    // 1. 可执行文件目录下的 models 子目录
    // 2. 当前工作目录下的 models 子目录
    // 3. SeetaFace 安装目录下的 model 目录（E:/share/SeetaFace/bin/model）
    QString appDir = QCoreApplication::applicationDirPath();
    m_detectorModelPath = appDir + "/models/face_detector.csta";
    m_landmarkerModelPath = appDir + "/models/face_landmarker_pts5.csta";
    m_recognizerModelPath = appDir + "/models/face_recognizer.csta";
    
    // 如果可执行文件目录下没有，尝试当前工作目录
    if (!QFile::exists(m_detectorModelPath)) {
        m_detectorModelPath = "models/face_detector.csta";
        m_landmarkerModelPath = "models/face_landmarker_pts5.csta";
        m_recognizerModelPath = "models/face_recognizer.csta";
    }
    
    // 如果仍然不存在，尝试 SeetaFace 安装目录（从环境变量或默认路径）
    if (!QFile::exists(m_detectorModelPath)) {
        // 尝试从环境变量获取 SeetaFace 路径
        QString seetaFaceRoot = qEnvironmentVariable("SEETAFACE_ROOT", "E:/share/SeetaFace");
        QString seetaFaceModelsDir = seetaFaceRoot + "/bin/model";
        
        // 如果环境变量路径不存在，尝试默认路径
        if (!QFile::exists(seetaFaceModelsDir + "/face_detector.csta")) {
            seetaFaceModelsDir = "E:/share/SeetaFace/bin/model";
        }
        
        if (QFile::exists(seetaFaceModelsDir + "/face_detector.csta")) {
            m_detectorModelPath = seetaFaceModelsDir + "/face_detector.csta";
            m_landmarkerModelPath = seetaFaceModelsDir + "/face_landmarker_pts5.csta";
            m_recognizerModelPath = seetaFaceModelsDir + "/face_recognizer.csta";
            qDebug() << "使用 SeetaFace 安装目录的模型文件：" << seetaFaceModelsDir;
        }
    }
#else
    qWarning() << "SeetaFace 未启用！请在 CMakeLists.txt 中配置 SEETAFACE_ROOT";
#endif
    
    m_modelPath = "seetaface_model.json";
}

SeetaFaceRecognizer::~SeetaFaceRecognizer()
{
#ifdef SEETAFACE_ENABLED
    if (m_initialized) {
        saveModel();
    }
    
    if (m_detector) {
        delete m_detector;
        m_detector = nullptr;
    }
    
    if (m_landmarker) {
        delete m_landmarker;
        m_landmarker = nullptr;
    }
    
    if (m_recognizer) {
        delete m_recognizer;
        m_recognizer = nullptr;
    }
#endif
}

bool SeetaFaceRecognizer::initialize(const QString &modelPath)
{
#ifdef SEETAFACE_ENABLED
    if (m_initialized) {
        qWarning() << "SeetaFace 已经初始化";
        return true;
    }
    
    // 确保 TenniS 只初始化一次（线程安全）
    {
        QMutexLocker locker(&g_tennis_init_mutex);
        if (!g_tennis_initialized) {
            qDebug() << "初始化 TenniS（全局单次初始化）...";
            if (!ts_setup()) {
                qCritical() << "TenniS 初始化失败！";
                return false;
            }
            g_tennis_initialized = true;
            qDebug() << "TenniS 全局初始化成功";
        } else {
            qDebug() << "TenniS 已经初始化（跳过）";
        }
    }
    
    // 如果提供了模型路径，使用提供的路径
    if (!modelPath.isEmpty()) {
        QDir modelDir(modelPath);
        // 首先尝试 .csta 格式（SeetaFace6Open）
        m_detectorModelPath = modelDir.absoluteFilePath("face_detector.csta");
        m_landmarkerModelPath = modelDir.absoluteFilePath("face_landmarker_pts5.csta");
        m_recognizerModelPath = modelDir.absoluteFilePath("face_recognizer.csta");
        
        // 如果 .csta 文件不存在，尝试 .dat 格式（旧版 SeetaFace）
        if (!QFile::exists(m_detectorModelPath)) {
            m_detectorModelPath = modelDir.absoluteFilePath("fd_2_00.dat");
            m_landmarkerModelPath = modelDir.absoluteFilePath("pd_2_00_pts5.dat");
            m_recognizerModelPath = modelDir.absoluteFilePath("fr_2_10.dat");
        }
    }
    
    // 检查模型文件是否存在，如果不存在则尝试其他路径
    if (!QFile::exists(m_detectorModelPath)) {
        qWarning() << "人脸检测模型文件不存在：" << m_detectorModelPath;
        
        // 尝试 SeetaFace 安装目录（从环境变量或默认路径）
        QString seetaFaceRoot = qEnvironmentVariable("SEETAFACE_ROOT", "E:/share/SeetaFace");
        QString seetaFaceModelsDir = seetaFaceRoot + "/bin/model";
        
        // 如果环境变量路径不存在，尝试默认路径
        if (!QFile::exists(seetaFaceModelsDir + "/face_detector.csta") && 
            !QFile::exists(seetaFaceModelsDir + "/fd_2_00.dat")) {
            seetaFaceModelsDir = "E:/share/SeetaFace/bin/model";
        }
        // 首先尝试 .csta 格式（SeetaFace6Open）
        QString altDetectorPath = seetaFaceModelsDir + "/face_detector.csta";
        QString altLandmarkerPath = seetaFaceModelsDir + "/face_landmarker_pts5.csta";
        QString altRecognizerPath = seetaFaceModelsDir + "/face_recognizer.csta";
        
        // 如果 .csta 格式不存在，尝试 .dat 格式（旧版 SeetaFace）
        if (!QFile::exists(altDetectorPath)) {
            altDetectorPath = seetaFaceModelsDir + "/fd_2_00.dat";
            altLandmarkerPath = seetaFaceModelsDir + "/pd_2_00_pts5.dat";
            altRecognizerPath = seetaFaceModelsDir + "/fr_2_10.dat";
        }
        
        if (QFile::exists(altDetectorPath) && QFile::exists(altLandmarkerPath) && QFile::exists(altRecognizerPath)) {
            m_detectorModelPath = altDetectorPath;
            m_landmarkerModelPath = altLandmarkerPath;
            m_recognizerModelPath = altRecognizerPath;
            qDebug() << "找到模型文件在：" << seetaFaceModelsDir;
        } else {
            qWarning() << "请确保模型文件在以下位置之一：";
            qWarning() << "  1. " << QCoreApplication::applicationDirPath() << "/models/";
            qWarning() << "  2. models/ (当前工作目录)";
            qWarning() << "  3. " << seetaFaceModelsDir;
        return false;
        }
    }
    
    if (!QFile::exists(m_landmarkerModelPath)) {
        qWarning() << "人脸关键点模型文件不存在：" << m_landmarkerModelPath;
        return false;
    }
    
    if (!QFile::exists(m_recognizerModelPath)) {
        qWarning() << "人脸识别模型文件不存在：" << m_recognizerModelPath;
        return false;
    }
    
    try {
        qDebug() << "开始初始化 SeetaFace...";
        
        // 注意：TenniS 初始化已在函数开头完成（全局单次初始化）
        
        qDebug() << "检测器模型：" << m_detectorModelPath;
        qDebug() << "关键点模型：" << m_landmarkerModelPath;
        qDebug() << "识别器模型：" << m_recognizerModelPath;
        
        // 创建模型设置（使用 CPU）
        qDebug() << "创建模型设置...";
        
        // 确保使用绝对路径
        QString absDetectorPath = QFileInfo(m_detectorModelPath).absoluteFilePath();
        QString absLandmarkerPath = QFileInfo(m_landmarkerModelPath).absoluteFilePath();
        QString absRecognizerPath = QFileInfo(m_recognizerModelPath).absoluteFilePath();
        
        qDebug() << "绝对路径 - 检测器：" << absDetectorPath;
        qDebug() << "绝对路径 - 关键点：" << absLandmarkerPath;
        qDebug() << "绝对路径 - 识别器：" << absRecognizerPath;
        
        // 转换为标准路径格式（使用正斜杠，SeetaFace 可能需要正斜杠）
        // 注意：不要使用 QDir::toNativeSeparators，保持正斜杠
        absDetectorPath = absDetectorPath.replace('\\', '/');
        absLandmarkerPath = absLandmarkerPath.replace('\\', '/');
        absRecognizerPath = absRecognizerPath.replace('\\', '/');
        
        // 验证文件是否存在
        if (!QFile::exists(absDetectorPath)) {
            qCritical() << "检测器模型文件不存在：" << absDetectorPath;
            return false;
        }
        if (!QFile::exists(absLandmarkerPath)) {
            qCritical() << "关键点模型文件不存在：" << absLandmarkerPath;
            return false;
        }
        if (!QFile::exists(absRecognizerPath)) {
            qCritical() << "识别器模型文件不存在：" << absRecognizerPath;
            return false;
        }
        
        // 创建模型设置（使用构造函数直接初始化，更安全）
        // ModelSetting 有构造函数：ModelSetting(const std::string &model, Device device)
        std::string detectorPathStr = absDetectorPath.toStdString();
        std::string landmarkerPathStr = absLandmarkerPath.toStdString();
        std::string recognizerPathStr = absRecognizerPath.toStdString();
        
        qDebug() << "创建 ModelSetting 对象...";
        qDebug() << "检测器路径字符串长度：" << detectorPathStr.length();
        qDebug() << "检测器路径字符串内容：" << QString::fromStdString(detectorPathStr);
        
        // 验证路径字符串不为空
        if (detectorPathStr.empty()) {
            qCritical() << "检测器路径字符串为空！";
            return false;
        }
        if (landmarkerPathStr.empty()) {
            qCritical() << "关键点路径字符串为空！";
            return false;
        }
        if (recognizerPathStr.empty()) {
            qCritical() << "识别器路径字符串为空！";
            return false;
        }
        
        // 使用构造函数直接初始化（推荐方式）
        // 注意：确保路径字符串在 ModelSetting 对象生命周期内有效
        seeta::ModelSetting detectorSetting;
        try {
            detectorSetting = seeta::ModelSetting(detectorPathStr, seeta::ModelSetting::CPU);
            qDebug() << "detectorSetting 创建成功";
        } catch (const std::exception &e) {
            qCritical() << "创建 detectorSetting 失败：" << e.what();
            return false;
        }
        
        seeta::ModelSetting landmarkerSetting;
        try {
            landmarkerSetting = seeta::ModelSetting(landmarkerPathStr, seeta::ModelSetting::CPU);
            qDebug() << "landmarkerSetting 创建成功";
        } catch (const std::exception &e) {
            qCritical() << "创建 landmarkerSetting 失败：" << e.what();
            return false;
        }
        
        seeta::ModelSetting recognizerSetting;
        try {
            recognizerSetting = seeta::ModelSetting(recognizerPathStr, seeta::ModelSetting::CPU);
            qDebug() << "recognizerSetting 创建成功";
        } catch (const std::exception &e) {
            qCritical() << "创建 recognizerSetting 失败：" << e.what();
            return false;
        }
        
        // 初始化检测器
        qDebug() << "初始化人脸检测器...";
        qDebug() << "模型文件大小：" << QFileInfo(m_detectorModelPath).size() << " 字节";
        qDebug() << "模型文件路径：" << m_detectorModelPath;
        qDebug() << "设备类型：CPU";
        
        // 刷新输出缓冲区，确保日志及时显示
        QCoreApplication::processEvents();
        
        // 验证 ModelSetting 是否有效
        qDebug() << "验证 ModelSetting...";
        qDebug() << "设备类型：" << detectorSetting.get_device();
        
        // 验证模型路径是否正确设置
        try {
            const auto &model = detectorSetting.get_model();
            qDebug() << "模型数量：" << model.size();
            if (model.empty()) {
                qCritical() << "ModelSetting 中没有模型路径！";
                return false;
            }
            for (size_t i = 0; i < model.size(); ++i) {
                qDebug() << "模型路径 [" << i << "]：" << QString::fromStdString(model[i]);
                // 验证文件是否存在
                if (!QFile::exists(QString::fromStdString(model[i]))) {
                    qCritical() << "模型文件不存在：" << QString::fromStdString(model[i]);
                    return false;
                }
            }
        } catch (const std::exception &e) {
            qCritical() << "获取模型路径失败：" << e.what();
            return false;
        } catch (...) {
            qCritical() << "获取模型路径失败（未知异常）";
            return false;
        }
        
        // 刷新输出缓冲区
        QCoreApplication::processEvents();
        
        try {
            qDebug() << "正在创建 FaceDetector 对象...";
            qDebug() << "使用模型路径：" << QString::fromStdString(detectorPathStr);
            
            // 验证模型文件可读性
            QFileInfo modelFileInfo(QString::fromStdString(detectorPathStr));
            if (!modelFileInfo.exists() || !modelFileInfo.isReadable()) {
                qCritical() << "模型文件不可读：" << QString::fromStdString(detectorPathStr);
                return false;
            }
            qDebug() << "模型文件大小：" << modelFileInfo.size() << " 字节";
            
            // 刷新输出缓冲区，确保所有日志都已输出
            QCoreApplication::processEvents();
            
            // 尝试延迟创建，先分配内存
            qDebug() << "准备调用 FaceDetector 构造函数...";
            qDebug() << "ModelSetting 设备类型：" << detectorSetting.get_device();
            qDebug() << "ModelSetting 模型数量：" << detectorSetting.count();
            
            // 直接创建并赋值
            // 如果这里崩溃，可能是 DLL 加载问题或模型文件问题
            // 注意：这里可能会访问模型文件，如果文件格式不正确会导致崩溃
            qDebug() << "调用 FaceDetector 构造函数（这可能会读取模型文件）...";
            
            // 尝试预加载模型文件到内存，验证文件可读性
            QFile modelFile(QString::fromStdString(detectorPathStr));
            if (!modelFile.open(QIODevice::ReadOnly)) {
                qCritical() << "无法打开模型文件：" << QString::fromStdString(detectorPathStr);
                return false;
            }
            QByteArray modelData = modelFile.readAll();
            modelFile.close();
            qDebug() << "模型文件已预加载到内存，大小：" << modelData.size() << " 字节";
            
            // 刷新输出缓冲区
            QCoreApplication::processEvents();
            
            // 现在创建 FaceDetector
            qDebug() << "准备创建 FaceDetector 对象（这可能会加载 TSM 模块和创建 Workbench）...";
            
            // 在创建 FaceDetector 之前，临时设置全局 locale 为 classic
            // 这可以防止 ModelFileInputStream 构造时发生 std::locale 相关的崩溃
            // 注意：我们只在创建 FaceDetector 时临时设置，创建完成后恢复
            std::locale oldLocale;
            bool localeChanged = false;
            try {
                // 尝试保存当前 locale（如果可能）
                try {
                    oldLocale = std::locale::global(std::locale::classic());
                    localeChanged = true;
                    qDebug() << "已临时设置 classic locale，避免 std::locale 冲突";
                } catch (const std::exception &e) {
                    qWarning() << "无法设置 locale：" << e.what() << "，继续尝试创建 FaceDetector";
                }
                
                // 刷新输出缓冲区，确保日志输出
                QCoreApplication::processEvents();
                
                // 尝试使用 try-catch 捕获可能的异常
                // 但如果是段错误（SIGSEGV），异常捕获可能无效
                m_detector = new seeta::FaceDetector(detectorSetting);
                
                // 如果成功创建，恢复原来的 locale
                if (localeChanged) {
                    try {
                        std::locale::global(oldLocale);
                        qDebug() << "已恢复原来的 locale";
                    } catch (const std::exception &e) {
                        qWarning() << "无法恢复 locale：" << e.what();
                    }
                }
            } catch (const std::exception &e) {
                // 如果创建失败，尝试恢复 locale
                if (localeChanged) {
                    try {
                        std::locale::global(oldLocale);
                    } catch (...) {
                        // 忽略恢复失败
                    }
                }
                qCritical() << "创建 FaceDetector 时发生异常：" << e.what();
                throw;
            } catch (...) {
                // 如果创建失败，尝试恢复 locale
                if (localeChanged) {
                    try {
                        std::locale::global(oldLocale);
                    } catch (...) {
                        // 忽略恢复失败
                    }
                }
                qCritical() << "创建 FaceDetector 时发生未知异常";
                throw;
            }
            
            qDebug() << "FaceDetector 对象创建成功，地址：" << QString("0x%1").arg(reinterpret_cast<quintptr>(m_detector), 0, 16);
            qDebug() << "人脸检测器初始化成功";
        } catch (const std::exception &e) {
            qCritical() << "初始化人脸检测器失败（异常）：" << e.what();
            qCritical() << "异常类型：" << typeid(e).name();
            if (m_detector) {
                delete m_detector;
                m_detector = nullptr;
            }
            return false;
        } catch (...) {
            qCritical() << "初始化人脸检测器失败（未知异常）";
            if (m_detector) {
                delete m_detector;
                m_detector = nullptr;
            }
            return false;
        }
        
        // 初始化关键点检测器
        qDebug() << "初始化关键点检测器...";
        m_landmarker = new seeta::FaceLandmarker(landmarkerSetting);
        qDebug() << "关键点检测器初始化成功";
        
        // 初始化识别器
        qDebug() << "初始化人脸识别器...";
        m_recognizer = new seeta::FaceRecognizer(recognizerSetting);
        qDebug() << "人脸识别器初始化成功";
        
        // 尝试加载已保存的模型
        qDebug() << "加载已保存的模型...";
        loadModel();
        
        m_initialized = true;
        qDebug() << "SeetaFace 初始化成功";
        qDebug() << "已训练用户数：" << m_trainedUsers.size();
        
        return true;
    } catch (const std::exception &e) {
        qCritical() << "SeetaFace 初始化失败（异常）：" << e.what();
        return false;
    } catch (...) {
        qCritical() << "SeetaFace 初始化失败（未知异常）";
        return false;
    }
#else
    qWarning() << "SeetaFace 未启用";
    return false;
#endif
}

seeta::SeetaImageData SeetaFaceRecognizer::qImageToSeetaImageData(const QImage &image)
{
#ifdef SEETAFACE_ENABLED
    QImage img = image.convertToFormat(QImage::Format_RGB888);
    
    seeta::SeetaImageData seetaImg;
    seetaImg.width = img.width();
    seetaImg.height = img.height();
    seetaImg.channels = 3;
    seetaImg.data = const_cast<unsigned char*>(img.bits());
    
    return seetaImg;
#else
    seeta::SeetaImageData seetaImg = {0};
    return seetaImg;
#endif
}

bool SeetaFaceRecognizer::detectFace(const QImage &image, QRect &faceRect)
{
#ifdef SEETAFACE_ENABLED
    if (!m_initialized || !m_detector) {
        qWarning() << "SeetaFace 未初始化";
        return false;
    }
    
    if (image.isNull()) {
        return false;
    }
    
    seeta::SeetaImageData seetaImg = qImageToSeetaImageData(image);
    
    // 检测人脸
    auto faces = m_detector->detect_v2(seetaImg);
    
    if (faces.empty()) {
        return false;
    }
    
    // 返回最大的人脸
    SeetaRect largestFace = faces[0].pos;
    for (size_t i = 1; i < faces.size(); ++i) {
        int area1 = largestFace.width * largestFace.height;
        int area2 = faces[i].pos.width * faces[i].pos.height;
        if (area2 > area1) {
            largestFace = faces[i].pos;
        }
    }
    
    faceRect = QRect(largestFace.x, largestFace.y, largestFace.width, largestFace.height);
    return true;
#else
    Q_UNUSED(image);
    Q_UNUSED(faceRect);
    return false;
#endif
}

QImage SeetaFaceRecognizer::preprocessImage(const QImage &image)
{
    // 转换为 RGB 格式
    QImage processed = image.convertToFormat(QImage::Format_RGB888);
    
    // 可以在这里添加其他预处理步骤，如尺寸调整、直方图均衡化等
    // 但 SeetaFace 通常不需要太多预处理
    
    return processed;
}

// 遮挡检测（返回遮挡比例，0.0-1.0）
double SeetaFaceRecognizer::detectOcclusion(const QImage &image, const QRect &faceRect)
{
#ifdef SEETAFACE_ENABLED
    if (!m_initialized || !m_landmarker || image.isNull()) {
        return 0.0;
    }
    
    // 转换为 SeetaFace 格式
    seeta::SeetaImageData seetaImg = qImageToSeetaImageData(image);
    seeta::SeetaRect seetaRect;
    seetaRect.x = faceRect.x();
    seetaRect.y = faceRect.y();
    seetaRect.width = faceRect.width();
    seetaRect.height = faceRect.height();
    
    // 获取关键点
    auto points = m_landmarker->mark(seetaImg, seetaRect);
    
    // SeetaFace 通常返回 5 个关键点：左眼、右眼、鼻子、左嘴角、右嘴角
    // 如果关键点检测失败或关键点位置异常，可能表示被遮挡
    
    // 检查关键点是否在合理范围内
    int validPoints = 0;
    int totalPoints = 5;  // SeetaFace 标准5点
    
    // 定义关键区域（基于标准人脸比例）
    int faceCenterX = faceRect.x() + faceRect.width() / 2;
    int faceCenterY = faceRect.y() + faceRect.height() / 2;
    
    // 检查每个关键点是否在合理位置
    for (int i = 0; i < totalPoints && i < static_cast<int>(points.size()); ++i) {
        const auto &point = points[i];
        
        // 检查关键点是否在图像范围内
        if (point.x >= 0 && point.x < image.width() && 
            point.y >= 0 && point.y < image.height()) {
            
            // 检查关键点是否在预期的人脸区域内
            // 左眼应该在左上1/4区域
            // 右眼应该在右上1/4区域
            // 鼻子应该在中心区域
            // 嘴角应该在下方区域
            
            bool inValidRegion = false;
            if (i == 0) {  // 左眼
                inValidRegion = (point.x < faceCenterX && point.y < faceCenterY);
            } else if (i == 1) {  // 右眼
                inValidRegion = (point.x >= faceCenterX && point.y < faceCenterY);
            } else if (i == 2) {  // 鼻子
                inValidRegion = (std::abs(point.x - faceCenterX) < faceRect.width() / 4 &&
                                std::abs(point.y - faceCenterY) < faceRect.height() / 4);
            } else {  // 嘴角
                inValidRegion = (point.y > faceCenterY);
            }
            
            if (inValidRegion) {
                validPoints++;
            }
        }
    }
    
    // 计算遮挡比例（基于有效关键点数量）
    double occlusionRatio = 1.0 - (static_cast<double>(validPoints) / totalPoints);
    
    // 额外检查：如果关键点位置异常（例如都在同一侧），增加遮挡比例
    if (points.size() >= 2) {
        // 检查左右眼的位置关系
        if (points.size() >= 2) {
            double leftEyeX = points[0].x;
            double rightEyeX = points[1].x;
            double eyeDistance = std::abs(rightEyeX - leftEyeX);
            
            // 正常眼睛距离应该约为脸宽的1/3到1/2
            double expectedEyeDistance = faceRect.width() * 0.4;
            if (eyeDistance < expectedEyeDistance * 0.5) {
                // 眼睛距离过近，可能被遮挡
                occlusionRatio = std::max(occlusionRatio, 0.3);
            }
        }
    }
    
    return std::max(0.0, std::min(1.0, occlusionRatio));
#else
    Q_UNUSED(image);
    Q_UNUSED(faceRect);
    return 0.0;
#endif
}

bool SeetaFaceRecognizer::train(const QString &username, const QImage &faceImage)
{
#ifdef SEETAFACE_ENABLED
    if (!m_initialized || !m_recognizer || !m_landmarker) {
        qWarning() << "SeetaFace 未初始化";
        return false;
    }
    
    if (faceImage.isNull()) {
        return false;
    }
    
    // 使用原始图像进行检测（不裁剪，保持完整图像上下文）
    seeta::SeetaImageData seetaImg = qImageToSeetaImageData(faceImage);
    
    // 检测人脸（在完整图像上检测，更准确）
    auto faces = m_detector->detect_v2(seetaImg);
    if (faces.empty()) {
        qWarning() << "未检测到人脸";
        return false;
    }
    
    // 选择最大的人脸（通常是最主要的人脸）
    seeta::SeetaFaceInfo largestFace = faces[0];
    for (size_t i = 1; i < faces.size(); ++i) {
        int area1 = largestFace.pos.width * largestFace.pos.height;
        int area2 = faces[i].pos.width * faces[i].pos.height;
        if (area2 > area1) {
            largestFace = faces[i];
        }
    }
    
    // 获取关键点（使用完整图像和检测到的人脸位置）
    auto points = m_landmarker->mark(seetaImg, largestFace.pos);
    
    // 遮挡检测（基于关键点）
    QRect faceRect(largestFace.pos.x, largestFace.pos.y, largestFace.pos.width, largestFace.pos.height);
    double occlusionRatio = detectOcclusion(faceImage, faceRect);
    if (occlusionRatio > 0.10) {  // 遮挡超过10%，拒绝训练
        qWarning() << "人脸遮挡过多，无法录入，遮挡比例：" << (occlusionRatio * 100) << "%";
        return false;
    }
    
    // 提取人脸特征
    int featureSize = m_recognizer->GetExtractFeatureSize();
    std::vector<float> features(featureSize);
    if (!m_recognizer->Extract(seetaImg, points.data(), features.data())) {
        qWarning() << "特征提取失败";
        return false;
    }
    
    // 归一化特征向量（L2归一化，提高匹配准确性）
    float norm = 0.0f;
    for (float f : features) {
        norm += f * f;
    }
    if (norm > 0.0f) {
        norm = std::sqrt(norm);
        for (float &f : features) {
            f /= norm;
        }
    }
    
    // 检查用户是否已存在
    int userIndex = m_trainedUsers.indexOf(username);
    int faceId;
    
    if (userIndex >= 0) {
        // 用户已存在，使用平均特征以提高鲁棒性（多样本融合）
        faceId = m_faceIds[userIndex];
        if (faceId < m_faceFeatures.size()) {
            QVector<float> &oldFeatures = m_faceFeatures[faceId];
            // 计算平均特征（可以存储多个样本的平均值）
            for (int i = 0; i < oldFeatures.size() && i < static_cast<int>(features.size()); ++i) {
                oldFeatures[i] = (oldFeatures[i] + features[i]) * 0.5f;
            }
            // 重新归一化
            float avgNorm = 0.0f;
            for (float f : oldFeatures) {
                avgNorm += f * f;
            }
            if (avgNorm > 0.0f) {
                avgNorm = std::sqrt(avgNorm);
                for (float &f : oldFeatures) {
                    f /= avgNorm;
                }
            }
            qDebug() << "更新用户特征（多样本融合）：" << username;
        }
    } else {
        // 新用户，创建新的 face_id
        if (m_faceIds.isEmpty()) {
            faceId = 0;
        } else {
            faceId = *std::max_element(m_faceIds.begin(), m_faceIds.end()) + 1;
        }
        
        m_trainedUsers.append(username);
        m_faceIds.append(faceId);
        m_faceFeatures.append(QVector<float>(features.begin(), features.end()));
    }
    
    // 保存模型
    saveModel();
    
    qDebug() << "训练成功，用户：" << username << "，Face ID：" << faceId;
    return true;
#else
    Q_UNUSED(username);
    Q_UNUSED(faceImage);
    qWarning() << "SeetaFace 未启用";
    return false;
#endif
}

QString SeetaFaceRecognizer::recognize(const QImage &image, float &similarity)
{
#ifdef SEETAFACE_ENABLED
    similarity = 0.0f;
    
    if (!m_initialized || !m_recognizer || !m_landmarker || m_trainedUsers.isEmpty()) {
        return QString();
    }
    
    if (image.isNull()) {
        return QString();
    }
    
    // 使用原始图像进行检测（不裁剪，保持完整图像上下文）
    seeta::SeetaImageData seetaImg = qImageToSeetaImageData(image);
    
    // 检测人脸（在完整图像上检测，更准确）
    auto faces = m_detector->detect_v2(seetaImg);
    if (faces.empty()) {
        return QString();
    }
    
    // 选择最大的人脸（通常是最主要的人脸）
    seeta::SeetaFaceInfo largestFace = faces[0];
    for (size_t i = 1; i < faces.size(); ++i) {
        int area1 = largestFace.pos.width * largestFace.pos.height;
        int area2 = faces[i].pos.width * faces[i].pos.height;
        if (area2 > area1) {
            largestFace = faces[i];
        }
    }
    
    // 获取关键点（使用完整图像和检测到的人脸位置）
    auto points = m_landmarker->mark(seetaImg, largestFace.pos);
    
    // 遮挡检测（基于关键点）
    QRect faceRect(largestFace.pos.x, largestFace.pos.y, largestFace.pos.width, largestFace.pos.height);
    double occlusionRatio = detectOcclusion(image, faceRect);
    if (occlusionRatio > 0.10) {  // 遮挡超过10%，拒绝识别
        qDebug() << "人脸遮挡过多，遮挡比例：" << (occlusionRatio * 100) << "%";
        return QString();
    }
    
    // 提取特征（使用完整图像和关键点）
    int featureSize = m_recognizer->GetExtractFeatureSize();
    std::vector<float> queryFeatures(featureSize);
    if (!m_recognizer->Extract(seetaImg, points.data(), queryFeatures.data())) {
        return QString();
    }
    
    // 归一化查询特征向量（L2归一化）
    float queryNorm = 0.0f;
    for (float f : queryFeatures) {
        queryNorm += f * f;
    }
    if (queryNorm > 0.0f) {
        queryNorm = std::sqrt(queryNorm);
        for (float &f : queryFeatures) {
            f /= queryNorm;
        }
    }
    
    // 与所有已训练的特征进行比较（使用余弦相似度）
    float bestSimilarity = 0.0f;
    int bestIndex = -1;
    
    for (int i = 0; i < m_faceFeatures.size(); ++i) {
        const QVector<float> &trainedFeatures = m_faceFeatures[i];
        
        // 计算余弦相似度（特征向量已归一化，直接计算点积）
        float dotProduct = 0.0f;
        
        int minSize = static_cast<int>(std::min(queryFeatures.size(), static_cast<size_t>(trainedFeatures.size())));
        for (int j = 0; j < minSize; ++j) {
            dotProduct += queryFeatures[j] * trainedFeatures[j];
        }
        
        // 由于特征已归一化，点积就是余弦相似度
        float currentSimilarity = std::max(0.0f, std::min(1.0f, dotProduct));
        
        if (currentSimilarity > bestSimilarity) {
            bestSimilarity = currentSimilarity;
            bestIndex = i;
        }
    }
    
    // 调整阈值：0.60 是一个平衡点，既能识别正确的人脸，又能减少误识别
    // SeetaFace 的特征质量较高，可以使用稍高的阈值
    const float SIMILARITY_THRESHOLD = 0.60f;
    
    if (bestIndex >= 0 && bestSimilarity >= SIMILARITY_THRESHOLD) {
        similarity = bestSimilarity;
        return m_trainedUsers[bestIndex];
    }
    
    return QString();
#else
    Q_UNUSED(image);
    Q_UNUSED(similarity);
    return QString();
#endif
}

bool SeetaFaceRecognizer::saveModel(const QString &modelPath)
{
    QString savePath = modelPath.isEmpty() ? m_modelPath : modelPath;
    
    QJsonObject root;
    QJsonArray usersArray;
    QJsonArray faceIdsArray;
    QJsonArray featuresArray;
    
    for (int i = 0; i < m_trainedUsers.size(); ++i) {
        usersArray.append(m_trainedUsers[i]);
        faceIdsArray.append(m_faceIds[i]);
        
        // 将特征向量转换为 JSON 数组
        QJsonArray featureArray;
        for (float f : m_faceFeatures[i]) {
            featureArray.append(f);
        }
        featuresArray.append(featureArray);
    }
    
    root["users"] = usersArray;
    root["faceIds"] = faceIdsArray;
    root["features"] = featuresArray;
    
    QJsonDocument doc(root);
    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "模型保存成功：" << savePath;
        return true;
    } else {
        qWarning() << "无法保存模型文件：" << savePath;
        return false;
    }
}

bool SeetaFaceRecognizer::loadModel(const QString &modelPath)
{
    QString loadPath = modelPath.isEmpty() ? m_modelPath : modelPath;
    
    if (!QFile::exists(loadPath)) {
        return false;
    }
    
    QFile file(loadPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开模型文件：" << loadPath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        qWarning() << "模型文件格式错误";
        return false;
    }
    
    QJsonObject root = doc.object();
    QJsonArray usersArray = root["users"].toArray();
    QJsonArray faceIdsArray = root["faceIds"].toArray();
    QJsonArray featuresArray = root["features"].toArray();
    
    m_trainedUsers.clear();
    m_faceIds.clear();
    m_faceFeatures.clear();
    
    int count = qMin(usersArray.size(), qMin(faceIdsArray.size(), featuresArray.size()));
    for (int i = 0; i < count; ++i) {
        m_trainedUsers.append(usersArray[i].toString());
        m_faceIds.append(faceIdsArray[i].toInt());
        
        QJsonArray featureArray = featuresArray[i].toArray();
        QVector<float> features;
        for (const QJsonValue &val : featureArray) {
            features.append(val.toDouble());
        }
        m_faceFeatures.append(features);
    }
    
    qDebug() << "模型加载成功，已训练用户数：" << m_trainedUsers.size();
    return true;
}

void SeetaFaceRecognizer::clear()
{
    m_trainedUsers.clear();
    m_faceIds.clear();
    m_faceFeatures.clear();
    m_initialized = false;
    qDebug() << "已清除所有训练数据";
}
