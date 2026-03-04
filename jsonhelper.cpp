#include "jsonhelper.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDate>
#include <QDateTime>

QString JsonHelper::getUserFilePath()
{
    return "user.json";
}

QString JsonHelper::getDishFilePath()
{
    return "dishes.json";
}

QString JsonHelper::getOrderFilePath()
{
    return "orders.json";
}

bool JsonHelper::saveFaceData(const QString &username, const QString &faceImagePath)
{
    const QString filePath = getUserFilePath();
    QFile file(filePath);
    QJsonObject users;

    // 读取现有用户
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            users = doc.object();
        }
        file.close();
    }

    // 更新用户的人脸数据
    if (!users.contains(username)) {
        return false; // 用户不存在
    }

    QJsonObject userObj = users[username].toObject();
    userObj["faceImagePath"] = faceImagePath;
    users[username] = userObj;

    // 保存前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath);
        QFile::copy(filePath, backupPath);
    }

    // 保存到文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(users);
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QString JsonHelper::loadFaceData(const QString &username)
{
    const QString filePath = getUserFilePath();
    const QString backupPath = filePath + ".bak";

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) {
            return false;
        }
        usersOut = doc.object();
        return true;
    };

    QJsonObject users;
    if (!loadFrom(filePath, users)) {
        if (!loadFrom(backupPath, users)) {
            return QString();
        }
    }

    if (!users.contains(username)) {
        return QString();
    }

    QJsonObject userObj = users[username].toObject();
    return userObj["faceImagePath"].toString();
}

bool JsonHelper::hasFaceData(const QString &username)
{
    QString facePath = loadFaceData(username);
    return !facePath.isEmpty() && QFile::exists(facePath);
}

bool JsonHelper::hasAnyFaceData()
{
    const QString filePath = getUserFilePath();
    const QString backupPath = filePath + ".bak";

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) {
            return false;
        }
        usersOut = doc.object();
        return true;
    };

    QJsonObject users;
    if (!loadFrom(filePath, users)) {
        if (!loadFrom(backupPath, users)) {
            return false;
        }
    }

    // 检查是否有任何人脸数据
    for (auto it = users.begin(); it != users.end(); ++it) {
        QJsonObject userObj = it.value().toObject();
        QString faceImagePath = userObj["faceImagePath"].toString();
        if (!faceImagePath.isEmpty() && QFile::exists(faceImagePath)) {
            return true;
        }
    }

    return false;
}

bool JsonHelper::saveUser(const QString &username, const QString &password)
{
    const QString filePath = getUserFilePath();
    QFile file(filePath);
    QJsonObject users;

    // 读取现有用户
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            users = doc.object();
        }
        file.close();
    }

    // 添加或更新用户
    QJsonObject userObj;
    if (users.contains(username)) {
        // 如果用户已存在，保留原有数据
        userObj = users[username].toObject();
    } else {
        // 新用户，添加创建时间
        userObj["createTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    userObj["password"] = password;
    users[username] = userObj;

    // 保存前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath);              // 删除旧备份
        QFile::copy(filePath, backupPath);      // 复制为新备份
    }

    // 保存到文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(users);
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

bool JsonHelper::loadUser(const QString &username, QString &password)
{
    const QString filePath = getUserFilePath();
    const QString backupPath = filePath + ".bak";

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) {
            return false;
        }
        usersOut = doc.object();
        return true;
    };

    QJsonObject users;
    // 优先尝试主文件，失败时尝试备份文件
    if (!loadFrom(filePath, users)) {
        if (!loadFrom(backupPath, users)) {
            return false;
        }
    }

    if (!users.contains(username)) {
        return false;
    }

    QJsonObject userObj = users[username].toObject();
    password = userObj["password"].toString();
    return true;
}

bool JsonHelper::userExists(const QString &username)
{
    QString password;
    return loadUser(username, password);
}

void JsonHelper::initializeDefaultUsers()
{
    // 只保证 admin 商家账号存在；如果用户文件存在，同时移除旧的 user 用户
    const QString filePath = getUserFilePath();
    QFile file(filePath);

    QJsonObject users;

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isObject()) {
            users = doc.object();
        }
    }

    // 移除旧的普通用户 user（如果存在）
    if (users.contains("user")) {
        users.remove("user");
    }

    // 确保 admin 存在
    if (!users.contains("admin")) {
        QJsonObject adminObj;
        adminObj["password"] = "admin123";
        users["admin"] = adminObj;
    }

    // 写回文件（带备份逻辑）
    if (!users.isEmpty()) {
        QFile::remove(filePath + ".bak");
        if (file.exists()) {
            QFile::copy(filePath, filePath + ".bak");
        }
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QJsonDocument outDoc(users);
            file.write(outDoc.toJson());
            file.close();
        }
    }
}

bool JsonHelper::saveDishes(const QVector<Dish> &dishes)
{
    const QString filePath = getDishFilePath();
    QFile file(filePath);
    QJsonArray dishArray;

    for (const Dish &dish : dishes) {
        QJsonObject dishObj;
        dishObj["id"] = dish.getId();
        dishObj["name"] = dish.getName();
        dishObj["category"] = dish.getCategory();
        dishObj["price"] = dish.getPrice();
        dishObj["description"] = dish.getDescription();
        dishObj["imagePath"] = dish.getImagePath();
        dishArray.append(dishObj);
    }

    QJsonObject root;
    root["dishes"] = dishArray;

    // 保存前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath);
        QFile::copy(filePath, backupPath);
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QVector<Dish> JsonHelper::loadDishes()
{
    QVector<Dish> dishes;
    const QString filePath = getDishFilePath();
    const QString backupPath = filePath + ".bak";
    QFile file(filePath);

    // 如果文件不存在：一次性创建并写入 16 个示例菜品
    if (!file.exists()) {
        int nextId = 1;

        auto addDish = [&](const QString &name,
                           const QString &category,
                           double price,
                           const QString &desc) {
            Dish d(name, category, price, desc);
            d.setId(nextId++);
            dishes.append(d);
        };

        addDish("宫保鸡丁", "热菜", 28.0, "经典川菜");
        addDish("麻婆豆腐", "热菜", 22.0, "四川名菜");
        addDish("凉拌黄瓜", "凉菜", 12.0, "清爽开胃");
        addDish("白米饭",   "主食",  3.0, "优质大米");
        addDish("鱼香肉丝",   "热菜", 24.0, "酸甜微辣");
        addDish("红烧茄子",   "热菜", 20.0, "家常菜");
        addDish("拍黄瓜",     "凉菜", 10.0, "爽口凉菜");
        addDish("紫菜蛋花汤", "汤品",  8.0, "清淡汤品");
        addDish("番茄炒蛋",   "热菜", 18.0, "家常经典");
        addDish("可乐",       "饮品",  5.0, "冰爽饮料");
        addDish("雪碧",       "饮品",  5.0, "冰爽饮料");
        addDish("鸡蛋炒饭",   "主食", 15.0, "营养主食");
        addDish("酸辣土豆丝", "热菜", 16.0, "开胃小炒");
        addDish("玉米排骨汤", "汤品", 26.0, "滋补汤品");
        addDish("水果拼盘",   "其他", 22.0, "时令水果");
        addDish("红豆沙",     "其他", 12.0, "甜品");

        // 写入 dishes.json
        saveDishes(dishes);
        return dishes;
    }

    // 文件存在但无法打开时，尝试读取备份文件
    if (!file.open(QIODevice::ReadOnly)) {
        QFile backupFile(backupPath);
        if (!backupFile.exists() || !backupFile.open(QIODevice::ReadOnly)) {
            return dishes;
        }

        QJsonDocument doc = QJsonDocument::fromJson(backupFile.readAll());
        backupFile.close();
        if (!doc.isObject()) {
            return dishes;
        }

        QJsonObject root = doc.object();
        QJsonArray dishArray = root["dishes"].toArray();

        for (int i = 0; i < dishArray.size(); ++i) {
            const QJsonObject dishObj = dishArray.at(i).toObject();
            Dish dish;
            dish.setId(dishObj["id"].toInt());
            dish.setName(dishObj["name"].toString());
            dish.setCategory(dishObj["category"].toString());
            dish.setPrice(dishObj["price"].toDouble());
            dish.setDescription(dishObj["description"].toString());
            dish.setImagePath(dishObj["imagePath"].toString());
            dishes.append(dish);
        }
        return dishes;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return dishes;
    }

    QJsonObject root = doc.object();
    QJsonArray dishArray = root["dishes"].toArray();

    for (int i = 0; i < dishArray.size(); ++i) {
        const QJsonObject dishObj = dishArray.at(i).toObject();
        Dish dish;
        dish.setId(dishObj["id"].toInt());
        dish.setName(dishObj["name"].toString());
        dish.setCategory(dishObj["category"].toString());
        dish.setPrice(dishObj["price"].toDouble());
        dish.setDescription(dishObj["description"].toString());
        dish.setImagePath(dishObj["imagePath"].toString());
        dishes.append(dish);
    }

    return dishes;
}

bool JsonHelper::addDish(const Dish &dish)
{
    QVector<Dish> dishes = loadDishes();
    int maxId = 0;
    for (int i = 0; i < dishes.size(); ++i) {
        const Dish &d = dishes.at(i);
        if (d.getId() > maxId) {
            maxId = d.getId();
        }
    }
    Dish newDish = dish;
    newDish.setId(maxId + 1);
    dishes.append(newDish);
    return saveDishes(dishes);
}

bool JsonHelper::updateDish(int id, const Dish &dish)
{
    QVector<Dish> dishes = loadDishes();
    for (int i = 0; i < dishes.size(); ++i) {
        if (dishes[i].getId() == id) {
            Dish updatedDish = dish;
            updatedDish.setId(id);
            dishes[i] = updatedDish;
            return saveDishes(dishes);
        }
    }
    return false;
}

bool JsonHelper::deleteDish(int id)
{
    QVector<Dish> dishes = loadDishes();
    for (int i = 0; i < dishes.size(); ++i) {
        if (dishes[i].getId() == id) {
            dishes.removeAt(i);
            return saveDishes(dishes);
        }
    }
    return false;
}

QJsonObject JsonHelper::orderToJson(const QVector<OrderItem> &orderItems, const QString &username)
{
    QJsonObject orderObj;
    orderObj["username"] = username;
    QJsonArray itemsArray;

    for (const OrderItem &item : orderItems) {
        QJsonObject itemObj;
        itemObj["dishId"] = item.getDish().getId();
        itemObj["dishName"] = item.getDish().getName();
        itemObj["quantity"] = item.getQuantity();
        itemObj["price"] = item.getDish().getPrice();
        itemsArray.append(itemObj);
    }

    orderObj["items"] = itemsArray;
    return orderObj;
}

QVector<OrderItem> JsonHelper::jsonToOrder(const QJsonObject &json)
{
    QVector<OrderItem> orderItems;
    QJsonArray itemsArray = json["items"].toArray();

    QVector<Dish> allDishes = loadDishes();

    for (int i = 0; i < itemsArray.size(); ++i) {
        const QJsonObject itemObj = itemsArray.at(i).toObject();
        int dishId = itemObj["dishId"].toInt();
        int quantity = itemObj["quantity"].toInt();

        // 查找对应的菜品
        for (int j = 0; j < allDishes.size(); ++j) {
            const Dish &dish = allDishes.at(j);
            if (dish.getId() == dishId) {
                orderItems.append(OrderItem(dish, quantity));
                break;
            }
        }
    }

    return orderItems;
}

bool JsonHelper::saveOrder(const Order &order)
{
    const QString filePath = getOrderFilePath();
    QFile file(filePath);
    QJsonArray orderArray;

    // 读取现有订单
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            QJsonObject root = doc.object();
            orderArray = root["orders"].toArray();
        }
        file.close();
    }

    // 转换为JSON
    QJsonObject orderObj;
    orderObj["orderNo"] = order.getOrderNo();
    orderObj["username"] = order.getUsername();
    orderObj["tableNumber"] = order.getTableNumber();
    orderObj["customerCount"] = order.getCustomerCount();
    orderObj["status"] = order.getStatusString();
    orderObj["createTime"] = order.getCreateTime().toString(Qt::ISODate);

    QJsonArray itemsArray;
    for (const OrderItem &item : order.getItems()) {
        QJsonObject itemObj;
        itemObj["dishId"] = item.getDish().getId();
        itemObj["dishName"] = item.getDish().getName();
        itemObj["quantity"] = item.getQuantity();
        itemObj["price"] = item.getDish().getPrice();
        itemsArray.append(itemObj);
    }
    orderObj["items"] = itemsArray;

    // 检查是否已存在，如果存在则更新，否则添加
    bool found = false;
    for (int i = 0; i < orderArray.size(); ++i) {
        const QJsonObject existing = orderArray[i].toObject();
        const QString existingOrderNo = existing.contains("orderNo")
                                            ? existing["orderNo"].toString()
                                            : QString();
        if (!existingOrderNo.isEmpty() && existingOrderNo == order.getOrderNo()) {
            orderArray[i] = orderObj;
            found = true;
            break;
        }
    }
    if (!found) {
        orderArray.append(orderObj);
    }

    // 保存到文件前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath);
        QFile::copy(filePath, backupPath);
    }

    QJsonObject root;
    root["orders"] = orderArray;
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QVector<Order> JsonHelper::loadOrders()
{
    QVector<Order> orders;
    const QString filePath = getOrderFilePath();
    const QString backupPath = filePath + ".bak";
    QFile file(filePath);

    if (!file.exists()) {
        return orders;
    }

    auto parseOrders = [&](QIODevice &device, QJsonObject &rootOut) -> bool {
        QJsonDocument doc = QJsonDocument::fromJson(device.readAll());
        if (!doc.isObject()) {
            return false;
        }
        rootOut = doc.object();
        return true;
    };

    QJsonObject root;

    // 先尝试主文件
    if (file.open(QIODevice::ReadOnly)) {
        if (!parseOrders(file, root)) {
            file.close();
            return orders;
        }
        file.close();
    } else {
        // 主文件无法打开时尝试备份
        QFile backupFile(backupPath);
        if (!backupFile.exists() || !backupFile.open(QIODevice::ReadOnly)) {
            return orders;
        }
        if (!parseOrders(backupFile, root)) {
            backupFile.close();
            return orders;
        }
        backupFile.close();
    }

    QJsonArray orderArray = root["orders"].toArray();

    QVector<Dish> allDishes = loadDishes();

    for (int i = 0; i < orderArray.size(); ++i) {
        const QJsonObject orderObj = orderArray.at(i).toObject();
        Order order;
        // 兼容旧字段：id(int)
        QString orderNo = orderObj.contains("orderNo") ? orderObj["orderNo"].toString() : QString();
        order.setUsername(orderObj["username"].toString());
        order.setTableNumber(orderObj.contains("tableNumber") ? orderObj["tableNumber"].toString() : "");
        order.setCustomerCount(orderObj.contains("customerCount") ? orderObj["customerCount"].toInt() : 0);
        order.setStatus(Order::statusFromString(orderObj["status"].toString()));

        QString timeStr = orderObj["createTime"].toString();
        if (!timeStr.isEmpty()) {
            order.setCreateTime(QDateTime::fromString(timeStr, Qt::ISODate));
        }

        if (orderNo.isEmpty() && orderObj.contains("id")) {
            // legacy: 用 createTime 的日期 + id(三位) 兜底
            const QDate legacyDate = order.getCreateTime().isValid() ? order.getCreateTime().date() : QDate::currentDate();
            const QString mmdd = legacyDate.toString("MMdd");
            const int legacyId = orderObj["id"].toInt();
            orderNo = mmdd + QString("%1").arg(legacyId % 1000, 3, 10, QChar('0'));
        }
        order.setOrderNo(orderNo);

        QJsonArray itemsArray = orderObj["items"].toArray();
        QVector<OrderItem> items;
        for (int j = 0; j < itemsArray.size(); ++j) {
            const QJsonObject itemObj = itemsArray.at(j).toObject();
            int dishId = itemObj["dishId"].toInt();
            int quantity = itemObj["quantity"].toInt();

            for (int k = 0; k < allDishes.size(); ++k) {
                const Dish &dish = allDishes.at(k);
                if (dish.getId() == dishId) {
                    items.append(OrderItem(dish, quantity));
                    break;
                }
            }
        }
        order.setItems(items);
        orders.append(order);
    }

    return orders;
}

bool JsonHelper::updateOrderStatus(const QString &orderNo, OrderStatus status)
{
    QVector<Order> orders = loadOrders();
    for (int i = 0; i < orders.size(); ++i) {
        if (orders[i].getOrderNo() == orderNo) {
            orders[i].setStatus(status);
            return saveOrder(orders[i]);
        }
    }
    return false;
}

QString JsonHelper::getNextOrderNo(const QDate &date)
{
    QVector<Order> orders = loadOrders();
    const QString mmdd = date.toString("MMdd");
    int maxSeq = 0;
    for (auto it = orders.cbegin(); it != orders.cend(); ++it) {
        const QString no = it->getOrderNo();
        if (no.startsWith(mmdd) && no.size() >= 7) {
            bool ok = false;
            const int seq = no.mid(4, 3).toInt(&ok);
            if (ok && seq > maxSeq) {
                maxSeq = seq;
            }
        }
    }
    const int nextSeq = maxSeq + 1;
    return mmdd + QString("%1").arg(nextSeq, 3, 10, QChar('0'));
}

bool JsonHelper::deleteCompletedOrders()
{
    const QString filePath = getOrderFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray orderArray = root["orders"].toArray();
    QJsonArray kept;
    for (int i = 0; i < orderArray.size(); ++i) {
        const QJsonObject o = orderArray.at(i).toObject();
        const QString statusStr = o["status"].toString();
        const OrderStatus st = Order::statusFromString(statusStr);
        if (st != OrderStatus::Completed) {
            kept.append(o);
        }
    }
    root["orders"] = kept;

    // 写入前备份
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath);
        QFile::copy(filePath, backupPath);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    file.close();
    return true;
}
