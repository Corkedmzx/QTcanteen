#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "dish.h"
#include "orderitem.h"
#include "order.h"

class JsonHelper
{
public:
    // 用户相关
    static bool saveUser(const QString &username, const QString &password);
    static bool loadUser(const QString &username, QString &password);
    static bool userExists(const QString &username);
    static bool deleteUser(const QString &username);
    static void initializeDefaultUsers();
    
    // 人脸识别相关
    static bool saveFaceData(const QString &username, const QString &faceImagePath);
    static QString loadFaceData(const QString &username);
    static bool hasFaceData(const QString &username);
    static bool hasAnyFaceData(); // 检查是否有任何人脸数据

    // 菜品相关
    static bool saveDishes(const QVector<Dish> &dishes);
    static QVector<Dish> loadDishes();
    static bool addDish(const Dish &dish);
    static bool updateDish(int id, const Dish &dish);
    static bool deleteDish(int id);

    // 订单相关
    static QJsonObject orderToJson(const QVector<OrderItem> &orderItems, const QString &username);
    static bool saveOrder(const Order &order);
    static QVector<Order> loadOrders();
    static bool updateOrderStatus(const QString &orderNo, OrderStatus status);
    static QString getNextOrderNo(const QDate &date = QDate::currentDate());
    static bool deleteCompletedOrders();

private:
    static QString getUserFilePath();
    static QString getDishFilePath();
    static QString getOrderFilePath();
};

#endif // JSONHELPER_H
