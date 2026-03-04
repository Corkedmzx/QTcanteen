#ifndef SEETAFACEFACERECOGNIZER_H
#define SEETAFACEFACERECOGNIZER_H

#include <QString>
#include <QImage>
#include <QVector>
#include <QRect>

// SeetaFace6Open 头文件
#ifdef SEETAFACE_AVAILABLE
#include <seeta/FaceDetector.h>
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceRecognizer.h>
#include <seeta/common_alignment.h>
#include <seeta/Common/CStruct.h>
#include <seeta/Common/Struct.h>
#define SEETAFACE_ENABLED
#else
// 如果没有 SeetaFace，定义基本接口
namespace seeta {
    class FaceDetector;
    class FaceLandmarker;
    class FaceRecognizer;
    
    // 基本类型定义
    struct SeetaImageData {
        int width;
        int height;
        int channels;
        unsigned char *data;
    };
    
    struct SeetaRect {
        int x;
        int y;
        int width;
        int height;
    };
}
#endif

class SeetaFaceRecognizer
{
public:
    SeetaFaceRecognizer();
    ~SeetaFaceRecognizer();

    // 初始化识别器（加载模型文件）
    bool initialize(const QString &modelPath = "");

    // 训练识别器（录入人脸）
    bool train(const QString &username, const QImage &faceImage);

    // 识别人脸
    QString recognize(const QImage &image, float &similarity);

    // 检测图像中是否有人脸
    bool detectFace(const QImage &image, QRect &faceRect);

    // 保存训练数据
    bool saveModel(const QString &modelPath = "");

    // 加载训练数据
    bool loadModel(const QString &modelPath = "");

    // 获取所有已训练的用户列表
    QVector<QString> getTrainedUsers() const { return m_trainedUsers; }

    // 清除所有训练数据
    void clear();

private:
    // QImage转SeetaImageData
    seeta::SeetaImageData qImageToSeetaImageData(const QImage &image);

    // 预处理图像
    QImage preprocessImage(const QImage &image);
    
    // 遮挡检测（返回遮挡比例，0.0-1.0）
    double detectOcclusion(const QImage &image, const QRect &faceRect);

#ifdef SEETAFACE_ENABLED
    seeta::FaceDetector *m_detector;
    seeta::FaceLandmarker *m_landmarker;
    seeta::FaceRecognizer *m_recognizer;
    
    // 模型文件路径
    QString m_detectorModelPath;
    QString m_landmarkerModelPath;
    QString m_recognizerModelPath;
#endif

    QVector<QString> m_trainedUsers;
    QVector<int> m_faceIds;  // SeetaFace 使用 face_id 来标识不同的人脸
    QVector<QVector<float>> m_faceFeatures;  // 存储人脸特征向量
    
    QString m_modelPath;
    bool m_initialized;
    
    // 默认模型路径（相对于可执行文件）
    static const QString DEFAULT_MODEL_PATH;
};

#endif // SEETAFACEFACERECOGNIZER_H
