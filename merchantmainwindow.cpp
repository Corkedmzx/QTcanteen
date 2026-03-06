#include "merchantmainwindow.h"
#include "jsonhelper.h"
#include "order.h"
#include "faceenrolldialog.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QTimer>
#include <QTableWidgetItem>
#include <QBrush>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QStatusBar>
#include <QFile>

MerchantMainWindow::MerchantMainWindow(const QString &username, QWidget *parent)
    : QMainWindow(parent), m_username(username), m_lastOrderCount(0)
{
    setWindowTitle("餐厅点餐系统 - 商家端");
    setFixedSize(1200, 700);

    setupUI();
    loadDishes();
    loadOrders();
    
    // 启动网络服务器（默认端口 8888）
    m_networkServer = new NetworkServer(this);
    connect(m_networkServer, &NetworkServer::newOrderReceived, this, &MerchantMainWindow::onNewOrderReceived);
    connect(m_networkServer, &NetworkServer::clientConnected, this, [this]() {
        statusBar()->showMessage("✓ 有新的客户端已连接到服务器", 3000);
    });
    connect(m_networkServer, &NetworkServer::clientDisconnected, this, [this]() {
        statusBar()->showMessage("⚠ 有客户端与服务器断开连接", 3000);
    });
    if (m_networkServer->startServer(8888)) {
        qDebug() << "商家端服务器启动成功";
        statusBar()->showMessage("✓ 网络服务器启动成功，监听端口 8888", 3000);
    } else {
        statusBar()->showMessage("⚠ 网络服务器启动失败，订单将通过文件同步", 5000);
    }
    
    // 设置定时刷新订单（每3秒刷新一次）
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &MerchantMainWindow::refreshOrders);
    m_refreshTimer->start(3000); // 3秒刷新一次
}

MerchantMainWindow::~MerchantMainWindow()
{
}

void MerchantMainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 顶部栏
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *welcomeLabel = new QLabel(QString("商家管理 - %1").arg(m_username), this);
    QFont welcomeFont = welcomeLabel->font();
    welcomeFont.setPointSize(14);
    welcomeFont.setBold(true);
    welcomeLabel->setFont(welcomeFont);
    topLayout->addWidget(welcomeLabel);
    topLayout->addStretch();

    m_logoutBtn = new QPushButton("退出登录", this);
    m_logoutBtn->setFixedSize(100, 35);
    m_logoutBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 5px; }"
                               "QPushButton:hover { background-color: #da190b; }");
    connect(m_logoutBtn, &QPushButton::clicked, this, &MerchantMainWindow::onLogoutClicked);
    topLayout->addWidget(m_logoutBtn);
    mainLayout->addLayout(topLayout);

    // 标签页
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet("QTabWidget::pane { border: 1px solid #ddd; background-color: white; }"
                               "QTabBar::tab { background-color: #f0f0f0; padding: 10px 20px; }"
                               "QTabBar::tab:selected { background-color: white; border-bottom: 2px solid #4CAF50; }");

    setupDishManagementTab();
    setupOrderManagementTab();
    setupAccountManagementTab();

    mainLayout->addWidget(m_tabWidget);

    // 设置整体样式
    setStyleSheet("QMainWindow { background-color: #fafafa; }"
                  "QLabel { color: #333; }"
                  "QLineEdit { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }"
                  "QLineEdit:focus { border: 1px solid #4CAF50; }");
    
    // 添加状态栏
    statusBar()->setStyleSheet("QStatusBar { background-color: #f5f5f5; border-top: 1px solid #ddd; }");
    statusBar()->showMessage("服务器已启动，等待客户端连接", 3000);
}

void MerchantMainWindow::setupDishManagementTab()
{
    // 菜品管理标签页的基础样式和布局配置
    m_dishTab = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(m_dishTab);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 左侧：菜品列表
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    m_refreshBtn = new QPushButton("刷新", m_dishTab);
    m_refreshBtn->setFixedSize(80, 35);
    m_refreshBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #0b7dda; }");
    connect(m_refreshBtn, &QPushButton::clicked, this, &MerchantMainWindow::refreshDishList);
    leftLayout->addWidget(m_refreshBtn);

    // 菜品表格
    m_dishTable = new QTableWidget(m_dishTab);
    m_dishTable->setColumnCount(6);
    m_dishTable->setHorizontalHeaderLabels(QStringList() << "ID" << "菜品名称" << "分类" << "价格" << "描述" << "图片路径");
    m_dishTable->horizontalHeader()->setStretchLastSection(true);
    m_dishTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dishTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dishTable->setAlternatingRowColors(true);
    m_dishTable->setStyleSheet("QTableWidget { border: 1px solid #ddd; border-radius: 4px; gridline-color: #e0e0e0; }"
                                "QTableWidget::item { padding: 8px; }"
                                "QHeaderView::section { background-color: #f5f5f5; padding: 8px; border: none; }");
    m_dishTable->verticalHeader()->setVisible(false);
    connect(m_dishTable, &QTableWidget::itemSelectionChanged, this, &MerchantMainWindow::onTableSelectionChanged);
    leftLayout->addWidget(m_dishTable);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_addBtn = new QPushButton("添加", m_dishTab);
    m_addBtn->setFixedSize(100, 40);
    m_addBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                            "QPushButton:hover { background-color: #45a049; }");
    connect(m_addBtn, &QPushButton::clicked, this, &MerchantMainWindow::onAddDishClicked);

    m_editBtn = new QPushButton("修改", m_dishTab);
    m_editBtn->setFixedSize(100, 40);
    m_editBtn->setEnabled(false);
    m_editBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                             "QPushButton:hover { background-color: #0b7dda; }"
                             "QPushButton:disabled { background-color: #ccc; }");
    connect(m_editBtn, &QPushButton::clicked, this, &MerchantMainWindow::onEditDishClicked);

    m_deleteBtn = new QPushButton("删除", m_dishTab);
    m_deleteBtn->setFixedSize(100, 40);
    m_deleteBtn->setEnabled(false);
    m_deleteBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #da190b; }"
                              "QPushButton:disabled { background-color: #ccc; }");
    connect(m_deleteBtn, &QPushButton::clicked, this, &MerchantMainWindow::onDeleteDishClicked);

    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_editBtn);
    btnLayout->addWidget(m_deleteBtn);
    btnLayout->addStretch();

    leftLayout->addLayout(btnLayout);

    // 右侧：表单
    QGroupBox *formGroup = new QGroupBox("菜品信息", m_dishTab);
    formGroup->setStyleSheet("QGroupBox { border: 1px solid #ddd; border-radius: 8px; padding: 15px; background-color: white; font-weight: bold; }");
    QVBoxLayout *formLayout = new QVBoxLayout(formGroup);
    formLayout->setSpacing(12);

    // 菜品名称
    QLabel *nameLabel = new QLabel("菜品名称:", formGroup);
    m_nameEdit = new QLineEdit(formGroup);
    m_nameEdit->setPlaceholderText("请输入菜品名称");
    m_nameEdit->setMinimumHeight(35);
    formLayout->addWidget(nameLabel);
    formLayout->addWidget(m_nameEdit);

    // 分类
    QLabel *categoryLabel = new QLabel("分类:", formGroup);
    m_categoryCombo = new QComboBox(formGroup);
    m_categoryCombo->addItems(QStringList() << "热菜" << "凉菜" << "主食" << "汤品" << "饮品" << "其他");
    m_categoryCombo->setEditable(true);
    m_categoryCombo->setMinimumHeight(35);
    m_categoryCombo->setStyleSheet("QComboBox { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    formLayout->addWidget(categoryLabel);
    formLayout->addWidget(m_categoryCombo);

    // 价格
    QLabel *priceLabel = new QLabel("价格:", formGroup);
    m_priceSpin = new QDoubleSpinBox(formGroup);
    m_priceSpin->setMinimum(0.0);
    m_priceSpin->setMaximum(9999.99);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSuffix(" 元");
    m_priceSpin->setMinimumHeight(35);
    m_priceSpin->setStyleSheet("QDoubleSpinBox { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    formLayout->addWidget(priceLabel);
    formLayout->addWidget(m_priceSpin);

    // 描述
    QLabel *descLabel = new QLabel("描述:", formGroup);
    m_descriptionEdit = new QTextEdit(formGroup);
    m_descriptionEdit->setPlaceholderText("请输入菜品描述");
    m_descriptionEdit->setMaximumHeight(80);
    m_descriptionEdit->setStyleSheet("QTextEdit { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }");
    formLayout->addWidget(descLabel);
    formLayout->addWidget(m_descriptionEdit);

    // 图片路径
    QLabel *imageLabel = new QLabel("图片路径:", formGroup);
    m_imagePathEdit = new QLineEdit(formGroup);
    m_imagePathEdit->setPlaceholderText("示例：images/dish1.jpg 或 C:/images/dish1.png 或 ./pics/food.jpg");
    m_imagePathEdit->setMinimumHeight(35);
    formLayout->addWidget(imageLabel);
    formLayout->addWidget(m_imagePathEdit);
    
    // 添加提示标签
    QLabel *imageHintLabel = new QLabel("提示：支持相对路径（如 images/xxx.jpg）或绝对路径（如 C:/images/xxx.jpg）", formGroup);
    imageHintLabel->setStyleSheet("color: #666; font-size: 11px; padding-top: 3px;");
    imageHintLabel->setWordWrap(true);
    imageHintLabel->setMinimumHeight(30);
    formLayout->addSpacing(5);
    formLayout->addWidget(imageHintLabel);

    formLayout->addStretch();

    mainLayout->addLayout(leftLayout, 2);
    mainLayout->addWidget(formGroup, 1);

    m_tabWidget->addTab(m_dishTab, "菜单管理");
}

void MerchantMainWindow::setupOrderManagementTab()
{
    // 订单管理标签页的基础样式和布局配置
    m_orderTab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(m_orderTab);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 顶部：新订单提示和刷新按钮
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_newOrderLabel = new QLabel("", m_orderTab);
    m_newOrderLabel->setStyleSheet("color: #f44336; font-size: 14px; font-weight: bold;");
    topLayout->addWidget(m_newOrderLabel);
    topLayout->addStretch();

    m_refreshOrderBtn = new QPushButton("刷新订单", m_orderTab);
    m_refreshOrderBtn->setFixedSize(100, 35);
    m_refreshOrderBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                                     "QPushButton:hover { background-color: #0b7dda; }");
    connect(m_refreshOrderBtn, &QPushButton::clicked, this, &MerchantMainWindow::refreshOrders);
    topLayout->addWidget(m_refreshOrderBtn);

    QPushButton *deleteCompletedBtn = new QPushButton("删除已完成", m_orderTab);
    deleteCompletedBtn->setFixedSize(110, 35);
    deleteCompletedBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 5px; }"
                                      "QPushButton:hover { background-color: #da190b; }");
    connect(deleteCompletedBtn, &QPushButton::clicked, this, &MerchantMainWindow::deleteCompletedOrders);
    topLayout->addWidget(deleteCompletedBtn);
    mainLayout->addLayout(topLayout);

    // 订单表格
    m_orderTable = new QTableWidget(m_orderTab);
    m_orderTable->setColumnCount(10);
    m_orderTable->setHorizontalHeaderLabels(QStringList() << "序号" << "订单号" << "用户名" << "桌号" << "人数" << "订单详情" << "总金额" << "状态" << "创建时间" << "操作");
    m_orderTable->horizontalHeader()->setStretchLastSection(true);
    m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_orderTable->setAlternatingRowColors(true);
    m_orderTable->setStyleSheet("QTableWidget { border: 1px solid #ddd; border-radius: 4px; gridline-color: #e0e0e0; }"
                                "QTableWidget::item { padding: 8px; }"
                                "QHeaderView::section { background-color: #f5f5f5; padding: 8px; border: none; }");
    m_orderTable->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_orderTable);

    m_tabWidget->addTab(m_orderTab, "订单管理");
}

void MerchantMainWindow::setupAccountManagementTab()
{
    // 账户下发管理标签页的基础样式和布局配置
    m_accountTab = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(m_accountTab);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel("账户下发管理", m_accountTab);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // 创建账户表单
    QGroupBox *createGroup = new QGroupBox("创建新账户", m_accountTab);
    createGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #ddd; border-radius: 5px; margin-top: 10px; padding-top: 10px; }"
                               "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *formLayout = new QVBoxLayout(createGroup);
    formLayout->setSpacing(12);

    // 账号输入
    QHBoxLayout *usernameLayout = new QHBoxLayout();
    QLabel *usernameLabel = new QLabel("账号：", createGroup);
    usernameLabel->setFixedWidth(80);
    m_newUsernameEdit = new QLineEdit(createGroup);
    m_newUsernameEdit->setPlaceholderText("请输入新账号（用于分店经理登录）");
    m_newUsernameEdit->setMinimumHeight(36);
    usernameLayout->addWidget(usernameLabel);
    usernameLayout->addWidget(m_newUsernameEdit);
    formLayout->addLayout(usernameLayout);

    // 密码输入
    QHBoxLayout *passwordLayout = new QHBoxLayout();
    QLabel *passwordLabel = new QLabel("密码：", createGroup);
    passwordLabel->setFixedWidth(80);
    m_newPasswordEdit = new QLineEdit(createGroup);
    m_newPasswordEdit->setPlaceholderText("请输入密码");
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setMinimumHeight(36);
    passwordLayout->addWidget(passwordLabel);
    passwordLayout->addWidget(m_newPasswordEdit);
    formLayout->addLayout(passwordLayout);

    // 确认密码输入
    QHBoxLayout *confirmLayout = new QHBoxLayout();
    QLabel *confirmLabel = new QLabel("确认密码：", createGroup);
    confirmLabel->setFixedWidth(80);
    m_confirmPasswordEdit = new QLineEdit(createGroup);
    m_confirmPasswordEdit->setPlaceholderText("请再次输入密码");
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setMinimumHeight(36);
    confirmLayout->addWidget(confirmLabel);
    confirmLayout->addWidget(m_confirmPasswordEdit);
    formLayout->addLayout(confirmLayout);

    // 创建按钮
    m_createAccountBtn = new QPushButton("创建账户", createGroup);
    m_createAccountBtn->setFixedSize(120, 40);
    m_createAccountBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; font-size: 14px; }"
                                      "QPushButton:hover { background-color: #45a049; }");
    connect(m_createAccountBtn, &QPushButton::clicked, this, &MerchantMainWindow::onCreateAccountClicked);
    formLayout->addWidget(m_createAccountBtn, 0, Qt::AlignLeft);

    mainLayout->addWidget(createGroup);

    // 人脸录入
    QGroupBox *faceGroup = new QGroupBox("人脸录入", m_accountTab);
    faceGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #ddd; border-radius: 5px; margin-top: 10px; padding-top: 10px; }"
                             "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *faceLayout = new QVBoxLayout(faceGroup);
    faceLayout->setSpacing(12);

    QHBoxLayout *faceAccountLayout = new QHBoxLayout();
    QLabel *faceAccountLabel = new QLabel("选择账户：", faceGroup);
    faceAccountLabel->setFixedWidth(80);
    m_faceAccountCombo = new QComboBox(faceGroup);
    m_faceAccountCombo->setMinimumHeight(36);
    m_faceAccountCombo->setEditable(false);
    // 加载所有账户（包括admin和已创建的账户）
    QFile userFile("user.json");
    if (userFile.exists() && userFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(userFile.readAll());
        userFile.close();
        if (doc.isObject()) {
            QJsonObject users = doc.object();
            for (auto it = users.begin(); it != users.end(); ++it) {
                m_faceAccountCombo->addItem(it.key());
            }
        }
    }
    // 如果没有账户，至少添加admin
    if (m_faceAccountCombo->count() == 0) {
        m_faceAccountCombo->addItem("admin");
    }
    faceAccountLayout->addWidget(faceAccountLabel);
    faceAccountLayout->addWidget(m_faceAccountCombo);
    faceLayout->addLayout(faceAccountLayout);

#if defined(SEETAFACE_AVAILABLE) || defined(OPENCV_AVAILABLE)
    m_enrollFaceBtn = new QPushButton("录入人脸", faceGroup);
    m_enrollFaceBtn->setFixedSize(120, 40);
    m_enrollFaceBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; border-radius: 5px; font-size: 14px; }"
                                   "QPushButton:hover { background-color: #F57C00; }");
    connect(m_enrollFaceBtn, &QPushButton::clicked, this, &MerchantMainWindow::onEnrollFaceClicked);
    faceLayout->addWidget(m_enrollFaceBtn, 0, Qt::AlignLeft);
#else
    m_enrollFaceBtn = nullptr;
    QLabel *faceDisabledLabel = new QLabel("人脸识别功能不可用（需要安装 OpenCV 或 SeetaFace）", faceGroup);
    faceDisabledLabel->setStyleSheet("color: #999; font-size: 12px;");
    faceLayout->addWidget(faceDisabledLabel, 0, Qt::AlignLeft);
#endif

    mainLayout->addWidget(faceGroup);

    // 账户列表
    QGroupBox *listGroup = new QGroupBox("已创建账户列表", m_accountTab);
    listGroup->setStyleSheet("QGroupBox { font-weight: bold; border: 2px solid #ddd; border-radius: 5px; margin-top: 10px; padding-top: 10px; }"
                             "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *listLayout = new QVBoxLayout(listGroup);

    m_accountTable = new QTableWidget(listGroup);
    m_accountTable->setColumnCount(2);
    m_accountTable->setHorizontalHeaderLabels(QStringList() << "账号" << "创建时间");
    m_accountTable->horizontalHeader()->setStretchLastSection(true);
    m_accountTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_accountTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_accountTable->setAlternatingRowColors(true);
    m_accountTable->setStyleSheet("QTableWidget { border: 1px solid #ddd; border-radius: 4px; gridline-color: #e0e0e0; }"
                                  "QTableWidget::item { padding: 8px; }"
                                  "QHeaderView::section { background-color: #f5f5f5; padding: 8px; border: none; }");
    m_accountTable->verticalHeader()->setVisible(false);
    listLayout->addWidget(m_accountTable);

    mainLayout->addWidget(listGroup);

    mainLayout->addStretch();

    m_tabWidget->addTab(m_accountTab, "账户下发");
    
    // 加载账户列表
    loadAccountList();
}

void MerchantMainWindow::onCreateAccountClicked()
{
    // 创建新账户
    QString username = m_newUsernameEdit->text().trimmed();
    QString password = m_newPasswordEdit->text();
    QString confirmPassword = m_confirmPasswordEdit->text();

    // 验证输入
    if (username.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入账号！");
        m_newUsernameEdit->setFocus();
        return;
    }
    if (password.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入密码！");
        m_newPasswordEdit->setFocus();
        return;
    }
    if (password != confirmPassword) {
        QMessageBox::warning(this, "错误", "两次输入的密码不一致！");
        m_newPasswordEdit->clear();
        m_confirmPasswordEdit->clear();
        m_newPasswordEdit->setFocus();
        return;
    }

    // 检查账号是否已存在
    if (JsonHelper::userExists(username)) {
        QMessageBox::warning(this, "错误", "该账号已存在，请使用其他账号！");
        m_newUsernameEdit->setFocus();
        return;
    }

    // 保存新账户
    if (JsonHelper::saveUser(username, password)) {
        QMessageBox::information(this, "成功", QString("账户创建成功！\n账号：%1\n密码：%2\n\n请将此账号密码提供给分店经理使用。").arg(username).arg(password));
        
        // 清空表单
        m_newUsernameEdit->clear();
        m_newPasswordEdit->clear();
        m_confirmPasswordEdit->clear();
        
        // 刷新人脸录入账户下拉框和账户列表
        m_faceAccountCombo->clear();
        QFile userFile("user.json");
        if (userFile.exists() && userFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(userFile.readAll());
            userFile.close();
            if (doc.isObject()) {
                QJsonObject users = doc.object();
                for (auto it = users.begin(); it != users.end(); ++it) {
                    m_faceAccountCombo->addItem(it.key());
                }
            }
        }
        loadAccountList(); // 刷新账户列表
        
        statusBar()->showMessage(QString("✓ 账户 %1 创建成功").arg(username), 3000);
    } else {
        QMessageBox::critical(this, "错误", "账户创建失败，请重试！");
    }
}

void MerchantMainWindow::onEnrollFaceClicked()
{
#if defined(SEETAFACE_AVAILABLE) || defined(OPENCV_AVAILABLE)
    // 录入人脸
    QString selectedUsername = m_faceAccountCombo->currentText();
    if (selectedUsername.isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择要录入人脸的账户！");
        return;
    }

    // 检查账户是否存在
    if (!JsonHelper::userExists(selectedUsername)) {
        QMessageBox::warning(this, "错误", "所选账户不存在！");
        return;
    }

    // 打开人脸录入对话框
    FaceEnrollDialog *dialog = new FaceEnrollDialog(selectedUsername, this);
    connect(dialog, &FaceEnrollDialog::faceEnrolled, this, &MerchantMainWindow::onFaceEnrolled);
    dialog->exec();
    dialog->deleteLater();
#else
    QMessageBox::warning(this, "错误", "人脸识别功能不可用！\n\n请安装 OpenCV 或 SeetaFace6Open 库以启用人脸识别功能。");
#endif
}

void MerchantMainWindow::onFaceEnrolled(const QString &username, const QString &faceImagePath)
{
    // 人脸录入成功
    Q_UNUSED(faceImagePath);
    statusBar()->showMessage(QString("✓ 账户 %1 人脸录入成功").arg(username), 3000);
    QMessageBox::information(this, "成功", QString("账户 %1 的人脸录入成功！").arg(username));
}

void MerchantMainWindow::loadDishes()
{
    // 加载菜品列表
    m_dishes = JsonHelper::loadDishes();
    updateDishTable();
}

void MerchantMainWindow::updateDishTable()
{
    // 更新菜品表格
    m_dishTable->setRowCount(m_dishes.size());

    for (int i = 0; i < m_dishes.size(); ++i) {
        const Dish &dish = m_dishes[i];

        m_dishTable->setItem(i, 0, new QTableWidgetItem(QString::number(dish.getId())));
        m_dishTable->setItem(i, 1, new QTableWidgetItem(dish.getName()));
        m_dishTable->setItem(i, 2, new QTableWidgetItem(dish.getCategory()));
        m_dishTable->setItem(i, 3, new QTableWidgetItem(QString("¥%1").arg(dish.getPrice(), 0, 'f', 2)));
        m_dishTable->setItem(i, 4, new QTableWidgetItem(dish.getDescription()));
        m_dishTable->setItem(i, 5, new QTableWidgetItem(dish.getImagePath().isEmpty() ? "未设置" : dish.getImagePath()));

        // 设置对齐方式
        m_dishTable->item(i, 0)->setTextAlignment(Qt::AlignCenter);
        m_dishTable->item(i, 3)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    // 调整列宽
    m_dishTable->resizeColumnsToContents();
    m_dishTable->setColumnWidth(0, 60);
    m_dishTable->setColumnWidth(1, 150);
    m_dishTable->setColumnWidth(2, 100);
    m_dishTable->setColumnWidth(3, 100);
    m_dishTable->setColumnWidth(4, 200);
}

void MerchantMainWindow::onTableSelectionChanged()
{
    // 表格选择变化
    bool hasSelection = m_dishTable->currentRow() >= 0;
    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);

    if (hasSelection) {
        int row = m_dishTable->currentRow();
        if (row >= 0 && row < m_dishes.size()) {
            fillForm(m_dishes[row]);
        }
    } else {
        clearForm();
    }
}

void MerchantMainWindow::onAddDishClicked()
{
    // 添加菜品
    if (!validateForm()) {
        return;
    }

    Dish newDish = getDishFromForm();
    if (JsonHelper::addDish(newDish)) {
        QMessageBox::information(this, "成功", "菜品添加成功！");
        loadDishes();
        clearForm();
        // 广播菜品更新通知
        if (m_networkServer) {
            m_networkServer->broadcastDishUpdate();
        }
    } else {
        QMessageBox::critical(this, "错误", "添加失败，请重试！");
    }
}

void MerchantMainWindow::onEditDishClicked()
{
    // 修改菜品
    int row = m_dishTable->currentRow();
    if (row < 0 || row >= m_dishes.size()) {
        return;
    }

    if (!validateForm()) {
        return;
    }

    int dishId = m_dishes[row].getId();
    Dish updatedDish = getDishFromForm();
    if (JsonHelper::updateDish(dishId, updatedDish)) {
        QMessageBox::information(this, "成功", "菜品修改成功！");
        loadDishes();
        clearForm();
        // 广播菜品更新通知
        if (m_networkServer) {
            m_networkServer->broadcastDishUpdate();
        }
    } else {
        QMessageBox::critical(this, "错误", "修改失败，请重试！");
    }
}

void MerchantMainWindow::onDeleteDishClicked()
{
    // 删除菜品
    int row = m_dishTable->currentRow();
    if (row < 0 || row >= m_dishes.size()) {
        return;
    }

    QString dishName = m_dishes[row].getName();
    int ret = QMessageBox::question(this, "确认删除", QString("确定要删除菜品 \"%1\" 吗？").arg(dishName),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        int dishId = m_dishes[row].getId();
        if (JsonHelper::deleteDish(dishId)) {
            QMessageBox::information(this, "成功", "菜品删除成功！");
            loadDishes();
            clearForm();
            // 广播菜品更新通知
            if (m_networkServer) {
                m_networkServer->broadcastDishUpdate();
            }
        } else {
            QMessageBox::critical(this, "错误", "删除失败，请重试！");
        }
    }
}


void MerchantMainWindow::onLogoutClicked()
{
    // 确认退出
    int ret = QMessageBox::question(this, "确认退出", "确定要退出商家端并停止服务器吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        if (m_networkServer && m_networkServer->isListening()) {
            m_networkServer->stopServer();
            statusBar()->showMessage("服务器已停止，窗口即将关闭", 3000);
        }
        close();
    }
}

void MerchantMainWindow::refreshDishList()
{
    // 刷新菜品列表
    loadDishes();
    QMessageBox::information(this, "提示", "菜品列表已刷新！");
}

void MerchantMainWindow::loadOrders()
{
    // 加载订单列表
    m_orders = JsonHelper::loadOrders();
    updateOrderTable();
}

void MerchantMainWindow::updateOrderTable()
{
    m_orderTable->setRowCount(m_orders.size());

    int pendingCount = 0;
    for (int i = 0; i < m_orders.size(); ++i) {
        const Order &order = m_orders[i];
        if (order.getStatus() == OrderStatus::Pending) {
            pendingCount++;
        }

        // 订单序号（列表内重排：1..N）
        m_orderTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        // 订单号（MMDD+当日位次三位，如 0210001）
        m_orderTable->setItem(i, 1, new QTableWidgetItem(order.getOrderNo()));
        m_orderTable->setItem(i, 2, new QTableWidgetItem(order.getUsername()));
        // 桌号和人数
        m_orderTable->setItem(i, 3, new QTableWidgetItem(order.getTableNumber().isEmpty() ? "未设置" : order.getTableNumber()));
        m_orderTable->setItem(i, 4, new QTableWidgetItem(order.getCustomerCount() > 0 ? QString::number(order.getCustomerCount()) : "未设置"));
        
        // 订单详情
        QString details;
        for (const OrderItem &item : order.getItems()) {
            if (!details.isEmpty()) details += ", ";
            details += QString("%1 x%2").arg(item.getDish().getName()).arg(item.getQuantity());
        }
        m_orderTable->setItem(i, 5, new QTableWidgetItem(details));
        
        m_orderTable->setItem(i, 6, new QTableWidgetItem(QString("¥%1").arg(order.getTotalAmount(), 0, 'f', 2)));
        m_orderTable->setItem(i, 7, new QTableWidgetItem(order.getStatusString()));
        m_orderTable->setItem(i, 8, new QTableWidgetItem(order.getCreateTime().toString("yyyy-MM-dd hh:mm:ss")));

        // 设置对齐方式（添加空指针检查）
        QTableWidgetItem *item0 = m_orderTable->item(i, 0);
        if (item0) item0->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *item1 = m_orderTable->item(i, 1);
        if (item1) item1->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *item3 = m_orderTable->item(i, 3);
        if (item3) item3->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *item4 = m_orderTable->item(i, 4);
        if (item4) item4->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *item6 = m_orderTable->item(i, 6);
        if (item6) item6->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QTableWidgetItem *item7 = m_orderTable->item(i, 7);
        if (item7) item7->setTextAlignment(Qt::AlignCenter);

        // 根据状态设置颜色
        QTableWidgetItem *statusItem = m_orderTable->item(i, 7);
        if (statusItem) {
            if (order.getStatus() == OrderStatus::Pending) {
                statusItem->setBackground(QBrush(QColor(255, 235, 238))); // 浅红色
            } else if (order.getStatus() == OrderStatus::Processing) {
                statusItem->setBackground(QBrush(QColor(227, 242, 253))); // 浅蓝色
            } else if (order.getStatus() == OrderStatus::Completed) {
                statusItem->setBackground(QBrush(QColor(232, 245, 233))); // 浅绿色
            }
        }
        
        // 添加"确认完成"按钮
        QPushButton *completeBtn = new QPushButton("确认完成", m_orderTab);
        completeBtn->setFixedSize(80, 25);
        completeBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 3px; font-size: 11px; padding: 2px; }"
                                   "QPushButton:hover { background-color: #45a049; }"
                                   "QPushButton:disabled { background-color: #ccc; }");
        // 如果订单已完成或已取消，禁用按钮
        if (order.getStatus() == OrderStatus::Completed || order.getStatus() == OrderStatus::Cancelled) {
            completeBtn->setEnabled(false);
        }
        // 使用lambda表达式连接按钮点击事件，传递订单号
        connect(completeBtn, &QPushButton::clicked, this, [this, orderNo = order.getOrderNo()]() {
            onCompleteOrderClicked(orderNo);
        });
        m_orderTable->setCellWidget(i, 9, completeBtn);
    }

    // 设置表格行高，确保按钮能正常显示
    m_orderTable->verticalHeader()->setDefaultSectionSize(35);
    
    // 调整列宽
    m_orderTable->resizeColumnsToContents();
    m_orderTable->setColumnWidth(0, 70);
    m_orderTable->setColumnWidth(1, 110);
    m_orderTable->setColumnWidth(2, 100);
    m_orderTable->setColumnWidth(3, 80);
    m_orderTable->setColumnWidth(4, 60);
    m_orderTable->setColumnWidth(5, 300);
    m_orderTable->setColumnWidth(6, 100);
    m_orderTable->setColumnWidth(7, 80);
    m_orderTable->setColumnWidth(9, 90);

    // 更新新订单提示
    if (pendingCount > 0) {
        m_newOrderLabel->setText(QString("⚠ 有 %1 个待处理订单").arg(pendingCount));
        // 如果有新订单且当前不在订单标签页，可以切换标签页
        if (m_orders.size() > m_lastOrderCount && m_tabWidget->currentIndex() == 0) {
            // 可选：自动切换到订单标签页
            // m_tabWidget->setCurrentIndex(1);
        }
    } else {
        m_newOrderLabel->setText("");
    }
    m_lastOrderCount = m_orders.size();
}

void MerchantMainWindow::refreshOrders()
{
    // 刷新订单列表
    loadOrders();
}

void MerchantMainWindow::onCompleteOrderClicked(const QString &orderNo)
{
    // 确认完成订单
    int ret = QMessageBox::question(this, "确认完成", 
                                     QString("确定要将订单 %1 标记为已完成吗？").arg(orderNo),
                                     QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    
    // 更新订单状态为已完成
    if (JsonHelper::updateOrderStatus(orderNo, OrderStatus::Completed)) {
        QMessageBox::information(this, "成功", QString("订单 %1 已标记为已完成！").arg(orderNo));
        // 发送完成提示给客户端
        sendOrderCompletedNotification(orderNo);
        // 刷新订单列表
        loadOrders();
    } else {
        QMessageBox::critical(this, "错误", "更新失败，请重试！");
    }
}

void MerchantMainWindow::sendOrderCompletedNotification(const QString &orderNo)
{
    // 发送订单完成通知
    if (!m_networkServer) {
        return;
    }
    
    // 创建订单完成消息
    QJsonObject message;
    message["type"] = "order_completed";
    message["orderNo"] = orderNo;
    message["message"] = QString("您的订单 %1 已完成！").arg(orderNo);
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson();
    
    // 广播给所有客户端
    m_networkServer->broadcastMessage(data);
    qDebug() << "已发送订单完成通知：" << orderNo;
}

void MerchantMainWindow::loadAccountList()
{
    // 加载账户列表
    m_accountTable->setRowCount(0);
    
    QFile userFile("user.json");
    if (!userFile.exists() || !userFile.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(userFile.readAll());
    userFile.close();
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonObject users = doc.object();
    int row = 0;
    for (auto it = users.begin(); it != users.end(); ++it) {
        QString username = it.key();
        // 跳过商家账户（isMerchant为true的账户）
        QJsonObject userObj = it.value().toObject();
        if (userObj["isMerchant"].toBool()) {
            continue;
        }
        
        m_accountTable->insertRow(row);
        m_accountTable->setItem(row, 0, new QTableWidgetItem(username));
        
        // 尝试获取创建时间（如果存在）
        QString createTime = userObj["createTime"].toString();
        if (createTime.isEmpty()) {
            createTime = "未知";
        }
        m_accountTable->setItem(row, 1, new QTableWidgetItem(createTime));
        
        // 设置对齐方式
        QTableWidgetItem *item0 = m_accountTable->item(row, 0);
        if (item0) item0->setTextAlignment(Qt::AlignCenter);
        QTableWidgetItem *item1 = m_accountTable->item(row, 1);
        if (item1) item1->setTextAlignment(Qt::AlignCenter);
        
        row++;
    }
}

void MerchantMainWindow::deleteCompletedOrders()
{
    // 确认删除已完成订单
    int ret = QMessageBox::question(this, "确认删除", "确定要一键删除所有“已完成”的订单吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (JsonHelper::deleteCompletedOrders()) {
        QMessageBox::information(this, "成功", "已删除所有已完成订单。");
        loadOrders(); // 会自动重排“订单序号”
    } else {
        QMessageBox::critical(this, "错误", "删除失败，请重试！");
    }
}

void MerchantMainWindow::clearForm()
{
    // 清空表单
    m_nameEdit->clear();
    m_categoryCombo->setCurrentIndex(0);
    m_priceSpin->setValue(0.0);
    m_descriptionEdit->clear();
    m_imagePathEdit->clear();
    m_dishTable->clearSelection();
}

void MerchantMainWindow::fillForm(const Dish &dish)
{
    // 填充表单
    m_nameEdit->setText(dish.getName());
    int categoryIndex = m_categoryCombo->findText(dish.getCategory());
    if (categoryIndex >= 0) {
        m_categoryCombo->setCurrentIndex(categoryIndex);
    } else {
        m_categoryCombo->setCurrentText(dish.getCategory());
    }
    m_priceSpin->setValue(dish.getPrice());
    m_descriptionEdit->setPlainText(dish.getDescription());
    m_imagePathEdit->setText(dish.getImagePath());
}

Dish MerchantMainWindow::getDishFromForm()
{
    // 获取表单数据
    Dish dish;
    dish.setName(m_nameEdit->text().trimmed());
    dish.setCategory(m_categoryCombo->currentText().trimmed());
    dish.setPrice(m_priceSpin->value());
    dish.setDescription(m_descriptionEdit->toPlainText().trimmed());
    dish.setImagePath(m_imagePathEdit->text().trimmed());
    return dish;
}

bool MerchantMainWindow::validateForm()
{
    // 验证表单数据
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入菜品名称！");
        m_nameEdit->setFocus();
        return false;
    }
    if (m_categoryCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "请选择或输入分类！");
        m_categoryCombo->setFocus();
        return false;
    }
    if (m_priceSpin->value() <= 0) {
        QMessageBox::warning(this, "错误", "请输入有效的价格！");
        m_priceSpin->setFocus();
        return false;
    }
    return true;
}

void MerchantMainWindow::onNewOrderReceived(const QJsonObject &orderJson)
{
    // 收到新订单
    if (orderJson.isEmpty() || !orderJson.contains("orderNo")) {
        qDebug() << "收到无效的订单数据";
        return;
    }
    
    // 将JSON订单转换为Order对象
    Order order;
    order.setOrderNo(orderJson["orderNo"].toString());
    order.setUsername(orderJson["username"].toString());
    order.setTableNumber(orderJson.contains("tableNumber") ? orderJson["tableNumber"].toString() : "");
    order.setCustomerCount(orderJson.contains("customerCount") ? orderJson["customerCount"].toInt() : 0);
    order.setStatus(Order::statusFromString(orderJson.contains("status") ? orderJson["status"].toString() : "待处理"));
    
    QString timeStr = orderJson.contains("createTime") ? orderJson["createTime"].toString() : "";
    if (!timeStr.isEmpty()) {
        QDateTime createTime = QDateTime::fromString(timeStr, Qt::ISODate);
        if (createTime.isValid()) {
            order.setCreateTime(createTime);
        } else {
            order.setCreateTime(QDateTime::currentDateTime());
        }
    } else {
        order.setCreateTime(QDateTime::currentDateTime());
    }
    
    // 加载所有菜品信息
    QVector<Dish> allDishes = JsonHelper::loadDishes();
    QJsonArray itemsArray = orderJson.contains("items") ? orderJson["items"].toArray() : QJsonArray();
    QVector<OrderItem> items;
    for (const QJsonValue &itemValue : itemsArray) {
        if (!itemValue.isObject()) continue;
        QJsonObject itemObj = itemValue.toObject();
        int dishId = itemObj.contains("dishId") ? itemObj["dishId"].toInt() : 0;
        int quantity = itemObj.contains("quantity") ? itemObj["quantity"].toInt() : 0;
        
        if (dishId <= 0 || quantity <= 0) continue;
        
        bool found = false;
        for (const Dish &dish : allDishes) {
            if (dish.getId() == dishId) {
                items.append(OrderItem(dish, quantity));
                found = true;
                break;
            }
        }
        if (!found) {
            qDebug() << "警告：找不到ID为" << dishId << "的菜品";
        }
    }
    
    if (items.isEmpty()) {
        qDebug() << "警告：订单中没有有效的菜品项";
        return;
    }
    
    order.setItems(items);
    
    // 保存订单
    if (JsonHelper::saveOrder(order)) {
        // 刷新订单列表
        loadOrders();
        
        // 显示提示消息
        QString message = QString("收到新订单！\n订单号：%1\n桌号：%2\n客户人数：%3\n总计：¥%4")
                         .arg(order.getOrderNo())
                         .arg(order.getTableNumber().isEmpty() ? "未设置" : order.getTableNumber())
                         .arg(order.getCustomerCount() > 0 ? QString::number(order.getCustomerCount()) : "未设置")
                         .arg(order.getTotalAmount(), 0, 'f', 2);
        
        QMessageBox::information(this, "新订单", message);
        
        // 自动切换到订单标签页
        if (m_tabWidget) {
            m_tabWidget->setCurrentIndex(1);
        }
        
        // 高亮显示新订单
        updateOrderTable();
    } else {
        qDebug() << "保存订单失败";
        QMessageBox::warning(this, "错误", "保存订单失败！");
    }
}
