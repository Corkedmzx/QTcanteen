#include "order.h"

Order::Order()
    : m_orderNo(""), m_username(""), m_tableNumber(""), m_customerCount(0), m_status(OrderStatus::Pending), m_createTime(QDateTime::currentDateTime())
{
}

Order::Order(const QString &orderNo, const QString &username, const QVector<OrderItem> &items, OrderStatus status)
    : m_orderNo(orderNo), m_username(username), m_tableNumber(""), m_customerCount(0), m_items(items), m_status(status), m_createTime(QDateTime::currentDateTime())
{
}

QString Order::getStatusString() const // 获取状态字符串（更新，从四状态返回变成二状态返回）
{
    switch (m_status) {
    case OrderStatus::Pending:
        return "待处理";
    case OrderStatus::Completed:
        return "已完成";
    default:
        return "待处理";
    }
}

OrderStatus Order::statusFromString(const QString &statusStr) // 从字符串转换状态（更新，从四状态转换变成二状态转换）
{
    if (statusStr == "待处理" || statusStr == "Pending") {
        return OrderStatus::Pending;
    } else if (statusStr == "已完成" || statusStr == "Completed") {
        return OrderStatus::Completed;
    }
    return OrderStatus::Pending;
}

double Order::getTotalAmount() const // 获取总金额
{
    double total = 0.0;
    for (const OrderItem &item : m_items) {
        total += item.getTotalPrice();
    }
    return total;
}
