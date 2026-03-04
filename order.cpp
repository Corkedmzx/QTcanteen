#include "order.h"

Order::Order()
    : m_orderNo(""), m_username(""), m_tableNumber(""), m_customerCount(0), m_status(OrderStatus::Pending), m_createTime(QDateTime::currentDateTime())
{
}

Order::Order(const QString &orderNo, const QString &username, const QVector<OrderItem> &items, OrderStatus status)
    : m_orderNo(orderNo), m_username(username), m_tableNumber(""), m_customerCount(0), m_items(items), m_status(status), m_createTime(QDateTime::currentDateTime())
{
}

QString Order::getStatusString() const
{
    switch (m_status) {
    case OrderStatus::Pending:
        return "待处理";
    case OrderStatus::Processing:
        return "处理中";
    case OrderStatus::Completed:
        return "已完成";
    case OrderStatus::Cancelled:
        return "已取消";
    default:
        return "未知";
    }
}

OrderStatus Order::statusFromString(const QString &statusStr)
{
    if (statusStr == "待处理" || statusStr == "Pending") {
        return OrderStatus::Pending;
    } else if (statusStr == "处理中" || statusStr == "Processing") {
        return OrderStatus::Processing;
    } else if (statusStr == "已完成" || statusStr == "Completed") {
        return OrderStatus::Completed;
    } else if (statusStr == "已取消" || statusStr == "Cancelled") {
        return OrderStatus::Cancelled;
    }
    return OrderStatus::Pending;
}

double Order::getTotalAmount() const
{
    double total = 0.0;
    for (const OrderItem &item : m_items) {
        total += item.getTotalPrice();
    }
    return total;
}
