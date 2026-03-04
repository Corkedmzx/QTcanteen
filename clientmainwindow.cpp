#include "clientmainwindow.h"
#include "jsonhelper.h"
#include "order.h"
#include "orderdetaildialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QHeaderView>
#include <QScrollArea>
#include <QGroupBox>
#include <QGridLayout>
#include <QSpacerItem>
#include <QTableWidgetItem>
#include <QSet>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QIntValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStatusBar>
#include <QTabBar>
#include <QEvent>

ClientMainWindow::ClientMainWindow(const QString &username, QWidget *parent)
    : QMainWindow(parent), m_username(username)
{
    setWindowTitle("餐厅点餐系统 - 客户端");
    setFixedSize(1000, 700);

    m_networkClient = new NetworkClient(this);
    connect(m_networkClient, &NetworkClient::orderConfirmed, this, &ClientMainWindow::onOrderConfirmed);
    connect(m_networkClient, &NetworkClient::checkoutSuccess, this, &ClientMainWindow::onCheckoutSuccess);
    connect(m_networkClient, &NetworkClient::messageReceived, this, &ClientMainWindow::onMessageReceived);
    connect(m_networkClient, &NetworkClient::connected, this, &ClientMainWindow::onNetworkConnected);
    connect(m_networkClient, &NetworkClient::disconnected, this, &ClientMainWindow::onNetworkDisconnected);
    // 连接错误信号用于调试
    connect(m_networkClient, &NetworkClient::errorOccurred, this, &ClientMainWindow::onNetworkError);

    // 自动连接服务器
    m_networkClient->connectToServer();

    setupUI();
    loadDishes();
}

ClientMainWindow::~ClientMainWindow()
{
}

void ClientMainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 顶部栏
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *welcomeLabel = new QLabel(QString("欢迎，%1").arg(m_username), this);
    QFont welcomeFont = welcomeLabel->font();
    welcomeFont.setPointSize(14);
    welcomeFont.setBold(true);
    welcomeLabel->setFont(welcomeFont);
    topLayout->addWidget(welcomeLabel);
    topLayout->addStretch();

    m_logoutBtn = new QPushButton("退出", this);
    m_logoutBtn->setFixedSize(100, 35);
    m_logoutBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 5px; }"
                               "QPushButton:hover { background-color: #da190b; }");
    connect(m_logoutBtn, &QPushButton::clicked, this, &ClientMainWindow::onLogoutClicked);
    topLayout->addWidget(m_logoutBtn);

    mainLayout->addLayout(topLayout);

    // 标签页
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #ddd; background-color: white; }"
                               "QTabBar::tab { background-color: #f0f0f0; padding: 10px 20px; }"
                               "QTabBar::tab:selected { background-color: white; border-bottom: 2px solid #4CAF50; }"
                               "QTabBar::tab:contains(●) { color: #333; }"); // 为包含圆点的标签设置样式

    // 菜单标签页
    m_menuTab = new QWidget();
    QVBoxLayout *menuLayout = new QVBoxLayout(m_menuTab);
    menuLayout->setContentsMargins(15, 15, 15, 15);
    menuLayout->setSpacing(15);

    // 分类选择
    QHBoxLayout *categoryLayout = new QHBoxLayout();
    QLabel *categoryLabel = new QLabel("菜品分类:", m_menuTab);
    categoryLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_categoryCombo = new QComboBox(m_menuTab);
    m_categoryCombo->addItem("全部");
    m_categoryCombo->setFixedHeight(35);
    m_categoryCombo->setStyleSheet("QComboBox { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ClientMainWindow::onCategoryChanged);
    categoryLayout->addWidget(categoryLabel);
    categoryLayout->addWidget(m_categoryCombo);
    categoryLayout->addStretch();
    menuLayout->addLayout(categoryLayout);

    // 菜品展示区域（使用滚动区域）
    m_scrollArea = new QScrollArea(m_menuTab);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: 1px solid #ddd; border-radius: 4px; background-color: white; }");
    m_dishContainer = new QWidget();
    m_dishLayout = new QVBoxLayout(m_dishContainer);
    m_dishLayout->setSpacing(10);
    m_dishLayout->setContentsMargins(10, 10, 10, 10);

    // 固定一个网格布局用于菜品展示
    m_menuGridLayout = new QGridLayout();
    m_menuGridLayout->setSpacing(15);
    m_menuGridLayout->setContentsMargins(0, 0, 0, 0);
    m_dishLayout->addLayout(m_menuGridLayout);
    m_dishLayout->addStretch();

    m_scrollArea->setWidget(m_dishContainer);
    menuLayout->addWidget(m_scrollArea);

    m_tabWidget->addTab(m_menuTab, "菜单");

    // 订单标签页
    m_orderTab = new QWidget();
    QVBoxLayout *orderLayout = new QVBoxLayout(m_orderTab);
    orderLayout->setContentsMargins(15, 15, 15, 15);
    orderLayout->setSpacing(15);

    QLabel *orderTitle = new QLabel("我的订单", m_orderTab);
    QFont orderTitleFont = orderTitle->font();
    orderTitleFont.setPointSize(16);
    orderTitleFont.setBold(true);
    orderTitle->setFont(orderTitleFont);
    orderLayout->addWidget(orderTitle);

    // 桌编号和客户人数输入
    QHBoxLayout *infoLayout = new QHBoxLayout();
    QLabel *tableLabel = new QLabel("桌编号:", m_orderTab);
    tableLabel->setStyleSheet("font-weight: bold;");
    m_tableNumberEdit = new QLineEdit(m_orderTab);
    m_tableNumberEdit->setPlaceholderText("请输入桌编号（如：1、A1等）");
    m_tableNumberEdit->setFixedHeight(35);
    m_tableNumberEdit->setStyleSheet("QLineEdit { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    
    QLabel *customerLabel = new QLabel("客户人数:", m_orderTab);
    customerLabel->setStyleSheet("font-weight: bold;");
    m_customerCountEdit = new QLineEdit(m_orderTab);
    m_customerCountEdit->setPlaceholderText("请输入客户人数");
    m_customerCountEdit->setFixedHeight(35);
    m_customerCountEdit->setValidator(new QIntValidator(1, 99, m_customerCountEdit));
    m_customerCountEdit->setStyleSheet("QLineEdit { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    
    infoLayout->addWidget(tableLabel);
    infoLayout->addWidget(m_tableNumberEdit);
    infoLayout->addSpacing(20);
    infoLayout->addWidget(customerLabel);
    infoLayout->addWidget(m_customerCountEdit);
    infoLayout->addStretch();
    orderLayout->addLayout(infoLayout);

    // 订单表格
    m_orderTable = new QTableWidget(m_orderTab);
    m_orderTable->setColumnCount(5);
    m_orderTable->setHorizontalHeaderLabels(QStringList() << "菜品名称" << "分类" << "单价" << "数量" << "小计");
    m_orderTable->horizontalHeader()->setStretchLastSection(true);
    m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_orderTable->setAlternatingRowColors(true);
    m_orderTable->setStyleSheet("QTableWidget { border: 1px solid #ddd; border-radius: 4px; gridline-color: #e0e0e0; }"
                                "QTableWidget::item { padding: 8px; }"
                                "QHeaderView::section { background-color: #f5f5f5; padding: 8px; border: none; }");
    m_orderTable->verticalHeader()->setVisible(false);
    orderLayout->addWidget(m_orderTable);

    // 总计和按钮
    QHBoxLayout *totalLayout = new QHBoxLayout();
    m_totalLabel = new QLabel("总计: ¥0.00", m_orderTab);
    QFont totalFont = m_totalLabel->font();
    totalFont.setPointSize(16);
    totalFont.setBold(true);
    m_totalLabel->setFont(totalFont);
    m_totalLabel->setStyleSheet("color: #f44336;");
    totalLayout->addWidget(m_totalLabel);
    totalLayout->addStretch();

    m_removeBtn = new QPushButton("移除选中", m_orderTab);
    m_removeBtn->setFixedSize(100, 40);
    m_removeBtn->setStyleSheet("QPushButton { background-color: #ff9800; color: white; border-radius: 5px; }"
                               "QPushButton:hover { background-color: #f57c00; }");
    connect(m_removeBtn, &QPushButton::clicked, this, &ClientMainWindow::onRemoveDishClicked);

    m_clearAllBtn = new QPushButton("一键清空", m_orderTab);
    m_clearAllBtn->setFixedSize(100, 40);
    m_clearAllBtn->setStyleSheet("QPushButton { background-color: #9e9e9e; color: white; border-radius: 5px; }"
                               "QPushButton:hover { background-color: #757575; }");
    connect(m_clearAllBtn, &QPushButton::clicked, this, &ClientMainWindow::onClearAllClicked);

    m_confirmBtn = new QPushButton("确认订单", m_orderTab);
    m_confirmBtn->setFixedSize(100, 40);
    m_confirmBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #0b7dda; }");
    connect(m_confirmBtn, &QPushButton::clicked, this, &ClientMainWindow::onConfirmOrderClicked);

    totalLayout->addWidget(m_removeBtn);
    totalLayout->addWidget(m_clearAllBtn);
    totalLayout->addWidget(m_confirmBtn);
    orderLayout->addLayout(totalLayout);

    // 添加订单标签页
    m_tabWidget->addTab(m_orderTab, "订单");
    
    // 创建红色圆点徽章（浮动在标签页右上角）
    m_orderBadge = new QLabel(m_tabWidget->tabBar());
    m_orderBadge->setAlignment(Qt::AlignCenter);
    m_orderBadge->setAttribute(Qt::WA_TransparentForMouseEvents); // 允许鼠标事件穿透
    m_orderBadge->raise(); // 置于最上层
    m_orderBadge->hide(); // 初始隐藏
    
    // 为标签栏安装事件过滤器，以便在大小变化时更新徽章位置
    m_tabWidget->tabBar()->installEventFilter(this);
    
    // 使用定时器更新徽章位置（因为标签页位置可能在布局后变化）
    QTimer::singleShot(100, this, [this]() {
        updateOrderTabBadge();
    });

    mainLayout->addWidget(m_tabWidget);

    // 设置整体样式
    setStyleSheet("QMainWindow { background-color: #fafafa; }");
    
    // 初始化订单标签页指示器
    updateOrderTabBadge();
    
    // 添加状态栏
    statusBar()->setStyleSheet("QStatusBar { background-color: #f5f5f5; border-top: 1px solid #ddd; }");
    statusBar()->showMessage("准备就绪", 2000);
}

void ClientMainWindow::loadDishes()
{
    m_allDishes = JsonHelper::loadDishes();

    // 收集所有分类
    QSet<QString> categories;
    for (const Dish &dish : m_allDishes) {
        categories.insert(dish.getCategory());
    }

    // 更新分类下拉框
    QString currentCategory = m_categoryCombo->currentText();
    m_categoryCombo->clear();
    m_categoryCombo->addItem("全部");
    for (const QString &category : categories) {
        m_categoryCombo->addItem(category);
    }
    if (currentCategory != "全部" && categories.contains(currentCategory)) {
        m_categoryCombo->setCurrentText(currentCategory);
    }

    updateMenuDisplay();
}

void ClientMainWindow::updateMenuDisplay()
{
    // 清除网格布局中的旧菜品卡片（保留布局本身）
    QLayoutItem *item;
    while ((item = m_menuGridLayout->takeAt(0)) != nullptr) {
        if (QWidget *w = item->widget()) {
            delete w;
        }
        delete item;
    }

    QString selectedCategory = m_categoryCombo->currentText();

    int row = 0, col = 0;
    const int colsPerRow = 4;

    // 收集要显示的菜品
    QVector<const Dish*> dishesToShow;
    for (const Dish &dish : m_allDishes) {
        if (selectedCategory == "全部" || dish.getCategory() == selectedCategory) {
            dishesToShow.append(&dish);
        }
    }

    // 按4列布局添加菜品
    for (const Dish *dish : dishesToShow) {
        QWidget *dishWidget = createDishWidget(*dish);
        m_menuGridLayout->addWidget(dishWidget, row, col);

        col++;
        if (col >= colsPerRow) {
            col = 0;
            row++;
        }
    }

    // 如果最后一行不满4列，添加弹性空间填充
    if (col > 0 && col < colsPerRow) {
        for (int i = col; i < colsPerRow; i++) {
            m_menuGridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), row, i);
        }
    }
}

QWidget* ClientMainWindow::createDishWidget(const Dish &dish)
{
    QGroupBox *dishBox = new QGroupBox(m_dishContainer);
    dishBox->setStyleSheet("QGroupBox { border: 1px solid #ddd; border-radius: 8px; padding: 10px; background-color: white; }"
                          "QGroupBox:hover { border: 2px solid #4CAF50; }");

    QVBoxLayout *boxLayout = new QVBoxLayout(dishBox);
    boxLayout->setSpacing(8);
    boxLayout->setContentsMargins(10, 10, 10, 10);

    // 图片显示
    QLabel *imageLabel = new QLabel(dishBox);
    imageLabel->setFixedSize(200, 150);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setScaledContents(true); // 允许图片缩放以适应标签大小
    imageLabel->setStyleSheet("border: 1px solid #ddd; border-radius: 4px; background-color: #f9f9f9;");
    
    // 尝试加载图片
    QString imagePath = dish.getImagePath();
    if (!imagePath.isEmpty()) {
        QString fullPath;
        bool found = false;
        
        // 处理相对路径和绝对路径
        QFileInfo fileInfo(imagePath);
        
        if (fileInfo.isAbsolute()) {
            // 绝对路径，直接使用
            fullPath = imagePath;
            if (QFileInfo::exists(fullPath)) {
                found = true;
            }
        } else {
            // 相对路径，尝试多个可能的位置
            QStringList searchPaths;
            
            // 1. 相对于可执行文件所在目录
            QString appDir = QCoreApplication::applicationDirPath();
            searchPaths << appDir;
            
            // 2. 相对于可执行文件所在目录向上查找项目根目录
            QDir appDirObj(appDir);
            // 向上查找，直到找到包含 images 目录的目录（项目根目录）
            QDir tempDir = appDirObj;
            int maxLevels = 5; // 最多向上查找5级
            for (int i = 0; i < maxLevels; ++i) {
                if (QDir(tempDir.absoluteFilePath("images")).exists()) {
                    searchPaths << tempDir.absolutePath();
                    break;
                }
                if (!tempDir.cdUp()) {
                    break;
                }
            }
            
            // 3. 相对于当前工作目录
            searchPaths << QDir::current().absolutePath();
            
            // 4. 相对于当前工作目录向上查找项目根目录
            QDir currentDir = QDir::current();
            // 向上查找，直到找到包含 images 目录的目录（项目根目录）
            QDir tempDir2 = currentDir;
            int maxLevels2 = 5; // 最多向上查找5级
            for (int i = 0; i < maxLevels2; ++i) {
                if (QDir(tempDir2.absoluteFilePath("images")).exists()) {
                    searchPaths << tempDir2.absolutePath();
                    break;
                }
                if (!tempDir2.cdUp()) {
                    break;
                }
            }
            
            // 尝试每个路径
            for (const QString &basePath : searchPaths) {
                QString testPath = QDir(basePath).absoluteFilePath(imagePath);
                if (QFileInfo::exists(testPath)) {
                    fullPath = testPath;
                    found = true;
                    qDebug() << "找到图片:" << fullPath;
                    break;
                }
            }
        }
        
        // 加载图片
        if (found) {
            QPixmap pixmap(fullPath);
            if (!pixmap.isNull()) {
                // 图片加载成功，显示图片
                imageLabel->setPixmap(pixmap.scaled(200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                imageLabel->setStyleSheet("border: 1px solid #ddd; border-radius: 4px;");
            } else {
                // 图片文件存在但加载失败
                qDebug() << "图片文件存在但加载失败:" << fullPath;
                imageLabel->setText("图片格式错误\n" + imagePath);
                imageLabel->setStyleSheet("border: 1px dashed #ccc; border-radius: 4px; background-color: #f9f9f9; color: #999;");
            }
        } else {
            // 图片文件不存在
            qDebug() << "图片文件未找到，尝试的路径:";
            qDebug() << "  原始路径:" << imagePath;
            qDebug() << "  应用程序目录:" << QCoreApplication::applicationDirPath();
            qDebug() << "  当前工作目录:" << QDir::current().absolutePath();
            imageLabel->setText("图片未找到\n" + imagePath);
            imageLabel->setStyleSheet("border: 1px dashed #ccc; border-radius: 4px; background-color: #f9f9f9; color: #999;");
        }
    } else {
        // 没有图片路径，显示占位文本
        imageLabel->setText("暂无图片\n(200x150)");
        imageLabel->setStyleSheet("border: 1px dashed #ccc; border-radius: 4px; background-color: #f9f9f9; color: #999;");
    }
    
    boxLayout->addWidget(imageLabel);

    // 菜品名称
    QLabel *nameLabel = new QLabel(dish.getName(), dishBox);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(14);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    boxLayout->addWidget(nameLabel);

    // 分类和价格
    QHBoxLayout *infoLayout = new QHBoxLayout();
    QLabel *categoryLabel = new QLabel(dish.getCategory(), dishBox);
    categoryLabel->setStyleSheet("color: #666; font-size: 12px;");
    QLabel *priceLabel = new QLabel(QString("¥%1").arg(dish.getPrice(), 0, 'f', 2), dishBox);
    priceLabel->setStyleSheet("color: #f44336; font-size: 14px; font-weight: bold;");
    infoLayout->addWidget(categoryLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(priceLabel);
    boxLayout->addLayout(infoLayout);

    // 描述
    if (!dish.getDescription().isEmpty()) {
        QLabel *descLabel = new QLabel(dish.getDescription(), dishBox);
        descLabel->setStyleSheet("color: #999; font-size: 11px;");
        descLabel->setWordWrap(true);
        boxLayout->addWidget(descLabel);
    }

    // 添加按钮
    QPushButton *addBtn = new QPushButton("+ 加入订单", dishBox);
    addBtn->setProperty("dishId", dish.getId());
    addBtn->setFixedHeight(35);
    addBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                         "QPushButton:hover { background-color: #45a049; }");
    connect(addBtn, &QPushButton::clicked, this, &ClientMainWindow::onAddDishClicked);
    boxLayout->addWidget(addBtn);

    dishBox->setFixedSize(200, 280);

    return dishBox;
}

void ClientMainWindow::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    updateMenuDisplay();
}

void ClientMainWindow::onAddDishClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    int dishId = btn->property("dishId").toInt();
    Dish selectedDish;
    for (const Dish &dish : m_allDishes) {
        if (dish.getId() == dishId) {
            selectedDish = dish;
            break;
        }
    }

    if (selectedDish.getId() == 0) return;

    // 检查是否已在订单中
    bool found = false;
    for (int i = 0; i < m_orderItems.size(); ++i) {
        if (m_orderItems[i].getDish().getId() == dishId) {
            m_orderItems[i].setQuantity(m_orderItems[i].getQuantity() + 1);
            found = true;
            break;
        }
    }

    if (!found) {
        m_orderItems.append(OrderItem(selectedDish, 1));
    }

    // 更新订单显示，但不自动切换到订单标签页
    updateOrderDisplay();
}

void ClientMainWindow::onRemoveDishClicked()
{
    int row = m_orderTable->currentRow();
    if (row < 0 || row >= m_orderItems.size()) {
        QMessageBox::information(this, "提示", "请选择要移除的菜品！");
        return;
    }

    m_orderItems.removeAt(row);
    updateOrderDisplay();
}

void ClientMainWindow::onClearAllClicked()
{
    if (m_orderItems.isEmpty()) {
        QMessageBox::information(this, "提示", "当前没有已加入的菜品！");
        return;
    }

    int ret = QMessageBox::question(this, "一键清空", "确定要清空当前订单中的所有菜品吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_orderItems.clear();
        updateOrderDisplay();
        QMessageBox::information(this, "提示", "已清空当前订单中的所有菜品。");
    }
}

void ClientMainWindow::onConfirmOrderClicked()
{
    if (m_orderItems.isEmpty()) {
        QMessageBox::warning(this, "错误", "订单为空，无法确认！");
        return;
    }

    // 验证桌编号和客户人数
    QString tableNumber = m_tableNumberEdit->text().trimmed();
    if (tableNumber.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入桌编号！");
        m_tableNumberEdit->setFocus();
        return;
    }

    QString customerCountStr = m_customerCountEdit->text().trimmed();
    if (customerCountStr.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入客户人数！");
        m_customerCountEdit->setFocus();
        return;
    }

    bool ok;
    int customerCount = customerCountStr.toInt(&ok);
    if (!ok || customerCount < 1) {
        QMessageBox::warning(this, "错误", "请输入有效的客户人数（1-99）！");
        m_customerCountEdit->setFocus();
        return;
    }

    // 创建订单（暂不保存），在订单详情页中确认结账后再保存
    const QString orderNo = JsonHelper::getNextOrderNo();
    Order order(orderNo, m_username, m_orderItems, OrderStatus::Pending);
    order.setTableNumber(tableNumber);
    order.setCustomerCount(customerCount);

    // 弹出订单详情对话框
    OrderDetailDialog detailDialog(order, this);
    if (detailDialog.exec() == QDialog::Accepted) {
        // 用户在详情页点击了"结账"，此时保存订单并通过网络发送
        if (JsonHelper::saveOrder(order)) {
            // 通过TCP发送订单（如果已连接）
            if (m_networkClient && m_networkClient->isConnected()) {
                QJsonObject orderObj;
                orderObj["orderNo"] = order.getOrderNo();
                orderObj["username"] = order.getUsername();
                orderObj["tableNumber"] = order.getTableNumber();
                orderObj["customerCount"] = order.getCustomerCount();
                orderObj["status"] = order.getStatusString();
                orderObj["createTime"] = order.getCreateTime().toString(Qt::ISODate);
                orderObj["totalAmount"] = order.getTotalAmount();
                
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
                
                m_networkClient->sendOrder(orderObj);
            }
            
            QMessageBox::information(this, "成功", QString("订单已提交！订单号：%1").arg(orderNo));

            // 清空当前订单和输入框
            m_orderItems.clear();
            if (m_tableNumberEdit) {
                m_tableNumberEdit->clear();
            }
            if (m_customerCountEdit) {
                m_customerCountEdit->clear();
            }
            updateOrderDisplay();
        } else {
            QMessageBox::critical(this, "错误", "订单提交失败，请重试！");
        }
    }
}

void ClientMainWindow::onOrderConfirmed()
{
    QMessageBox::information(this, "成功", "订单已确认！");
}

void ClientMainWindow::onCheckoutSuccess(double totalAmount)
{
    QMessageBox::information(this, "结账成功", QString("结账成功！\n总计: ¥%1").arg(totalAmount, 0, 'f', 2));
    m_orderItems.clear();
    updateOrderDisplay();
}

void ClientMainWindow::onNetworkError(const QString &error)
{
    // 网络错误时给出提示，同时记录日志
    qDebug() << "网络错误:" << error;
    statusBar()->showMessage("⚠ 与服务器通信出现问题，当前改为本地模式处理订单", 5000);
}

void ClientMainWindow::onMessageReceived(const QString &message)
{
    // 处理服务器消息
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        QJsonObject json = doc.object();
        QString type = json["type"].toString();
        
        if (type == "dish_update") {
            // 收到菜品更新通知，刷新菜品列表
            qDebug() << "收到菜品更新通知，刷新菜品列表";
            statusBar()->showMessage("✓ 菜品已更新，正在刷新菜单", 2000);
            loadDishes();
        } else if (type == "order_completed") {
            // 收到订单完成通知
            QString orderNo = json["orderNo"].toString();
            QString msg = json["message"].toString();
            if (msg.isEmpty()) {
                msg = QString("您的订单 %1 已完成！").arg(orderNo);
            }
            qDebug() << "收到订单完成通知：" << orderNo;
            QMessageBox::information(this, "订单完成", msg);
            statusBar()->showMessage(QString("✓ %1").arg(msg), 5000);
        }
    }
}

void ClientMainWindow::onNetworkConnected()
{
    qDebug() << "已连接到服务器";
    statusBar()->showMessage("✓ 已成功连接到商家端服务器，订单将实时同步", 3000);
}

void ClientMainWindow::onNetworkDisconnected()
{
    qDebug() << "与服务器断开连接";
    statusBar()->showMessage("⚠ 已与商家端服务器断开连接，当前订单将暂存在本地", 3000);
    // 不再自动重连，避免在服务器停止时创建不必要的连接
    // 如果服务器重新启动，用户可以手动刷新或重新打开客户端
}

void ClientMainWindow::onLogoutClicked()
{
    // 退出当前点餐窗口前给出确认提示
    int ret = QMessageBox::question(this, "确认退出", "确定要退出点餐吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (m_networkClient && m_networkClient->isConnected()) {
            m_networkClient->disconnectFromServer();
        }
        close();
    }
}

void ClientMainWindow::updateOrderDisplay()
{
    m_orderTable->setRowCount(m_orderItems.size());

    for (int i = 0; i < m_orderItems.size(); ++i) {
        const OrderItem &item = m_orderItems[i];
        const Dish &dish = item.getDish();

        m_orderTable->setItem(i, 0, new QTableWidgetItem(dish.getName()));
        m_orderTable->setItem(i, 1, new QTableWidgetItem(dish.getCategory()));
        m_orderTable->setItem(i, 2, new QTableWidgetItem(QString("¥%1").arg(dish.getPrice(), 0, 'f', 2)));
        m_orderTable->setItem(i, 3, new QTableWidgetItem(QString::number(item.getQuantity())));
        m_orderTable->setItem(i, 4, new QTableWidgetItem(QString("¥%1").arg(item.getTotalPrice(), 0, 'f', 2)));

        // 设置对齐方式
        for (int j = 0; j < 5; ++j) {
            QTableWidgetItem *tableItem = m_orderTable->item(i, j);
            if (tableItem) {
                if (j == 2 || j == 4) {
                    tableItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                } else {
                    tableItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                }
            }
        }
    }

    // 调整列宽
    m_orderTable->resizeColumnsToContents();
    m_orderTable->setColumnWidth(0, 200);
    m_orderTable->setColumnWidth(1, 100);
    m_orderTable->setColumnWidth(2, 100);
    m_orderTable->setColumnWidth(3, 80);

    calculateTotal();
    updateOrderTabBadge(); // 更新订单标签页的红色圆点
}

void ClientMainWindow::updateOrderTabBadge()
{
    // 计算订单中所有菜品的总数量
    int totalQuantity = 0;
    for (const OrderItem &item : m_orderItems) {
        totalQuantity += item.getQuantity();
    }
    
    // 更新红色圆点徽章
    if (totalQuantity > 0 && m_orderBadge) {
        // 显示徽章并设置数字
        m_orderBadge->setText(QString::number(totalQuantity));
        
        // 根据数字位数调整徽章大小
        int badgeWidth = 20;
        int badgeHeight = 20;
        int fontSize = 11;
        
        if (totalQuantity >= 100) {
            badgeWidth = 26;
            badgeHeight = 20;
            fontSize = 9;
        } else if (totalQuantity >= 10) {
            badgeWidth = 22;
            badgeHeight = 20;
            fontSize = 10;
        }
        
        m_orderBadge->setFixedSize(badgeWidth, badgeHeight);
        m_orderBadge->setStyleSheet(QString("QLabel { "
                                           "background-color: #ff0000; "
                                           "color: white; "
                                           "border-radius: %1px; "
                                           "font-size: %2px; "
                                           "font-weight: bold; "
                                           "padding: 0px; "
                                           "}").arg(badgeHeight / 2).arg(fontSize));
        
        // 计算徽章位置（订单标签页的右上角）
        QTabBar *tabBar = m_tabWidget->tabBar();
        if (tabBar && tabBar->count() > 1) {
            QRect tabRect = tabBar->tabRect(1); // 订单标签页是索引1
            int x = tabRect.right() - badgeWidth - 5; // 距离右边缘5px
            int y = tabRect.top() + 3; // 距离顶部3px
            m_orderBadge->move(x, y);
            m_orderBadge->show();
            m_orderBadge->raise(); // 确保在最上层
        }
    } else if (m_orderBadge) {
        // 隐藏徽章
        m_orderBadge->hide();
    }
}

bool ClientMainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 当标签栏大小变化时，更新徽章位置
    if (obj == m_tabWidget->tabBar() && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        if (m_orderBadge && m_orderBadge->isVisible()) {
            updateOrderTabBadge();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

double ClientMainWindow::calculateTotal()
{
    double total = 0.0;
    for (const OrderItem &item : m_orderItems) {
        total += item.getTotalPrice();
    }
    m_totalLabel->setText(QString("总计: ¥%1").arg(total, 0, 'f', 2));
    return total;
}
