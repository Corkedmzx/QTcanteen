#ifndef ORDER_H
#define ORDER_H

#include <QString>
#include <QDateTime>
#include <QVector>
#include "orderitem.h"

enum class OrderStatus {
    Pending,      // 待处理
    Processing,   // 处理中
    Completed,    // 已完成
    Cancelled     // 已取消
};

class Order
{
public:
    Order();
    Order(const QString &orderNo, const QString &username, const QVector<OrderItem> &items, OrderStatus status = OrderStatus::Pending);

    QString getOrderNo() const { return m_orderNo; }
    void setOrderNo(const QString &orderNo) { m_orderNo = orderNo; }

    QString getUsername() const { return m_username; }
    void setUsername(const QString &username) { m_username = username; }

    QString getTableNumber() const { return m_tableNumber; }
    void setTableNumber(const QString &tableNumber) { m_tableNumber = tableNumber; }

    int getCustomerCount() const { return m_customerCount; }
    void setCustomerCount(int count) { m_customerCount = count; }

    QVector<OrderItem> getItems() const { return m_items; }
    void setItems(const QVector<OrderItem> &items) { m_items = items; }

    OrderStatus getStatus() const { return m_status; }
    void setStatus(OrderStatus status) { m_status = status; }

    QString getStatusString() const;
    static OrderStatus statusFromString(const QString &statusStr);

    QDateTime getCreateTime() const { return m_createTime; }
    void setCreateTime(const QDateTime &time) { m_createTime = time; }

    double getTotalAmount() const;

private:
    QString m_orderNo; // 订单号：MMDD + 当日序号(3位)，如 0210001
    QString m_username;
    QString m_tableNumber; // 桌编号
    int m_customerCount; // 客户人数
    QVector<OrderItem> m_items;
    OrderStatus m_status;
    QDateTime m_createTime;
};

#endif // ORDER_H
