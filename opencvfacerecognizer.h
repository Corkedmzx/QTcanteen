#ifndef OPENCVFACERECOGNIZER_H
#define OPENCVFACERECOGNIZER_H

#include <QString>
#include <QImage>
#include <QVector>
#include <QRect>
#include <QPoint>

#ifdef OPENCV_AVAILABLE
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/dnn.hpp>
// 尝试包含 tracking 模块（需要 opencv_contrib）
#if __has_include(<opencv2/tracking.hpp>)
#include <opencv2/tracking.hpp>
#define OPENCV_TRACKING_AVAILABLE
#elif __has_include(<opencv2/tracking/tracker.hpp>)
#include <opencv2/tracking/tracker.hpp>
#define OPENCV_TRACKING_AVAILABLE
#endif

// 尝试包含 face 模块（需要 opencv_contrib）
#if __has_include(<opencv2/face.hpp>)
#include <opencv2/face.hpp>
#define OPENCV_FACE_AVAILABLE
#else
// 如果没有 face 模块，定义基本接口（编译时会报错，提示需要安装 opencv_contrib）
// 用户需要安装 opencv_contrib 才能使用此功能
namespace cv {
namespace face {
    class LBPHFaceRecognizer;
}
}
#endif
#endif // OPENCV_AVAILABLE

class OpenCVFaceRecognizer
{
public:
    OpenCVFaceRecognizer();
    ~OpenCVFaceRecognizer();

    // 初始化识别器（加载已训练的数据）
    bool initialize();

    // 训练识别器（录入人脸）
    bool train(const QString &username, const QImage &faceImage);

    // 识别人脸
    QString recognize(const QImage &image, double &confidence);

    // 检测图像中是否有人脸
    bool detectFace(const QImage &image, QRect &faceRect);
    
    // 初始化跟踪器（使用检测到的人脸位置）
    bool initTracker(const QImage &image, const QRect &faceRect);
    
    // 更新跟踪器（返回跟踪到的人脸位置）
    bool updateTracker(const QImage &image, QRect &faceRect);
    
    // 重置跟踪器
    void resetTracker();

    // 保存训练数据
    bool saveModel();

    // 加载训练数据
    bool loadModel();

    // 获取所有已训练的用户列表
    QVector<QString> getTrainedUsers() const { return m_trainedUsers; }

private:
    // QImage转cv::Mat
    cv::Mat qImageToMat(const QImage &image);

    // cv::Mat转QImage
    QImage matToQImage(const cv::Mat &mat);

    // 预处理图像（灰度化、直方图均衡化、尺寸归一化）
    cv::Mat preprocessImage(const cv::Mat &image);
    
    // 人脸对齐（基于关键点）
    cv::Mat alignFace(const cv::Mat &image, const cv::Rect &faceRect);
    
    // 提取深度特征（使用 DNN 或传统方法）
    cv::Mat extractDeepFeatures(const cv::Mat &alignedFace);
    
    // 计算特征相似度（余弦相似度）
    double calculateSimilarity(const cv::Mat &features1, const cv::Mat &features2);
    
    // 质量评估
    double assessFaceQuality(const cv::Mat &faceImage);
    
    // 遮挡检测（返回遮挡比例，0.0-1.0）
    double detectOcclusion(const cv::Mat &faceImage, const cv::Rect &faceRect);

#ifdef OPENCV_AVAILABLE
#ifdef OPENCV_FACE_AVAILABLE
    cv::Ptr<cv::face::LBPHFaceRecognizer> m_recognizer;
#endif
    cv::CascadeClassifier m_faceCascade;
    
    // 使用深度特征存储（更现代的方法）
    QVector<cv::Mat> m_faceFeatures;  // 存储深度特征向量
    
    // DNN 模型（如果可用）
    cv::dnn::Net m_faceNet;
    bool m_dnnAvailable;
    
    // Tracker（用于实时跟踪）
#ifdef OPENCV_TRACKING_AVAILABLE
    cv::Ptr<cv::Tracker> m_tracker;
    bool m_trackerInitialized;
#else
    // 简单的基于位置预测的跟踪器（当 OpenCV tracking 模块不可用时使用）
    QRect m_lastTrackedRect;  // 上一帧跟踪到的人脸位置
    QPoint m_lastVelocity;    // 上一帧的移动速度
    int m_trackFrameCount;    // 连续跟踪的帧数
    bool m_trackerInitialized;
#endif
#endif // OPENCV_AVAILABLE
    
    QVector<QString> m_trainedUsers;
    QVector<int> m_labels;  // 保留用于兼容性
    QString m_modelPath;
    bool m_initialized;
};

#endif // OPENCVFACERECOGNIZER_H
