#include "jsonhelper.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDate>
#include <QDateTime>

QString JsonHelper::getUserFilePath() // 获取用户文件路径
{
    return "user.json";
}

QString JsonHelper::getDishFilePath() // 获取菜品文件路径
{
    return "dishes.json";
}

QString JsonHelper::getOrderFilePath()
{
    return "orders.json";
}

bool JsonHelper::saveFaceData(const QString &username, const QString &faceImagePath) // 保存人脸数据
{
    // 获取用户文件路径
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

    QJsonObject userObj = users[username].toObject(); // 获取用户对象
    userObj["faceImagePath"] = faceImagePath; // 设置人脸图像路径
    users[username] = userObj; // 设置用户对象

    // 保存前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak"; // 获取备份文件路径
        QFile::remove(backupPath); // 删除备份文件
        QFile::copy(filePath, backupPath); // 复制文件到备份文件
    }

    // 保存到文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(users); // 创建JSON文档
        file.write(doc.toJson()); // 写入JSON数据
        file.close(); // 关闭文件
        return true; // 返回true
    }
    return false;
}

QString JsonHelper::loadFaceData(const QString &username) // 加载人脸数据
{
    // 获取用户文件路径
    const QString filePath = getUserFilePath();
    const QString backupPath = filePath + ".bak";

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        // 加载文件
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false; // 如果文件不存在或无法打开，则返回false
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll()); // 读取JSON数据
        f.close(); // 关闭文件
        if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
            return false; // 返回false
        }
        usersOut = doc.object(); // 设置用户对象
        return true; // 返回true
    };

    QJsonObject users; // 创建用户对象
    if (!loadFrom(filePath, users)) { // 如果加载文件失败，则加载备份文件
        if (!loadFrom(backupPath, users)) { // 如果加载备份文件失败，则返回空字符串
            return QString(); // 返回空字符串
        }
    }

    if (!users.contains(username)) { // 如果用户不存在，则返回空字符串
        return QString(); // 返回空字符串
    }

    QJsonObject userObj = users[username].toObject(); // 获取用户对象
    return userObj["faceImagePath"].toString(); // 返回人脸图像路径
}

bool JsonHelper::hasFaceData(const QString &username) // 检查是否有任何人脸数据
{
    QString facePath = loadFaceData(username); // 加载人脸数据
    return !facePath.isEmpty() && QFile::exists(facePath); // 返回是否有人脸数据
}

bool JsonHelper::hasAnyFaceData() // 检查是否有任何人脸数据
{
    const QString filePath = getUserFilePath(); // 获取用户文件路径
    const QString backupPath = filePath + ".bak"; // 获取备份文件路径

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        QFile f(path); // 打开文件
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false; // 如果文件不存在或无法打开，则返回false
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll()); // 读取JSON数据
        f.close(); // 关闭文件
        if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
            return false; // 返回false
        }
        usersOut = doc.object(); // 设置用户对象
        return true;
    };

    QJsonObject users; // 创建用户对象
    if (!loadFrom(filePath, users)) { // 如果加载文件失败，则加载备份文件
        if (!loadFrom(backupPath, users)) {
            return false; // 如果加载备份文件失败，则返回false
        }
    }

    // 检查是否有任何人脸数据
    for (auto it = users.begin(); it != users.end(); ++it) {
        QJsonObject userObj = it.value().toObject(); // 获取用户对象
        QString faceImagePath = userObj["faceImagePath"].toString(); // 获取人脸图像路径
        if (!faceImagePath.isEmpty() && QFile::exists(faceImagePath)) { // 如果人脸图像路径不为空且文件存在，则返回true
            return true; // 返回true
        }
    }

    return false; // 返回false
}

bool JsonHelper::saveUser(const QString &username, const QString &password) // 保存用户
{
    const QString filePath = getUserFilePath(); // 获取用户文件路径
    QFile file(filePath); // 打开文件
    QJsonObject users; // 创建用户对象

    // 读取现有用户
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取JSON数据
        if (doc.isObject()) {
            users = doc.object(); // 设置用户对象
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
        QJsonDocument doc(users); // 创建JSON文档
        file.write(doc.toJson()); // 写入JSON数据
        file.close(); // 关闭文件
        return true; // 返回true
    }
    return false; // 返回false
}

bool JsonHelper::loadUser(const QString &username, QString &password) // 加载用户
{
    const QString filePath = getUserFilePath(); // 获取用户文件路径
    const QString backupPath = filePath + ".bak";

    auto loadFrom = [&](const QString &path, QJsonObject &usersOut) -> bool {
        QFile f(path); // 打开文件
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            return false; // 如果文件不存在或无法打开，则返回false
        }
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll()); // 读取JSON数据
        f.close(); // 关闭文件
        if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
            return false; // 返回false
        }
        usersOut = doc.object(); // 设置用户对象
        return true; // 返回true
    };

    QJsonObject users; // 创建用户对象
    // 优先尝试主文件，失败时尝试备份文件
    if (!loadFrom(filePath, users)) {
        if (!loadFrom(backupPath, users)) {
            return false; // 如果加载备份文件失败，则返回false
        }
    }

    if (!users.contains(username)) {
        return false; // 如果用户不存在，则返回false
    }

    QJsonObject userObj = users[username].toObject(); // 获取用户对象
    password = userObj["password"].toString();
    return true; // 返回true
}

bool JsonHelper::userExists(const QString &username) // 检查用户是否存在
{
    QString password; // 创建密码字符串
    return loadUser(username, password);
}

bool JsonHelper::deleteUser(const QString &username) // 删除用户
{
    const QString filePath = getUserFilePath(); // 获取用户文件路径 
    QFile file(filePath); // 打开文件
    QJsonObject users; // 创建用户对象

    // 读取现有用户
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取JSON数据
        if (doc.isObject()) {
            users = doc.object(); // 设置用户对象
        }
        file.close();
    }

    // 检查用户是否存在
    if (!users.contains(username)) {
        return false; // 用户不存在
    }

    // 删除用户
    users.remove(username);

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

void JsonHelper::initializeDefaultUsers()
{
    // 只保证 admin 商家账号存在；如果用户文件存在，同时移除旧的 user 用户
    const QString filePath = getUserFilePath();
    QFile file(filePath); // 打开文件

    QJsonObject users; // 创建用户对象

    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取JSON数据
        file.close(); // 关闭文件
        if (doc.isObject()) {
            users = doc.object(); // 设置用户对象
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

bool JsonHelper::saveDishes(const QVector<Dish> &dishes) // 保存菜品
{
    const QString filePath = getDishFilePath(); // 获取菜品文件路径
    QFile file(filePath); // 打开文件
    QJsonArray dishArray;

    for (const Dish &dish : dishes) { // 遍历菜品
        QJsonObject dishObj;
        dishObj["id"] = dish.getId();
        dishObj["name"] = dish.getName();
        dishObj["category"] = dish.getCategory();
        dishObj["price"] = dish.getPrice();
        dishObj["description"] = dish.getDescription();
        dishObj["imagePath"] = dish.getImagePath();
        dishArray.append(dishObj);
    }

    QJsonObject root; // 创建菜品对象
    root["dishes"] = dishArray;

    // 保存前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak";
        QFile::remove(backupPath); // 删除备份文件
        QFile::copy(filePath, backupPath); // 复制文件到备份文件
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root);
        file.write(doc.toJson()); // 写入JSON数据
        file.close(); // 关闭文件
        return true; // 返回true
    }
    return false; // 返回false
}

QVector<Dish> JsonHelper::loadDishes() // 加载菜品
{
    QVector<Dish> dishes; // 创建菜品对象
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

        QJsonDocument doc = QJsonDocument::fromJson(backupFile.readAll()); // 读取JSON数据
        backupFile.close();
        if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
            return dishes;
        }

        QJsonObject root = doc.object(); // 设置菜品对象
        QJsonArray dishArray = root["dishes"].toArray(); // 设置菜品数组

        for (int i = 0; i < dishArray.size(); ++i) {
            const QJsonObject dishObj = dishArray.at(i).toObject(); // 获取菜品对象
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

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取JSON数据
    file.close();

    if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
        return dishes;
    }

    QJsonObject root = doc.object(); // 设置菜品对象
    QJsonArray dishArray = root["dishes"].toArray(); // 设置菜品数组

    for (int i = 0; i < dishArray.size(); ++i) {
        const QJsonObject dishObj = dishArray.at(i).toObject(); // 获取菜品对象
        Dish dish;
        dish.setId(dishObj["id"].toInt());
        dish.setName(dishObj["name"].toString());
        dish.setCategory(dishObj["category"].toString());
        dish.setPrice(dishObj["price"].toDouble());
        dish.setDescription(dishObj["description"].toString());
        dish.setImagePath(dishObj["imagePath"].toString());
        dishes.append(dish); // 添加菜品
    }

    return dishes; // 返回菜品对象
}

bool JsonHelper::addDish(const Dish &dish)
{
    QVector<Dish> dishes = loadDishes(); // 加载菜品
    int maxId = 0; // 创建最大ID
    for (int i = 0; i < dishes.size(); ++i) {
        const Dish &d = dishes.at(i);
        if (d.getId() > maxId) {
            maxId = d.getId();
        }
    }
    Dish newDish = dish; // 创建新菜品
    newDish.setId(maxId + 1); // 设置新菜品ID
    dishes.append(newDish); // 添加新菜品
    return saveDishes(dishes); // 保存菜品
}

bool JsonHelper::updateDish(int id, const Dish &dish) // 更新菜品
{
    QVector<Dish> dishes = loadDishes(); // 加载菜品
    for (int i = 0; i < dishes.size(); ++i) {
        if (dishes[i].getId() == id) {
            Dish updatedDish = dish; // 创建更新菜品
            updatedDish.setId(id);
            dishes[i] = updatedDish; // 设置更新菜品
            return saveDishes(dishes);
        }
    }
    return false;
}

bool JsonHelper::deleteDish(int id) // 删除菜品
{
    QVector<Dish> dishes = loadDishes(); // 加载菜品
    for (int i = 0; i < dishes.size(); ++i) {
        if (dishes[i].getId() == id) {
            dishes.removeAt(i); // 删除菜品
            return saveDishes(dishes); // 保存菜品
        }
    }
    return false;
}

QJsonObject JsonHelper::orderToJson(const QVector<OrderItem> &orderItems, const QString &username)
{
    // 将订单项转换为JSON对象
    QJsonObject orderObj; // 创建订单对象
    orderObj["username"] = username; // 设置用户名
    QJsonArray itemsArray; // 创建订单项数组
    for (const OrderItem &item : orderItems) { // 遍历订单项
        QJsonObject itemObj; // 创建订单项对象
        itemObj["dishId"] = item.getDish().getId(); // 设置菜品ID
        itemObj["dishName"] = item.getDish().getName(); // 设置菜品名称
        itemObj["quantity"] = item.getQuantity(); // 设置数量
        itemObj["price"] = item.getDish().getPrice(); // 设置价格
        itemsArray.append(itemObj); // 添加订单项对象
    }
    orderObj["items"] = itemsArray; // 设置订单项数组
    return orderObj; // 返回订单对象
}

bool JsonHelper::saveOrder(const Order &order) // 保存订单
{
    const QString filePath = getOrderFilePath(); // 获取订单文件路径
    QFile file(filePath); // 打开文件
    QJsonArray orderArray; // 创建订单数组

    // 读取现有订单
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); // 读取JSON数据
        if (doc.isObject()) {
            QJsonObject root = doc.object(); // 设置订单对象
            orderArray = root["orders"].toArray();
        }
        file.close(); // 关闭文件
    }

    // 转换为JSON
    QJsonObject orderObj; // 创建订单对象
    orderObj["orderNo"] = order.getOrderNo(); // 设置订单号
    orderObj["username"] = order.getUsername(); // 设置用户名
    orderObj["tableNumber"] = order.getTableNumber(); // 设置桌号
    orderObj["customerCount"] = order.getCustomerCount(); // 设置顾客数量
    orderObj["status"] = order.getStatusString(); // 设置状态
    orderObj["createTime"] = order.getCreateTime().toString(Qt::ISODate); // 设置创建时间

    QJsonArray itemsArray; // 创建订单项数组
    for (const OrderItem &item : order.getItems()) {
        QJsonObject itemObj; // 创建订单项对象
        itemObj["dishId"] = item.getDish().getId(); // 设置菜品ID
        itemObj["dishName"] = item.getDish().getName(); // 设置菜品名称
        itemObj["quantity"] = item.getQuantity(); // 设置数量
        itemObj["price"] = item.getDish().getPrice(); // 设置价格
        itemsArray.append(itemObj); // 添加订单项对象
    }
    orderObj["items"] = itemsArray; // 设置订单项数组

    // 检查是否已存在，如果存在则更新，否则添加
    bool found = false;
    for (int i = 0; i < orderArray.size(); ++i) {
        const QJsonObject existing = orderArray[i].toObject(); // 获取订单对象
        const QString existingOrderNo = existing.contains("orderNo")
                                            ? existing["orderNo"].toString()
                                            : QString(); // 获取订单号
        if (!existingOrderNo.isEmpty() && existingOrderNo == order.getOrderNo()) {
            orderArray[i] = orderObj; // 设置订单对象
            found = true; // 设置已找到
            break;
        }
    }
    if (!found) {
        orderArray.append(orderObj); // 添加订单对象
    }

    // 保存到文件前先备份旧文件
    if (QFile::exists(filePath)) {
        const QString backupPath = filePath + ".bak"; // 获取备份文件路径
        QFile::remove(backupPath); // 删除备份文件
        QFile::copy(filePath, backupPath); // 复制文件到备份文件
    }

    QJsonObject root; // 创建订单对象
    root["orders"] = orderArray; // 设置订单数组
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(root); // 创建JSON文档
        file.write(doc.toJson()); // 写入JSON数据
        file.close(); // 关闭文件
        return true;
    }
    return false;
}

QVector<Order> JsonHelper::loadOrders() // 加载订单
{
    QVector<Order> orders; // 创建订单对象
    const QString filePath = getOrderFilePath(); // 获取订单文件路径
    const QString backupPath = filePath + ".bak"; // 获取备份文件路径
    QFile file(filePath); // 打开文件

    if (!file.exists()) { // 如果文件不存在，则返回false
        return orders;
    }

    auto parseOrders = [&](QIODevice &device, QJsonObject &rootOut) -> bool {
        QJsonDocument doc = QJsonDocument::fromJson(device.readAll()); // 读取JSON数据
        if (!doc.isObject()) { // 如果JSON数据不是对象，则返回false
            return false;
        }
        rootOut = doc.object(); // 设置订单对象
        return true;
    };

    QJsonObject root; // 创建订单对象

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

    QJsonArray orderArray = root["orders"].toArray(); // 获取订单数组

    QVector<Dish> allDishes = loadDishes(); // 加载菜品

    for (int i = 0; i < orderArray.size(); ++i) {
        const QJsonObject orderObj = orderArray.at(i).toObject(); // 获取订单对象
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

bool JsonHelper::updateOrderStatus(const QString &orderNo, OrderStatus status) // 更新订单状态
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

QString JsonHelper::getNextOrderNo(const QDate &date) // 获取下一个订单号
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

bool JsonHelper::deleteCompletedOrders() // 删除已完成订单
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
