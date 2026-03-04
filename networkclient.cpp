#include "networkclient.h"
#include <QDebug>

NetworkClient::NetworkClient(QObject *parent)
    : QObject(parent), m_socket(nullptr), m_port(8888)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &NetworkClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &NetworkClient::onError);
}

NetworkClient::~NetworkClient()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

bool NetworkClient::connectToServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_socket->connectToHost(host, port);
    return true;
}

void NetworkClient::disconnectFromServer()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
    }
}

bool NetworkClient::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void NetworkClient::sendServiceRequest(const QString &request)
{
    if (!isConnected()) {
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json;
    json["type"] = "service";
    json["request"] = request;

    QJsonDocument doc(json);
    m_socket->write(doc.toJson());
}

void NetworkClient::sendOrder(const QJsonObject &order)
{
    if (!isConnected()) {
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json;
    json["type"] = "order";
    json["order"] = order;

    QJsonDocument doc(json);
    m_socket->write(doc.toJson());
}

void NetworkClient::sendCheckoutRequest(const QJsonObject &order)
{
    if (!isConnected()) {
        emit errorOccurred("未连接到服务器");
        return;
    }

    QJsonObject json;
    json["type"] = "checkout";
    json["order"] = order;

    QJsonDocument doc(json);
    m_socket->write(doc.toJson());
}

void NetworkClient::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }

    QJsonObject json = doc.object();
    QString type = json["type"].toString();

    if (type == "order_confirm") {
        emit orderConfirmed();
    } else if (type == "checkout_success") {
        double totalAmount = json["totalAmount"].toDouble();
        emit checkoutSuccess(totalAmount);
    } else if (type == "dish_update") {
        // 菜品更新通知，将整个JSON对象作为消息发送
        QJsonDocument doc(json);
        emit messageReceived(doc.toJson());
    } else if (type == "order_completed") {
        // 订单完成通知，将整个JSON对象作为消息发送
        QJsonDocument doc(json);
        emit messageReceived(doc.toJson());
    } else if (type == "message") {
        QString message = json["message"].toString();
        emit messageReceived(message);
    }
}

void NetworkClient::onConnected()
{
    emit connected();
}

void NetworkClient::onDisconnected()
{
    emit disconnected();
}

void NetworkClient::onError(QAbstractSocket::SocketError error)
{
    QString errorMsg;
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
