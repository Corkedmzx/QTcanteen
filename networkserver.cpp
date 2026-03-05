#include "networkserver.h"
#include <QDebug>
#include <QJsonDocument>

NetworkServer::NetworkServer(QObject *parent) // 构造函数
    : QObject(parent), m_server(nullptr), m_port(8888)
{
    m_server = new QTcpServer(this); // 创建TCP服务器
    connect(m_server, &QTcpServer::newConnection, this, &NetworkServer::onNewConnection);
}

NetworkServer::~NetworkServer() // 析构函数
{
    stopServer(); // 停止服务器
}

bool NetworkServer::startServer(quint16 port) // 启动服务器，bool类型，返回是否启动成功
{
    m_port = port; // 设置端口号
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "服务器启动成功，监听端口:" << port;
        return true;
    } else {
        qDebug() << "服务器启动失败:" << m_server->errorString();
        return false;
    }
}

void NetworkServer::stopServer() // 停止服务器
{
    if (m_server && m_server->isListening()) {
        // 断开所有客户端连接
        for (QTcpSocket *client : m_clients) {
            if (client) {
                client->disconnectFromHost();
            }
        }
        m_clients.clear(); // 清空客户端列表
        m_server->close(); // 关闭服务器
        qDebug() << "服务器已停止";
    }
}

bool NetworkServer::isListening() const // 检查是否在监听
{
    return m_server && m_server->isListening();
}

void NetworkServer::onNewConnection() // 新连接处理
{
    QTcpSocket *client = m_server->nextPendingConnection(); // 获取下一个连接的客户端
    if (!client) {
        return; // 如果客户端为空，则返回
    }

    m_clients.append(client); // 添加客户端到客户端列表
    connect(client, &QTcpSocket::readyRead, this, &NetworkServer::onClientReadyRead); // 连接读取信号，当有数据可读时，调用onClientReadyRead函数，将数据读取到client中，json解析，然后根据type类型，调用相应的函数
    connect(client, &QTcpSocket::disconnected, this, &NetworkServer::onClientDisconnected); // 连接断开信号，当客户端断开连接时，调用onClientDisconnected函数，将客户端从客户端列表中删除

    qDebug() << "新客户端连接，当前连接数:" << m_clients.size();
    emit clientConnected(); // 发出客户端连接信号
}

void NetworkServer::onClientReadyRead() // 客户端数据读取处理
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender()); // 获取发送信号的客户端
    if (!client) {
        return; // 如果客户端为空，则返回
    }

    QByteArray data = client->readAll(); // 读取数据
    if (data.isEmpty()) {
        return; // 如果数据为空，则返回
    }

    QJsonParseError parseError; // 创建JSON解析错误
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) { // 如果解析错误不为空，或者数据不是JSON对象，则返回
        qDebug() << "收到无效的JSON数据:" << parseError.errorString();
        return;
    }

    QJsonObject json = doc.object(); // 获取JSON对象
    if (!json.contains("type")) {
        qDebug() << "JSON数据缺少type字段";
        return;
    }

    QString type = json["type"].toString(); // 获取类型

    if (type == "order") { // 如果类型为订单，则处理订单
        // 收到订单
        if (!json.contains("order") || !json["order"].isObject()) { // 如果订单数据格式错误，则返回
            qDebug() << "订单数据格式错误";
            return;
        }
        
        QJsonObject orderObj = json["order"].toObject(); // 获取订单对象
        if (orderObj.isEmpty() || !orderObj.contains("orderNo")) {
            qDebug() << "订单对象无效";
            return;
        }
        
        qDebug() << "收到新订单:" << orderObj["orderNo"].toString();
        
        // 直接发出信号（QTcpServer已经在主线程中运行）
        emit newOrderReceived(orderObj);

        // 发送确认消息
        QJsonObject response;
        response["type"] = "order_confirm";
        QJsonDocument responseDoc(response);
        QByteArray responseData = responseDoc.toJson();
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(responseData);
        }
    } else if (type == "checkout") {
        // 收到结账请求
        if (json.contains("order") && json["order"].isObject()) {
            QJsonObject orderObj = json["order"].toObject(); // 获取订单对象
            double totalAmount = orderObj.contains("totalAmount") ? orderObj["totalAmount"].toDouble() : 0.0;
            
            QJsonObject response; // 创建响应对象
            response["type"] = "checkout_success";
            response["totalAmount"] = totalAmount;
            QJsonDocument responseDoc(response); // 创建JSON文档
            QByteArray responseData = responseDoc.toJson();
            if (client->state() == QAbstractSocket::ConnectedState) {
                client->write(responseData);
            }
        }
    } else if (type == "service") {
        // 收到服务请求
        QString request = json.contains("request") ? json["request"].toString() : "";
        qDebug() << "收到服务请求:" << request;
        // 可以在这里处理服务请求
    }
}

void NetworkServer::onClientDisconnected() // 客户端断开处理
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender()); // 获取发送信号的客户端
    if (client) {
        m_clients.removeAll(client); // 从客户端列表中删除客户端
        client->deleteLater();
        qDebug() << "客户端断开连接，当前连接数:" << m_clients.size();
        emit clientDisconnected(); // 发出客户端断开信号
    }
}

// 广播菜品更新通知给所有客户端
void NetworkServer::broadcastDishUpdate()
{
    QJsonObject message;
    message["type"] = "dish_update";
    QJsonDocument doc(message);
    QByteArray data = doc.toJson();
    broadcastMessage(data);
}

// 广播消息给所有客户端
void NetworkServer::broadcastMessage(const QByteArray &message)
{
    for (QTcpSocket *client : m_clients) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->write(message);
        }
    }
    qDebug() << "已广播消息给" << m_clients.size() << "个客户端";
}
