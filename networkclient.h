#ifndef NETWORKCLIENT_H
#define NETWORKCLIENT_H

#include <QTcpSocket>
#include <QObject>
#include <QJsonObject>

class NetworkClient : public QObject
{
    Q_OBJECT

public:
    explicit NetworkClient(QObject *parent = nullptr);
    ~NetworkClient();

    bool connectToServer(const QString &host = "localhost", quint16 port = 8888);
    void disconnectFromServer();
    bool isConnected() const;

    // 发送服务请求
    void sendServiceRequest(const QString &request);
    // 发送订单
    void sendOrder(const QJsonObject &order);
    // 发送结账请求
    void sendCheckoutRequest(const QJsonObject &order);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);
    void orderConfirmed();
    void checkoutSuccess(double totalAmount);
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    QTcpSocket *m_socket;
    QString m_host;
    quint16 m_port;
};

#endif // NETWORKCLIENT_H
