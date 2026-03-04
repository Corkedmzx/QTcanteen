#include "networkserver.h"
#include <QDebug>
#include <QJsonDocument>

NetworkServer::NetworkServer(QObject *parent)
    : QObject(parent), m_server(nullptr), m_port(8888)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &NetworkServer::onNewConnection);
}

NetworkServer::~NetworkServer()
{
    stopServer();
}

bool NetworkServer::startServer(quint16 port)
{
    m_port = port;
    if (m_server->listen(QHostAddress::Any, port)) {
        qDebug() << "服务器启动成功，监听端口:" << port;
        return true;
    } else {
        qDebug() << "服务器启动失败:" << m_server->errorString();
        return false;
    }
}

void NetworkServer::stopServer()
{
    if (m_server && m_server->isListening()) {
        // 断开所有客户端连接
        for (QTcpSocket *client : m_clients) {
            if (client) {
                client->disconnectFromHost();
            }
        }
        m_clients.clear();
        m_server->close();
        qDebug() << "服务器已停止";
    }
}

bool NetworkServer::isListening() const
{
    return m_server && m_server->isListening();
}

void NetworkServer::onNewConnection()
{
    QTcpSocket *client = m_server->nextPendingConnection();
    if (!client) {
        return;
    }

    m_clients.append(client);
    connect(client, &QTcpSocket::readyRead, this, &NetworkServer::onClientReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &NetworkServer::onClientDisconnected);

    qDebug() << "新客户端连接，当前连接数:" << m_clients.size();
    emit clientConnected();
}

void NetworkServer::onClientReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) {
        return;
    }

    QByteArray data = client->readAll();
    if (data.isEmpty()) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "收到无效的JSON数据:" << parseError.errorString();
        return;
    }

    QJsonObject json = doc.object();
    if (!json.contains("type")) {
        qDebug() << "JSON数据缺少type字段";
        return;
    }

    QString type = json["type"].toString();

    if (type == "order") {
        // 收到订单
        if (!json.contains("order") || !json["order"].isObject()) {
            qDebug() << "订单数据格式错误";
            return;
        }
        
        QJsonObject orderObj = json["order"].toObject();
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
            QJsonObject orderObj = json["order"].toObject();
            double totalAmount = orderObj.contains("totalAmount") ? orderObj["totalAmount"].toDouble() : 0.0;
            
            QJsonObject response;
            response["type"] = "checkout_success";
            response["totalAmount"] = totalAmount;
            QJsonDocument responseDoc(response);
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

void NetworkServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        m_clients.removeAll(client);
        client->deleteLater();
        qDebug() << "客户端断开连接，当前连接数:" << m_clients.size();
        emit clientDisconnected();
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
