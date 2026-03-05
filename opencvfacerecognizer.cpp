#include "opencvfacerecognizer.h"
#include "jsonhelper.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 构造函数, 初始化LBPH识别器, 设置模型路径
OpenCVFaceRecognizer::OpenCVFaceRecognizer()
    : m_initialized(false), m_dnnAvailable(false)
#ifdef OPENCV_TRACKING_AVAILABLE
    , m_trackerInitialized(false)
#else
    , m_trackerInitialized(false), m_trackFrameCount(0)
#endif
{
    m_modelPath = "face_model";
    
#ifdef OPENCV_FACE_AVAILABLE
    // 初始化LBPH识别器（作为后备方案）
    // 参数：radius=1, neighbors=8, grid_x=8, grid_y=8, threshold=80.0
    m_recognizer = cv::face::LBPHFaceRecognizer::create(1, 8, 8, 8, 80.0);
#else
    qWarning() << "OpenCV face 模块未找到，将使用深度特征提取方法";
    m_recognizer = nullptr;
#endif
    
    // 尝试加载 DNN 模型（如果可用）
    // 注意：需要下载预训练的人脸识别模型
    // 这里先检查，实际使用时需要提供模型文件
    m_dnnAvailable = false;  // 默认不使用 DNN，使用改进的传统方法
    
    // 加载人脸检测器（Haar Cascade）
    QString cascadePath = "haarcascade_frontalface_alt.xml"; // 人脸检测器路径（文件必须存在）
    bool cascadeLoaded = false; // 人脸检测器是否加载成功
    
    if (QFile::exists(cascadePath)) { // 如果文件存在，则加载人脸检测器
        std::string cascadePathStd = cascadePath.toStdString(); // 将路径转换为标准字符串
        if (m_faceCascade.load(cascadePathStd)) {
            qDebug() << "人脸检测器加载成功：" << cascadePath;
            cascadeLoaded = true;
        }
    }
    
    if (!cascadeLoaded) { // 如果人脸检测器未加载成功，则尝试其他可能的位置
        // 尝试其他可能的位置
        QStringList possiblePaths = {
            "data/haarcascade_frontalface_alt.xml",
            "../data/haarcascade_frontalface_alt.xml",
            "../../data/haarcascade_frontalface_alt.xml",
            // 尝试从 OpenCV 安装目录查找（已验证存在）
            "E:/share/opencv452/etc/haarcascades/haarcascade_frontalface_alt.xml",
            "E:/share/opencv452/data/haarcascades/haarcascade_frontalface_alt.xml",
            "E:/share/opencv452/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml",
            "E:/share/opencv452/build/etc/haarcascades/haarcascade_frontalface_alt.xml",
            "E:/share/opencv452/build/data/haarcascades/haarcascade_frontalface_alt.xml",
            // 尝试从环境变量获取的 OpenCV 路径
            qEnvironmentVariable("OpenCV_DIR", "") + "/etc/haarcascades/haarcascade_frontalface_alt.xml",
            qEnvironmentVariable("OpenCV_DIR", "") + "/data/haarcascades/haarcascade_frontalface_alt.xml",
            qEnvironmentVariable("OpenCV_DIR", "") + "/share/opencv4/haarcascades/haarcascade_frontalface_alt.xml"
        };
        
        for (const QString &path : possiblePaths) {
            if (!path.isEmpty() && QFile::exists(path)) {
                std::string pathStd = path.toStdString();
                if (m_faceCascade.load(pathStd)) {
                    qDebug() << "人脸检测器加载成功：" << path;
                    cascadeLoaded = true;
                break;
                }
            }
            }
        }
        
    if (!cascadeLoaded) {
            qWarning() << "Haar Cascade文件未找到，人脸检测功能可能不可用";
            qWarning() << "请从OpenCV安装目录复制haarcascade_frontalface_alt.xml到项目根目录";
        qWarning() << "或从以下位置下载：https://github.com/opencv/opencv/tree/master/data/haarcascades";
        // 不返回整个图像，而是返回 false，让调用者知道检测失败
    }
}

OpenCVFaceRecognizer::~OpenCVFaceRecognizer()
{
    // 如果识别器已初始化，则保存模型
    if (m_initialized) {
        saveModel(); // 保存模型
    }
}

cv::Mat OpenCVFaceRecognizer::qImageToMat(const QImage &image)
{
    QImage img = image.convertToFormat(QImage::Format_RGB888); // 将QImage转换为cv::Mat
    return cv::Mat(img.height(), img.width(), CV_8UC3, 
                   const_cast<uchar*>(img.bits()), 
                   img.bytesPerLine()).clone(); // 返回cv::Mat
}

QImage OpenCVFaceRecognizer::matToQImage(const cv::Mat &mat)
{
    if (mat.type() == CV_8UC1) { // 如果图像是灰度图
        // 灰度图
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) { // 如果图像是RGB图
        // RGB图
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
    }
    return QImage();
}

cv::Mat OpenCVFaceRecognizer::preprocessImage(const cv::Mat &image)
{
    cv::Mat processed; // 处理后的图像
    
    // 转换为灰度图
    if (image.channels() == 3) { // 如果图像是RGB图
        cv::cvtColor(image, processed, cv::COLOR_BGR2GRAY);
    } else {
        processed = image.clone(); // 如果图像是灰度图，则直接复制
    }
    
    // 直方图均衡化（减少光照影响）
    cv::equalizeHist(processed, processed); // 直方图均衡化
    
    // 尺寸归一化（固定尺寸，提高识别精度）
    // 使用更大的尺寸（256x256）以提高识别精度
    cv::resize(processed, processed, cv::Size(256, 256), 0, 0, cv::INTER_LINEAR); // 尺寸归一化
    
    // 可选：应用高斯模糊以减少噪声（轻微模糊，不影响特征）
    cv::GaussianBlur(processed, processed, cv::Size(3, 3), 0); // 应用高斯模糊
    
    return processed; // 返回处理后的图像
}

bool OpenCVFaceRecognizer::detectFace(const QImage &image, QRect &faceRect)
{
    if (m_faceCascade.empty()) {
        // 如果没有加载人脸检测器，无法进行人脸检测
        qWarning() << "人脸检测器未加载，无法检测人脸";
        return false;
    }
    
    cv::Mat mat = qImageToMat(image); // 将QImage转换为cv::Mat
    cv::Mat gray; // 灰度图
    
    if (mat.channels() == 3) {
        cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY); // 将RGB图转换为灰度图
    } else {
        gray = mat.clone(); // 如果图像是灰度图，则直接复制
    }
    
    // 直方图均衡化（提高对比度，减少光照影响）
    cv::equalizeHist(gray, gray); // 直方图均衡化
    
    // 优化的人脸检测参数（平衡速度和精度）：
    // scaleFactor=1.1: 适中的缩放步长，提高检测速度
    // minNeighbors=3: 适中的最小邻居数，平衡检测速度和准确性
    // flags=0: 使用默认标志
    // minSize: 根据图像大小动态调整最小人脸尺寸
    int minSize = std::max(30, std::min(image.width(), image.height()) / 10);
    std::vector<cv::Rect> faces;
    m_faceCascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(minSize, minSize));
    
    if (faces.empty()) {
        // 如果第一次检测失败，尝试更宽松的参数
        m_faceCascade.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(20, 20));
        if (faces.empty()) {
            qDebug() << "未检测到人脸，请确保人脸清晰可见且未被遮挡";
        return false;
        }
    }
    
    qDebug() << "检测到" << faces.size() << "个人脸";
    
    // 返回最大的人脸（通常是最主要的人脸）
    cv::Rect largestFace = faces[0];
    for (size_t i = 1; i < faces.size(); ++i) {
        if (faces[i].area() > largestFace.area()) {
            largestFace = faces[i];
        }
    }
    
    // 扩展人脸区域（增加一些边界，确保包含完整人脸）
    int expandX = largestFace.width / 10;
    int expandY = largestFace.height / 10;
    largestFace.x = std::max(0, largestFace.x - expandX);
    largestFace.y = std::max(0, largestFace.y - expandY);
    largestFace.width = std::min(mat.cols - largestFace.x, largestFace.width + 2 * expandX);
    largestFace.height = std::min(mat.rows - largestFace.y, largestFace.height + 2 * expandY);
    
    faceRect = QRect(largestFace.x, largestFace.y, largestFace.width, largestFace.height);
    qDebug() << "人脸区域：" << faceRect.x() << "," << faceRect.y() << "," << faceRect.width() << "x" << faceRect.height();
    return true;
}

bool OpenCVFaceRecognizer::train(const QString &username, const QImage &faceImage)
{
    if (faceImage.isNull()) { // 如果图像为空，则返回false
        return false;
    }
    
    // 检测人脸
    QRect faceRect;
    if (!detectFace(faceImage, faceRect)) { // 如果未检测到人脸，则返回false
        qWarning() << "未检测到人脸";
        return false;
    }
    
    // 提取人脸区域
    QImage faceRegion = faceImage.copy(faceRect); // 复制人脸区域
    cv::Mat faceMat = qImageToMat(faceRegion);
    
    // 验证提取的人脸区域是否有效
    if (faceMat.empty() || faceMat.cols <= 0 || faceMat.rows <= 0) { // 如果提取的人脸区域无效，则返回false
        qWarning() << "提取的人脸区域无效";
        return false;
    }
    
    // 质量评估
    double quality = assessFaceQuality(faceMat);
    if (quality < 0.5) {
        qWarning() << "人脸质量不足，质量分数：" << quality;
        // 不阻止训练，但记录警告
    }
    
    // 遮挡检测（在原始图像上检测）
    cv::Mat fullImage = qImageToMat(faceImage); // 将QImage转换为cv::Mat
    cv::Rect cvFaceRect(faceRect.x(), faceRect.y(), faceRect.width(), faceRect.height()); // 人脸区域矩形
    double occlusionRatio = detectOcclusion(fullImage, cvFaceRect); // 检测遮挡
    if (occlusionRatio > 0.10) {  // 遮挡超过10%，拒绝训练
        qWarning() << "人脸遮挡过多，无法录入，遮挡比例：" << (occlusionRatio * 100) << "%";
        return false;
    }
    
    // 人脸对齐（提高识别准确性）
    // 传入完整的人脸区域矩形（相对于 faceMat，即整个区域）
    cv::Rect fullRect(0, 0, faceMat.cols, faceMat.rows); // 完整人脸区域矩形
    cv::Mat alignedFace = alignFace(faceMat, fullRect); // 对齐人脸
    
    // 验证对齐后的人脸是否有效
    if (alignedFace.empty()) { // 如果对齐后的人脸无效，则返回false
        qWarning() << "人脸对齐失败";
        return false;
    }
    
    // 提取深度特征
    cv::Mat features = extractDeepFeatures(alignedFace);
    if (features.empty()) {
        qWarning() << "特征提取失败";
        return false;
    }
    
    // 归一化特征向量（L2归一化，提高匹配准确性）
    cv::Mat normalizedFeatures; // 归一化后的特征向量
    if (features.empty()) { // 如果特征为空，则返回false
        qWarning() << "特征为空，无法归一化";
        return false;
    }
    
    try {
        cv::normalize(features, normalizedFeatures, 1.0, 0.0, cv::NORM_L2); // 归一化特征向量
        if (normalizedFeatures.empty()) { // 如果归一化后的特征为空，则返回false
            qWarning() << "归一化后的特征为空";
            return false;
        }
    } catch (const cv::Exception &e) {
        qWarning() << "特征归一化失败：" << e.what(); // 特征归一化失败
        return false;
    }
    
    // 检查用户是否已存在
    int userIndex = m_trainedUsers.indexOf(username);
    if (userIndex >= 0) {
        // 用户已存在，更新特征（使用平均特征以提高鲁棒性）
        if (userIndex < m_faceFeatures.size()) {
            cv::Mat oldFeatures = m_faceFeatures[userIndex];
            if (!oldFeatures.empty() && oldFeatures.size() == normalizedFeatures.size()) {
                try {
                    // 计算平均特征（可以存储多个样本的平均值）
                    cv::Mat avgFeatures = (oldFeatures + normalizedFeatures) * 0.5;
                    cv::normalize(avgFeatures, m_faceFeatures[userIndex], 1.0, 0.0, cv::NORM_L2);
                    qDebug() << "更新用户特征（多样本融合）：" << username;
                } catch (const cv::Exception &e) {
                    qWarning() << "特征融合失败：" << e.what();
                    // 如果融合失败，直接替换
                    m_faceFeatures[userIndex] = normalizedFeatures.clone();
                }
    } else {
                // 如果旧特征无效，直接替换
                m_faceFeatures[userIndex] = normalizedFeatures.clone();
            }
        }
    } else {
        // 新用户，添加特征
        m_trainedUsers.append(username);
        m_faceFeatures.append(normalizedFeatures.clone());
        int label = m_trainedUsers.size() - 1;
        m_labels.append(label);
        qDebug() << "添加新用户：" << username << "，标签：" << label;
    }
    
    // 同时使用 LBPH 作为后备
#ifdef OPENCV_FACE_AVAILABLE
    if (m_recognizer) { // 如果识别器已初始化，则更新识别器
        cv::Mat processed = preprocessImage(alignedFace);
    std::vector<cv::Mat> images; // 图像列表
    std::vector<int> labels; // 标签列表
    
        int label = m_trainedUsers.indexOf(username); // 获取用户标签
    images.push_back(processed);
    labels.push_back(label); // 添加标签
    
    try {
        if (m_initialized) { // 如果识别器已初始化，则更新识别器
            m_recognizer->update(images, labels);
        } else {
            m_recognizer->train(images, labels); // 训练识别器
            m_initialized = true;
        }
        } catch (const cv::Exception &e) {
            qWarning() << "LBPH 训练失败：" << e.what(); // LBPH 训练失败
        }
    }
#endif
        
        // 保存模型
        saveModel();
        
    qDebug() << "训练成功，用户：" << username;
        return true;
}

QString OpenCVFaceRecognizer::recognize(const QImage &image, double &confidence)
{
        confidence = 0.0; // 置信度为0
    
    if (image.isNull() || m_trainedUsers.isEmpty() || m_faceFeatures.isEmpty()) { // 如果图像为空，则返回空字符串
        return QString();
    }
    
    // 检测人脸
    QRect faceRect;
    if (!detectFace(image, faceRect)) { // 如果未检测到人脸，则返回空字符串
        return QString();
    }
    
    // 提取人脸区域
    QImage faceRegion = image.copy(faceRect); // 复制人脸区域
    cv::Mat faceMat = qImageToMat(faceRegion);
    
    // 验证提取的人脸区域是否有效
    if (faceMat.empty() || faceMat.cols <= 0 || faceMat.rows <= 0) { // 如果提取的人脸区域无效，则返回空字符串
        return QString();
    }
    
    // 质量评估
    double quality = assessFaceQuality(faceMat);
    if (quality < 0.3) {
        qDebug() << "人脸质量过低，质量分数：" << quality;
        return QString(); // 人脸质量过低，返回空字符串
    }
    
    // 遮挡检测（在原始图像上检测，使用完整的人脸区域）
    cv::Mat fullImage = qImageToMat(image); // 将QImage转换为cv::Mat
    cv::Rect cvFaceRect(faceRect.x(), faceRect.y(), faceRect.width(), faceRect.height()); // 人脸区域矩形
    double occlusionRatio = detectOcclusion(fullImage, cvFaceRect); // 检测遮挡
    if (occlusionRatio > 0.10) {  // 遮挡超过10%，拒绝识别
        qDebug() << "人脸遮挡过多，遮挡比例：" << (occlusionRatio * 100) << "%";
        return QString(); // 遮挡过多，返回空字符串
    }
    
    // 人脸对齐
    // 传入完整的人脸区域矩形（相对于 faceMat，即整个区域）
    cv::Rect fullRect(0, 0, faceMat.cols, faceMat.rows); // 完整人脸区域矩形
    cv::Mat alignedFace = alignFace(faceMat, fullRect); // 对齐人脸
    
    // 验证对齐后的人脸是否有效
    if (alignedFace.empty()) { // 如果对齐后的人脸无效，则返回空字符串
        qDebug() << "人脸对齐失败";
        return QString(); // 人脸对齐失败，返回空字符串
    }
    
    // 提取深度特征
    cv::Mat queryFeatures = extractDeepFeatures(alignedFace); // 提取深度特征
    if (queryFeatures.empty()) { // 如果提取的深度特征为空，则返回空字符串
        return QString();
    }
    
    // 归一化特征向量
    cv::Mat normalizedQuery; // 归一化后的特征向量
    if (queryFeatures.empty()) {
        qDebug() << "查询特征为空";
        return QString(); // 查询特征为空，返回空字符串
    }
    
    try {
        cv::normalize(queryFeatures, normalizedQuery, 1.0, 0.0, cv::NORM_L2); // 归一化特征向量
        if (normalizedQuery.empty()) {
            qDebug() << "归一化后的查询特征为空";
            return QString(); // 归一化后的查询特征为空，返回空字符串
        }
    } catch (const cv::Exception &e) {
        qWarning() << "查询特征归一化失败：" << e.what();
        return QString(); // 查询特征归一化失败，返回空字符串
    }
    
    // 与所有已训练的特征进行比较（使用余弦相似度）
    double bestSimilarity = 0.0; // 最佳相似度
    int bestIndex = -1; // 最佳索引
    
    for (int i = 0; i < m_faceFeatures.size(); ++i) {
        double similarity = calculateSimilarity(normalizedQuery, m_faceFeatures[i]); // 计算相似度
        
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity; // 更新最佳相似度
            bestIndex = i;
        }
    }
    
    // 设置阈值：0.65 是一个平衡点，既能识别正确的人脸，又能减少误识别
    const double SIMILARITY_THRESHOLD = 0.65;
    
    if (bestIndex >= 0 && bestSimilarity >= SIMILARITY_THRESHOLD) {
        confidence = bestSimilarity; // 更新置信度
        qDebug() << "智能识别成功 - 用户：" << m_trainedUsers[bestIndex] 
                 << "，相似度：" << bestSimilarity << "，质量：" << quality;
        return m_trainedUsers[bestIndex]; // 返回用户名
    } else if (bestIndex >= 0) {
        qDebug() << "相似度不足 - 用户：" << m_trainedUsers[bestIndex] 
                 << "，相似度：" << bestSimilarity << "（阈值：" << SIMILARITY_THRESHOLD << "）";
    } // 相似度不足，返回空字符串
    
    // 如果深度特征匹配失败，尝试使用 LBPH 作为后备
#ifdef OPENCV_FACE_AVAILABLE
    if (m_recognizer && m_initialized) { // 如果识别器已初始化，则更新识别器
        cv::Mat processed = preprocessImage(alignedFace);
    int predictedLabel = -1; // 预测标签
    double predictedConfidence = 0.0; // 预测置信度
    
    try {
        m_recognizer->predict(processed, predictedLabel, predictedConfidence); // 预测标签和置信度
            
            if (predictedLabel >= 0 && predictedLabel < m_trainedUsers.size() && predictedConfidence < 100.0) {
                confidence = 1.0 - (predictedConfidence / 120.0); // 更新置信度
            if (confidence < 0) confidence = 0; // 置信度小于0，则置为0
            if (confidence > 1) confidence = 1; // 置信度大于1，则置为1
            
                if (confidence >= 0.6) {
                    qDebug() << "LBPH 后备识别成功 - 用户：" << m_trainedUsers[predictedLabel] 
                             << "，相似度：" << confidence;
            return m_trainedUsers[predictedLabel]; // 返回用户名
                }
        }
    } catch (const cv::Exception &e) {
            qWarning() << "LBPH 识别失败：" << e.what(); // LBPH 识别失败
    }
    }
#endif
    
    return QString(); // 返回空字符串
}

bool OpenCVFaceRecognizer::saveModel()
{
    // 保存深度特征模型（主要方法）
        QJsonObject root;
        QJsonArray usersArray;
        QJsonArray labelsArray;
        QJsonArray featuresArray;
        
        for (int i = 0; i < m_trainedUsers.size(); ++i) {
            usersArray.append(m_trainedUsers[i]); // 添加用户名
            labelsArray.append(m_labels[i]); // 添加标签
        
            // 保存深度特征向量
            if (i < m_faceFeatures.size() && !m_faceFeatures[i].empty()) {
                QJsonArray featureArray;
                cv::Mat features = m_faceFeatures[i]; // 获取深度特征
                for (int j = 0; j < features.cols; ++j) {
                    featureArray.append(static_cast<double>(features.at<float>(0, j))); // 添加深度特征
                }
                featuresArray.append(featureArray); // 添加深度特征
            } else {
                featuresArray.append(QJsonArray()); // 添加空深度特征
            }
        }
        
        root["users"] = usersArray; // 添加用户名
        root["labels"] = labelsArray; // 添加标签
        root["features"] = featuresArray; // 添加深度特征
        root["method"] = "deep_features";  // 标记使用深度特征方法
        
        QJsonDocument doc(root);
        QFile file(m_modelPath + "_deep.json"); // 深度特征模型文件路径
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
            qDebug() << "深度特征模型保存成功：" << m_modelPath + "_deep.json";
        } else {
            qWarning() << "无法保存深度特征模型文件";
            return false;
        }
    
    // 同时保存 LBPH 模型（如果可用，作为后备）
#ifdef OPENCV_FACE_AVAILABLE
    if (m_recognizer && m_initialized) { // 如果识别器已初始化，则保存识别器
        try {
            std::string modelFile = (m_modelPath + ".xml").toStdString(); // 识别器文件路径
            m_recognizer->write(modelFile); // 保存识别器
            
            // 保存用户标签映射（用于 LBPH）
            QJsonObject lbphRoot;
            lbphRoot["users"] = usersArray; // 添加用户名
            lbphRoot["labels"] = labelsArray; // 添加标签
            
            QJsonDocument lbphDoc(lbphRoot);
            QFile lbphFile(m_modelPath + "_mapping.json"); // LBPH 模型文件路径
                if (lbphFile.open(QIODevice::WriteOnly)) {
                    lbphFile.write(lbphDoc.toJson()); // 保存 LBPH 模型
                    lbphFile.close();
            }
        } catch (const cv::Exception &e) {
                qWarning() << "保存 LBPH 模型失败：" << e.what(); // LBPH 模型保存失败
        }
    }
#endif
    
    return true;
}

bool OpenCVFaceRecognizer::loadModel()
{
    // 优先加载深度特征模型（新方法）
    QString deepModelFile = m_modelPath + "_deep.json"; // 深度特征模型文件路径
    if (QFile::exists(deepModelFile)) {
        QFile file(deepModelFile); // 深度特征模型文件
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取深度特征模型文件
            file.close(); // 关闭深度特征模型文件
            
            QJsonObject root = doc.object(); // 获取深度特征模型文件对象
            QJsonArray usersArray = root["users"].toArray(); // 获取用户名数组
            QJsonArray labelsArray = root["labels"].toArray(); // 获取标签数组
            QJsonArray featuresArray = root["features"].toArray(); // 获取深度特征数组
            
            m_trainedUsers.clear(); // 清空用户名
            m_labels.clear(); // 清空标签
            m_faceFeatures.clear(); // 清空深度特征
            
            int count = qMin(usersArray.size(), qMin(labelsArray.size(), featuresArray.size()));
            for (int i = 0; i < count; ++i) {
                m_trainedUsers.append(usersArray[i].toString()); // 添加用户名
                m_labels.append(labelsArray[i].toInt()); // 添加标签
                
                // 加载深度特征向量
                QJsonArray featureArray = featuresArray[i].toArray();
                if (!featureArray.isEmpty()) { // 如果深度特征数组不为空，则加载深度特征
                    cv::Mat features(1, featureArray.size(), CV_32F); // 创建深度特征矩阵
                    for (int j = 0; j < featureArray.size(); ++j) {
                        features.at<float>(0, j) = static_cast<float>(featureArray[j].toDouble()); // 添加深度特征
                    }
                    cv::normalize(features, features, 1.0, 0.0, cv::NORM_L2); // 归一化深度特征
                    m_faceFeatures.append(features); // 添加深度特征
                } else {
                    m_faceFeatures.append(cv::Mat()); // 添加空深度特征
    }
            }
            m_initialized = true; // 初始化识别器
            qDebug() << "深度特征模型加载成功，已训练用户数：" << m_trainedUsers.size(); // 深度特征模型加载成功，已训练用户数
            return true;
        }
    }
    
    // 如果没有深度特征模型，尝试加载旧的 LBPH 模型（向后兼容）
#ifdef OPENCV_FACE_AVAILABLE
    if (m_recognizer) {
    QString modelFile = m_modelPath + ".xml"; // LBPH 模型文件路径
        if (QFile::exists(modelFile)) {
            try {
                std::string modelFileStd = modelFile.toStdString(); // 将路径转换为标准字符串
                m_recognizer->read(modelFileStd); // 读取 LBPH 模型
                
                QFile mappingFile(m_modelPath + "_mapping.json"); // LBPH 模型文件路径
                if (mappingFile.open(QIODevice::ReadOnly)) {
                    QJsonDocument doc = QJsonDocument::fromJson(mappingFile.readAll()); // 读取 LBPH 模型文件
                    mappingFile.close(); // 关闭 LBPH 模型文件
                    
                    QJsonObject root = doc.object(); // 获取 LBPH 模型文件对象
                    QJsonArray usersArray = root["users"].toArray(); // 获取用户名数组
                    QJsonArray labelsArray = root["labels"].toArray(); // 获取标签数组
                    
                    m_trainedUsers.clear(); // 清空用户名
                    m_labels.clear(); // 清空标签
                            m_faceFeatures.clear(); // 清空深度特征
                    
                    for (int i = 0; i < usersArray.size() && i < labelsArray.size(); ++i) {
                        m_trainedUsers.append(usersArray[i].toString()); // 添加用户名
                        m_labels.append(labelsArray[i].toInt()); // 添加标签
                                m_faceFeatures.append(cv::Mat());  // LBPH 模型不存储深度特征
                    } // 添加空深度特征
                    
                    m_initialized = true; // 初始化识别器
                            qDebug() << "LBPH 模型加载成功（向后兼容），已训练用户数：" << m_trainedUsers.size();
                    return true; // 返回成功
                }
            } catch (const cv::Exception &e) {
                        qWarning() << "加载 LBPH 模型失败：" << e.what();
            }
        }
    }
#endif
    
    return false;
}

bool OpenCVFaceRecognizer::initialize()
{
    // 尝试加载已保存的模型
    if (loadModel()) {
        return true;
    }
    
    // 如果没有保存的模型，从user.json加载已录入的人脸并训练
    QFile userFile("user.json"); // 用户文件路径
    if (!userFile.exists() || !userFile.open(QIODevice::ReadOnly)) { // 如果用户文件不存在或无法打开，则返回失败
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(userFile.readAll()); // 读取用户文件
    userFile.close(); // 关闭用户文件
    
    if (!doc.isObject()) {
        return false; // 返回失败
    }
    
    QJsonObject users = doc.object(); // 获取用户对象
    bool hasData = false;
    
    for (auto it = users.begin(); it != users.end(); ++it) { // 遍历用户对象
        QString username = it.key(); // 获取用户名
        QJsonObject userObj = it.value().toObject(); // 获取用户对象
        QString faceImagePath = userObj["faceImagePath"].toString(); // 获取人脸图像路径
        
        if (faceImagePath.isEmpty() || !QFile::exists(faceImagePath)) { // 如果人脸图像路径为空或不存在，则跳过
            continue;
        }
        
        QImage faceImage(faceImagePath); // 创建人脸图像
        if (!faceImage.isNull()) {
            if (train(username, faceImage)) { // 如果训练成功，则设置为true
                hasData = true;
            }
        }
    }
    
    return hasData;
}

// 人脸对齐（基于关键点检测，提高识别准确性）
cv::Mat OpenCVFaceRecognizer::alignFace(const cv::Mat &image, const cv::Rect &faceRect)
{
    if (image.empty() || faceRect.width <= 0 || faceRect.height <= 0) {
        // 如果输入无效，直接返回归一化的原图
        cv::Mat normalized;
        cv::resize(image, normalized, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
        return normalized; // 返回归一化的图像
    }
    
    cv::Mat aligned = image.clone(); // 复制图像
    
    // 如果图像太小，先放大
    if (faceRect.width < 100 || faceRect.height < 100) { // 如果图像宽度或高度小于100，则放大图像
        double scale = 200.0 / std::max(faceRect.width, faceRect.height); // 计算缩放比例
        cv::resize(aligned, aligned, cv::Size(), scale, scale, cv::INTER_LINEAR); // 缩放图像
    }
    
    // 提取人脸区域（带边界扩展，确保不越界）
    int expand = std::min(faceRect.width, faceRect.height) / 4; // 计算扩展值
    expand = std::max(0, expand);  // 确保 expand 不为负
    
    // 计算扩展后的矩形，确保不超出图像边界
    int x = std::max(0, faceRect.x - expand);
    int y = std::max(0, faceRect.y - expand);
    int width = std::min(aligned.cols - x, faceRect.width + 2 * expand);
    int height = std::min(aligned.rows - y, faceRect.height + 2 * expand);
    
    // 确保宽度和高度为正
    if (width <= 0 || height <= 0) {
        // 如果计算出的区域无效，使用原始人脸区域
        x = std::max(0, std::min(faceRect.x, aligned.cols - 1));
        y = std::max(0, std::min(faceRect.y, aligned.rows - 1));
        width = std::min(aligned.cols - x, faceRect.width);
        height = std::min(aligned.rows - y, faceRect.height);
    }
    
    cv::Rect expandedRect(x, y, width, height); // 创建扩展后的矩形
    
    // 再次验证矩形是否有效
    if (expandedRect.x < 0 || expandedRect.y < 0 || 
        expandedRect.x + expandedRect.width > aligned.cols ||
        expandedRect.y + expandedRect.height > aligned.rows ||
        expandedRect.width <= 0 || expandedRect.height <= 0) {
        // 如果仍然无效，使用整个图像
        qWarning() << "人脸区域计算无效，使用整个图像";
        expandedRect = cv::Rect(0, 0, aligned.cols, aligned.rows);
    }
    
    cv::Mat faceRegion = aligned(expandedRect); // 提取人脸区域
    
    // 归一化到固定尺寸（112x112 是现代人脸识别的标准尺寸）
    cv::Mat normalized;
    if (faceRegion.empty()) {
        // 如果提取失败，使用整个图像
        cv::resize(aligned, normalized, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
    } else { 
        // 如果提取成功，则缩放人脸区域
        cv::resize(faceRegion, normalized, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
    }
    
    return normalized; // 返回归一化后的图像
}

// 提取深度特征（使用改进的特征提取方法）
cv::Mat OpenCVFaceRecognizer::extractDeepFeatures(const cv::Mat &alignedFace)
{
    if (alignedFace.empty()) { // 如果对齐后的人脸图像为空，则返回空矩阵
        qWarning() << "对齐后的人脸图像为空";
        return cv::Mat();
    }
    
    cv::Mat features; // 特征向量
    
    if (m_dnnAvailable && !m_faceNet.empty()) {
        // 使用 DNN 模型提取特征（如果可用）
        // 这里需要预训练的模型文件
        // 暂时不使用 DNN
    }
    
    // 使用改进的传统特征提取方法
    // 1. 转换为灰度图
    cv::Mat gray;
    try {
        if (alignedFace.channels() == 3) {
            cv::cvtColor(alignedFace, gray, cv::COLOR_BGR2GRAY); // 转换为灰度图
        } else {
            gray = alignedFace.clone(); // 如果图像是灰度图，则直接复制
        }
        
        if (gray.empty()) { // 如果灰度图图像为空，则返回空矩阵
            qWarning() << "灰度转换失败";
            return cv::Mat();
        }
    } catch (const cv::Exception &e) {
        qWarning() << "灰度转换异常：" << e.what();
        return cv::Mat();
    }
    
    // 2. 直方图均衡化
    try {
        cv::equalizeHist(gray, gray);
    } catch (const cv::Exception &e) {
        qWarning() << "直方图均衡化异常：" << e.what();
        // 继续处理，不返回
    }
    
    // 3. 提取 LBP 特征（局部二值模式）
    cv::Mat lbp;
    cv::Mat lbpFeatures;
    
    // 简化的 LBP 特征提取
    int radius = 1;
    int neighbors = 8;
    int histSize = 256;
    
    // 计算 LBP
    lbp = cv::Mat::zeros(gray.size(), CV_8UC1);
    for (int i = radius; i < gray.rows - radius; i++) {
        for (int j = radius; j < gray.cols - radius; j++) {
            uchar center = gray.at<uchar>(i, j);
            uchar code = 0;
            for (int k = 0; k < neighbors; k++) {
                double angle = 2.0 * M_PI * k / neighbors;
                int x = static_cast<int>(j + radius * cos(angle));
                int y = static_cast<int>(i - radius * sin(angle));
                if (x >= 0 && x < gray.cols && y >= 0 && y < gray.rows) {
                    if (gray.at<uchar>(y, x) >= center) {
                        code |= (1 << k);
                    }
                }
            }
            lbp.at<uchar>(i, j) = code;
        }
    }
    
    // 计算直方图作为特征
    std::vector<cv::Mat> planes = {lbp};
    cv::Mat hist;
    std::vector<int> histSizeVec = {histSize};
    std::vector<float> rangeVec = {0.0f, 256.0f};
    std::vector<int> channelsVec = {0};
    
    // 使用正确的 calcHist 重载版本
    cv::calcHist(planes, channelsVec, cv::Mat(), hist, histSizeVec, rangeVec, false);
    
    // 归一化直方图
    if (hist.empty()) {
        qWarning() << "直方图为空";
        return cv::Mat();
    }
    
    cv::normalize(hist, hist, 1.0, 0.0, cv::NORM_L1);
    
    // 转换为特征向量
    cv::Mat histReshaped = hist.reshape(1, 1);
    if (histReshaped.empty()) {
        qWarning() << "直方图重塑失败";
        return cv::Mat();
    }
    
    histReshaped.convertTo(features, CV_32F);
    if (features.empty()) {
        qWarning() << "特征转换失败";
        return cv::Mat();
    }
    
    // 4. 添加 HOG 特征（方向梯度直方图）以提高鲁棒性
    try {
        cv::HOGDescriptor hog(cv::Size(112, 112), cv::Size(16, 16), cv::Size(8, 8), cv::Size(8, 8), 9);
        std::vector<float> hogFeatures;
        hog.compute(gray, hogFeatures);
        
        if (hogFeatures.empty()) {
            qWarning() << "HOG 特征提取失败，仅使用 LBP 特征";
            return features;
        }
        
        // 合并特征
        cv::Mat hogMat(hogFeatures, true);
        if (hogMat.empty()) {
            qWarning() << "HOG 矩阵创建失败，仅使用 LBP 特征";
            return features;
        }
        
        hogMat = hogMat.reshape(1, 1);
        hogMat.convertTo(hogMat, CV_32F);
        
        if (hogMat.empty() || hogMat.cols <= 0) {
            qWarning() << "HOG 矩阵转换失败，仅使用 LBP 特征";
            return features;
        }
        
        // 验证两个矩阵是否兼容
        if (features.rows == hogMat.rows) {
            cv::hconcat(features, hogMat, features);
        } else {
            qWarning() << "特征矩阵维度不匹配，仅使用 LBP 特征";
            return features;
        }
    } catch (const cv::Exception &e) {
        qWarning() << "HOG 特征提取异常：" << e.what() << "，仅使用 LBP 特征";
        return features;
    }
    
    return features;
}

// 计算特征相似度（余弦相似度）
double OpenCVFaceRecognizer::calculateSimilarity(const cv::Mat &features1, const cv::Mat &features2)
{
    if (features1.empty() || features2.empty()) {
        qDebug() << "特征矩阵为空";
        return 0.0;
    }
    
    if (features1.size() != features2.size()) {
        qDebug() << "特征矩阵尺寸不匹配：" 
                 << features1.cols << "x" << features1.rows 
                 << " vs " 
                 << features2.cols << "x" << features2.rows;
        return 0.0;
    }
    
    if (features1.type() != features2.type()) {
        qDebug() << "特征矩阵类型不匹配：" << features1.type() << " vs " << features2.type();
        return 0.0;
    }
    
    try {
        // 计算点积（余弦相似度，因为特征已经归一化）
        cv::Mat mulResult = features1.mul(features2);
        if (mulResult.empty()) {
            qDebug() << "矩阵乘法结果为空";
            return 0.0;
        }
        
        cv::Scalar sumResult = cv::sum(mulResult);
        double dotProduct = sumResult[0];
        
        // 由于特征已归一化，点积就是余弦相似度
        return std::max(0.0, std::min(1.0, dotProduct));
    } catch (const cv::Exception &e) {
        qWarning() << "相似度计算异常：" << e.what();
        return 0.0;
    }
}

// 质量评估
double OpenCVFaceRecognizer::assessFaceQuality(const cv::Mat &faceImage)
{
    if (faceImage.empty()) {
        return 0.0;
    }
    
    double quality = 1.0;
    
    // 1. 清晰度评估（使用拉普拉斯算子）
    cv::Mat gray, laplacian;
    if (faceImage.channels() == 3) {
        cv::cvtColor(faceImage, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = faceImage.clone();
    }
    
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    double sharpness = stddev.val[0] * stddev.val[0];
    
    // 归一化清晰度分数（经验值：>100 为清晰）
    double sharpnessScore = std::min(1.0, sharpness / 100.0);
    quality *= (0.3 + 0.7 * sharpnessScore);  // 清晰度占70%权重
    
    // 2. 亮度评估
    cv::Scalar meanBrightness = cv::mean(gray);
    double brightness = meanBrightness.val[0];
    
    // 理想亮度范围：100-180
    double brightnessScore = 1.0;
    if (brightness < 50 || brightness > 220) {
        brightnessScore = 0.3;  // 过暗或过亮
    } else if (brightness < 100 || brightness > 180) {
        brightnessScore = 0.7;  // 稍暗或稍亮
    }
    quality *= (0.2 + 0.8 * brightnessScore);  // 亮度占30%权重
    
    // 3. 尺寸评估
    double sizeScore = std::min(1.0, std::min(faceImage.cols, faceImage.rows) / 100.0);
    quality *= (0.1 + 0.9 * sizeScore);  // 尺寸占10%权重
    
    return std::max(0.0, std::min(1.0, quality));
}

// 遮挡检测（返回遮挡比例，0.0-1.0）
double OpenCVFaceRecognizer::detectOcclusion(const cv::Mat &image, const cv::Rect &faceRect)
{
    if (image.empty() || faceRect.width <= 0 || faceRect.height <= 0) {
        return 1.0;  // 如果无法检测，假设完全遮挡
    }
    
    // 确保人脸区域在图像范围内
    cv::Rect validRect = faceRect & cv::Rect(0, 0, image.cols, image.rows);
    if (validRect.width <= 0 || validRect.height <= 0) {
        return 1.0;
    }
    
    // 提取人脸区域
    cv::Mat faceRegion = image(validRect);
    
    // 转换为灰度图
    cv::Mat gray;
    if (faceRegion.channels() == 3) {
        cv::cvtColor(faceRegion, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = faceRegion.clone();
    }
    
    // 将人脸区域分成多个关键区域进行分析
    // 标准人脸区域划分：左眼、右眼、鼻子、嘴巴、左脸颊、右脸颊
    int w = gray.cols;
    int h = gray.rows;
    
    // 定义关键区域（基于标准人脸比例）
    // 上1/3: 眼睛区域
    // 中1/3: 鼻子区域
    // 下1/3: 嘴巴区域
    // 左右各1/3: 脸颊区域
    
    std::vector<cv::Rect> regions;
    regions.push_back(cv::Rect(w/6, h/6, w/3, h/3));      // 左眼区域
    regions.push_back(cv::Rect(w/2, h/6, w/3, h/3));      // 右眼区域
    regions.push_back(cv::Rect(w/3, h/3, w/3, h/3));      // 鼻子区域
    regions.push_back(cv::Rect(w/4, h*2/3, w/2, h/3));    // 嘴巴区域
    regions.push_back(cv::Rect(0, h/4, w/3, h/2));        // 左脸颊
    regions.push_back(cv::Rect(w*2/3, h/4, w/3, h/2));    // 右脸颊
    
    // 计算每个区域的纹理密度（使用拉普拉斯算子的方差）
    std::vector<double> regionVariances;
    double totalVariance = 0.0;
    
    for (const auto &region : regions) {
        // 确保区域在图像范围内
        cv::Rect validRegion = region & cv::Rect(0, 0, w, h);
        if (validRegion.width <= 0 || validRegion.height <= 0) {
            regionVariances.push_back(0.0);
            continue;
        }
        
        cv::Mat regionROI = gray(validRegion);
        
        // 计算拉普拉斯算子的方差（纹理密度指标）
        cv::Mat laplacian;
        cv::Laplacian(regionROI, laplacian, CV_64F);
        cv::Scalar mean, stddev;
        cv::meanStdDev(laplacian, mean, stddev);
        double variance = stddev.val[0] * stddev.val[0];
        
        regionVariances.push_back(variance);
        totalVariance += variance;
    }
    
    if (regionVariances.empty() || totalVariance == 0.0) {
        return 0.0;  // 无法计算，假设无遮挡
    }
    
    // 计算平均方差
    double avgVariance = totalVariance / regionVariances.size();
    
    // 检测被遮挡的区域（使用更严格的阈值，减少误判）
    // 只认为方差明显低于平均值（低于30%）的区域被遮挡，而不是50%
    int occludedRegions = 0;
    double occlusionThreshold = avgVariance * 0.3;  // 如果区域方差低于平均值的30%，认为被遮挡
    
    // 计算每个区域相对于平均值的比例
    std::vector<double> regionRatios;
    for (double variance : regionVariances) {
        double ratio = (avgVariance > 0) ? (variance / avgVariance) : 1.0;
        regionRatios.push_back(ratio);
        
        // 只有方差明显低于平均值时才认为被遮挡
        if (variance < occlusionThreshold && ratio < 0.3) {
            occludedRegions++;
        }
    }
    
    // 计算遮挡比例（只计算明显被遮挡的区域）
    double occlusionRatio = static_cast<double>(occludedRegions) / regionVariances.size();
    
    // 额外检查：对称性分析（左右脸颊的方差差异）
    // 使用更宽松的阈值，避免误判
    if (regionVariances.size() >= 6) {
        double leftCheekVariance = regionVariances[4];  // 左脸颊
        double rightCheekVariance = regionVariances[5]; // 右脸颊
        
        if (leftCheekVariance > 0 && rightCheekVariance > 0) {
            double symmetryRatio = std::min(leftCheekVariance, rightCheekVariance) / 
                                  std::max(leftCheekVariance, rightCheekVariance);
            
            // 如果对称性很差（比例 < 0.2），可能一侧被遮挡（更严格的阈值）
            if (symmetryRatio < 0.2) {
                occlusionRatio = std::max(occlusionRatio, 0.2);  // 至少20%遮挡
            }
        }
    }
    
    // 额外检查：眼睛区域检测（使用更严格的判断）
    if (regionVariances.size() >= 2) {
        double leftEyeVariance = regionVariances[0];
        double rightEyeVariance = regionVariances[1];
        
        // 计算眼睛区域相对于平均值的比例
        double leftEyeRatio = (avgVariance > 0) ? (leftEyeVariance / avgVariance) : 1.0;
        double rightEyeRatio = (avgVariance > 0) ? (rightEyeVariance / avgVariance) : 1.0;
        
        bool leftEyeOccluded = leftEyeVariance < occlusionThreshold && leftEyeRatio < 0.3;
        bool rightEyeOccluded = rightEyeVariance < occlusionThreshold && rightEyeRatio < 0.3;
        
        if (leftEyeOccluded && rightEyeOccluded) {
            occlusionRatio = std::max(occlusionRatio, 0.4);  // 双眼都被遮挡，至少40%遮挡
        } else if (leftEyeOccluded || rightEyeOccluded) {
            occlusionRatio = std::max(occlusionRatio, 0.15);  // 单眼被遮挡，至少15%遮挡
        }
    }
    
    // 如果遮挡比例很小（< 0.15），可能是误判，设置为0
    // 进一步优化：如果大部分区域（>= 5个）都是正常的，认为无遮挡
    int normalRegions = 0;
    for (double ratio : regionRatios) {
        if (ratio >= 0.5) {  // 区域方差至少是平均值的50%
            normalRegions++;
        }
    }
    
    // 如果至少有5个区域正常，认为无遮挡
    if (normalRegions >= 5) {
        occlusionRatio = 0.0;
    } else if (occlusionRatio < 0.15) {
        occlusionRatio = 0.0;
    }
    
    qDebug() << "遮挡检测 - 正常区域数：" << normalRegions << "/" << regionRatios.size() 
             << "，遮挡比例：" << (occlusionRatio * 100) << "%";
    
    return std::max(0.0, std::min(1.0, occlusionRatio));
}

bool OpenCVFaceRecognizer::initTracker(const QImage &image, const QRect &faceRect)
{
    // 初始化跟踪器（如果可用）
#ifdef OPENCV_TRACKING_AVAILABLE
    if (image.isNull() || faceRect.isEmpty()) { // 如果图像为空或人脸区域为空，则返回false
        return false;
    }
    
    cv::Mat frame = qImageToMat(image); // 将图像转换为OpenCV格式
    if (frame.empty()) {
        return false; // 如果图像为空，则返回false
    }
    
    // 转换为灰度图（Tracker 通常需要灰度图）
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY); // 转换为灰度图
    } else {
        gray = frame.clone(); // 如果图像是灰度图，则直接复制
    }
    
    // 创建 Tracker（使用 KCF 算法，性能好且准确）
    try {
        m_tracker = cv::TrackerKCF::create();
        
        // 将 QRect 转换为 cv::Rect2d
        cv::Rect2d roi(faceRect.x(), faceRect.y(), faceRect.width(), faceRect.height());
        
        // 确保 ROI 在图像范围内
        roi.x = std::max(0.0, std::min(roi.x, static_cast<double>(gray.cols - 1)));
        roi.y = std::max(0.0, std::min(roi.y, static_cast<double>(gray.rows - 1)));
        roi.width = std::min(roi.width, static_cast<double>(gray.cols - roi.x));
        roi.height = std::min(roi.height, static_cast<double>(gray.rows - roi.y));
        
        if (roi.width <= 0 || roi.height <= 0) {
            qWarning() << "Tracker 初始化失败：无效的 ROI";
            return false;
        }
        
        // 初始化跟踪器
        if (m_tracker->init(gray, roi)) {
            m_trackerInitialized = true;
            qDebug() << "Tracker 初始化成功，ROI:" << roi.x << "," << roi.y << "," << roi.width << "x" << roi.height;
            return true;
        } else {
            qWarning() << "Tracker 初始化失败";
            m_trackerInitialized = false;
            return false;
        }
    } catch (const cv::Exception &e) {
        qWarning() << "Tracker 初始化异常：" << e.what();
        m_trackerInitialized = false;
        return false;
    }
#else
    // 简单的跟踪器实现（基于位置预测）
    if (image.isNull() || faceRect.isEmpty()) {
        return false;
    }
    
    m_lastTrackedRect = faceRect;
    m_lastVelocity = QPoint(0, 0);
    m_trackFrameCount = 1;
    m_trackerInitialized = true;
    qDebug() << "简单跟踪器初始化成功，ROI:" << faceRect.x() << "," << faceRect.y() << "," << faceRect.width() << "x" << faceRect.height();
    return true;
#endif
}

bool OpenCVFaceRecognizer::updateTracker(const QImage &image, QRect &faceRect)
{
    // 更新跟踪器
#ifdef OPENCV_TRACKING_AVAILABLE
    if (!m_trackerInitialized || !m_tracker || image.isNull()) { // 如果跟踪器未初始化或跟踪器为空或图像为空，则返回false
        return false;
    }
    
    cv::Mat frame = qImageToMat(image);
    if (frame.empty()) {
        return false; // 如果图像为空，则返回false
    }
    
    // 转换为灰度图
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }
    
    try {
        cv::Rect2d roi; // 创建矩形区域
        
        // 更新跟踪器
        if (m_tracker->update(gray, roi)) {
            // 确保 ROI 在图像范围内
            roi.x = std::max(0.0, std::min(roi.x, static_cast<double>(gray.cols - 1)));
            roi.y = std::max(0.0, std::min(roi.y, static_cast<double>(gray.rows - 1)));
            roi.width = std::min(roi.width, static_cast<double>(gray.cols - roi.x));
            roi.height = std::min(roi.height, static_cast<double>(gray.rows - roi.y));
            
            if (roi.width > 0 && roi.height > 0) {
                // 将 cv::Rect2d 转换为 QRect
                faceRect = QRect(
                    static_cast<int>(roi.x),
                    static_cast<int>(roi.y),
                    static_cast<int>(roi.width),
                    static_cast<int>(roi.height)
                );
                return true;
            }
        }
        
        // 跟踪失败，重置跟踪器
        m_trackerInitialized = false;
        m_tracker.release();
        return false;
    } catch (const cv::Exception &e) { // 捕获异常
        qWarning() << "Tracker 更新异常：" << e.what();
        m_trackerInitialized = false;
        m_tracker.release();
        return false;
    }
#else
    // 简单的跟踪器实现（基于位置预测和局部搜索）
    if (!m_trackerInitialized || image.isNull()) {
        return false;
    }
    
    // 计算预测位置（基于上一帧的位置和速度）
    QPoint predictedCenter = m_lastTrackedRect.center() + m_lastVelocity;
    QRect predictedRect(
        predictedCenter.x() - m_lastTrackedRect.width() / 2,
        predictedCenter.y() - m_lastTrackedRect.height() / 2,
        m_lastTrackedRect.width(),
        m_lastTrackedRect.height()
    );
    
    // 在预测位置周围进行检测（扩大搜索范围以应对快速移动）
    // 根据速度动态调整搜索范围：速度越快，搜索范围越大
    int baseMargin = 80;  // 基础搜索范围（80像素）
    int speedFactor = qMax(qAbs(m_lastVelocity.x()), qAbs(m_lastVelocity.y()));
    int searchMargin = baseMargin + speedFactor / 2;  // 根据速度动态增加搜索范围
    searchMargin = qMin(searchMargin, 150);  // 限制最大搜索范围为150，避免性能问题
    
    QRect searchArea( // 创建搜索区域
        std::max(0, predictedRect.x() - searchMargin),
        std::max(0, predictedRect.y() - searchMargin),
        std::min(image.width() - std::max(0, predictedRect.x() - searchMargin), predictedRect.width() + 2 * searchMargin),
        std::min(image.height() - std::max(0, predictedRect.y() - searchMargin), predictedRect.height() + 2 * searchMargin)
    );
    
    // 裁剪搜索区域
    searchArea = searchArea.intersected(QRect(0, 0, image.width(), image.height()));
    
    if (searchArea.isEmpty() || searchArea.width() < 50 || searchArea.height() < 50) {
        // 搜索区域太小，重新检测(<50像素)
        m_trackerInitialized = false;
        return false;
    }
    
    // 在搜索区域内进行人脸检测
    QImage searchImage = image.copy(searchArea); // 复制搜索区域图像
    QRect detectedRect;
    bool found = detectFace(searchImage, detectedRect); // 检测人脸
    
    if (found && !detectedRect.isEmpty()) {
        // 将检测结果转换回原图坐标
        faceRect = QRect(
            searchArea.x() + detectedRect.x(),
            searchArea.y() + detectedRect.y(),
            detectedRect.width(),
            detectedRect.height()
        );
        
        // 更新速度和位置（使用平滑的速度计算，提高预测精度）
        QPoint currentCenter = faceRect.center();
        QPoint lastCenter = m_lastTrackedRect.center();
        QPoint instantVelocity = currentCenter - lastCenter;
        
        // 使用指数移动平均（EMA）平滑速度，减少抖动
        // alpha = 0.7 表示新速度占70%，历史速度占30%
        m_lastVelocity = QPoint(
            m_lastVelocity.x() * 0.3 + instantVelocity.x() * 0.7,
            m_lastVelocity.y() * 0.3 + instantVelocity.y() * 0.7
        );
        
        m_lastTrackedRect = faceRect;
        m_trackFrameCount++;
        
        return true;
    } else {
        // 在搜索区域内未找到，使用预测位置（如果合理）
        if (predictedRect.x() >= 0 && predictedRect.y() >= 0 &&
            predictedRect.right() < image.width() && predictedRect.bottom() < image.height() &&
            predictedRect.width() > 50 && predictedRect.height() > 50) {
            faceRect = predictedRect;
            m_lastTrackedRect = faceRect;
            m_trackFrameCount++;
            // 速度衰减（当使用预测位置时，速度衰减更快，避免过度预测）
            m_lastVelocity = QPoint(m_lastVelocity.x() * 0.7, m_lastVelocity.y() * 0.7);
            return true;
        } else {
            // 预测位置不合理，跟踪失败
            m_trackerInitialized = false;
            return false;
        }
    }
#endif
}

void OpenCVFaceRecognizer::resetTracker() // 重置跟踪器
{
#ifdef OPENCV_TRACKING_AVAILABLE // 如果OpenCV Tracking模块可用
    m_trackerInitialized = false;
    if (m_tracker) { // 如果跟踪器不为空，则释放跟踪器
        m_tracker.release();
        m_tracker = nullptr; // 将跟踪器设置为空
    }
#else // 如果OpenCV Tracking模块不可用
    m_trackerInitialized = false;
    m_lastTrackedRect = QRect(); // 将上一帧跟踪到的人脸位置设置为空
    m_lastVelocity = QPoint(0, 0); // 将上一帧的移动速度设置为0
    m_trackFrameCount = 0; // 将连续跟踪的帧数设置为0
#endif
}
