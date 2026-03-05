#ifndef NETWORKSERVER_H
#define NETWORKSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QJsonObject>
#include <QVector>

class NetworkServer : public QObject
{
    Q_OBJECT

public:
    explicit NetworkServer(QObject *parent = nullptr);
    ~NetworkServer();

    // 默认监听端口为 8888
    bool startServer(quint16 port = 8888);
    void stopServer();
    bool isListening() const;
    void broadcastDishUpdate(); // 广播菜品更新通知
    void broadcastMessage(const QByteArray &message); // 广播消息给所有客户端

signals:
    void newOrderReceived(const QJsonObject &order);
    void dishUpdateRequested(); // 请求客户端刷新菜品
    void clientConnected();
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    QTcpServer *m_server;
    QVector<QTcpSocket*> m_clients;
    quint16 m_port;
};

#endif // NETWORKSERVER_H
