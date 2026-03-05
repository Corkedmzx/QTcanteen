#include "networkclient.h"
#include <QDebug>

NetworkClient::NetworkClient(QObject *parent) // 构造函数
    : QObject(parent), m_socket(nullptr), m_port(8888)
{
    m_socket = new QTcpSocket(this); // 创建TCP套接字
    connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead); // 连接读取信号，当有数据可读时，调用onReadyRead函数，将数据读取到m_socket中，json解析，然后根据type类型，调用相应的函数
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &NetworkClient::onError);
}

NetworkClient::~NetworkClient() // 析构函数
{
    if (m_socket) {
        m_socket->disconnectFromHost(); // 断开与服务器的连接
    }
}

bool NetworkClient::connectToServer(const QString &host, quint16 port) // 创建连接到服务器，bool类型，返回是否连接成功
{
    m_host = host; // 设置主机地址
    m_port = port; // 设置端口号
    m_socket->connectToHost(host, port); // 连接到服务器
    return true; // 返回true
}

void NetworkClient::disconnectFromServer() // 断开与服务器的连接，void类型，没有返回值
{
    if (m_socket) {
        m_socket->disconnectFromHost(); // 断开与服务器的连接
    }
}

bool NetworkClient::isConnected() const // 检查是否连接到服务器，bool类型，返回是否连接成功
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::sendServiceRequest(const QString &request) // 发送服务请求，void类型，没有返回值
{
    if (!isConnected()) { // 如果未连接到服务器，则发送错误信号
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json; // 创建JSON对象
    json["type"] = "service"; // 添加请求类型
    json["request"] = request; // 添加请求内容

    QJsonDocument doc(json); // 创建JSON文档
    m_socket->write(doc.toJson()); // 发送JSON数据
}

void NetworkClient::sendOrder(const QJsonObject &order) // 发送订单
{
    if (!isConnected()) { // 如果未连接到服务器，则发送错误信号
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json; // 创建JSON对象
    json["type"] = "order";
    json["order"] = order; // 添加订单类型和订单内容

    QJsonDocument doc(json);
    m_socket->write(doc.toJson()); // 发送JSON数据
}

void NetworkClient::sendCheckoutRequest(const QJsonObject &order) // 发送结账请求
{
    if (!isConnected()) { // 如果未连接到服务器，则发送错误信号
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json; // 创建JSON对象
    json["type"] = "checkout";
    json["order"] = order;

    QJsonDocument doc(json);
    m_socket->write(doc.toJson());
}

void NetworkClient::onReadyRead() // 数据读取处理
{
    QByteArray data = m_socket->readAll(); // 读取数据
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) { // 如果数据不是JSON对象，则返回
        return;
    }

    QJsonObject json = doc.object(); // 获取JSON对象
    QString type = json["type"].toString();

    if (type == "order_confirm") {
        emit orderConfirmed(); // 发出订单确认信号
    } else if (type == "checkout_success") {
        double totalAmount = json["totalAmount"].toDouble(); // 获取总金额
        emit checkoutSuccess(totalAmount); // 发出结账成功信号
    } else if (type == "dish_update") {
        // 菜品更新通知，将整个JSON对象作为消息发送
        QJsonDocument doc(json);
        emit messageReceived(doc.toJson()); // 发出消息接收信号
    } else if (type == "order_completed") {
        // 订单完成通知，将整个JSON对象作为消息发送
        QJsonDocument doc(json);
        emit messageReceived(doc.toJson()); // 发出消息接收信号
    } else if (type == "message") {
        QString message = json["message"].toString(); // 获取消息内容
        emit messageReceived(message); // 发出消息接收信号
    }
}

void NetworkClient::onConnected() // 连接成功处理
{
    emit connected(); // 发出连接成功信号
}

void NetworkClient::onDisconnected() // 断开连接处理
{
    emit disconnected(); // 发出断开连接信号
}

void NetworkClient::onError(QAbstractSocket::SocketError error) // 错误处理
{
    QString errorMsg; // 错误信息
    switch (error) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "连接被拒绝";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMsg = "远程主机关闭连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "找不到主机";
        break;
    default:
        errorMsg = "网络错误: " + m_socket->errorString();
    }
    emit errorOccurred(errorMsg);
}
