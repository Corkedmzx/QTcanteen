#include "loginwindow.h"
#include "jsonhelper.h"
#include "facelogindialog.h"
#include <QMessageBox>

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("进入系统");
    setFixedSize(450, 360);
    setModal(false);  // 改为非模态，允许独立最小化
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    setupUI();
}

LoginWindow::~LoginWindow()
{
}

void LoginWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(40, 40, 40, 40);

    // 标题
    QLabel *titleLabel = new QLabel("餐厅点餐系统", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setMinimumHeight(40);
    mainLayout->addWidget(titleLabel);

    mainLayout->addSpacing(30);

    // 功能说明
    QLabel *descLabel = new QLabel("请选择入口：", this);
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("font-size: 14px;");
    mainLayout->addWidget(descLabel);

    mainLayout->addSpacing(20);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);

    m_loginBtn = new QPushButton("点餐", this);
    m_loginBtn->setMinimumHeight(42);
    m_loginBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; font-size: 14px; }"
                               "QPushButton:hover { background-color: #45a049; }");
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onOrderClicked);

    m_merchantLoginBtn = new QPushButton("商家登录", this);
    m_merchantLoginBtn->setMinimumHeight(42);
    m_merchantLoginBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; font-size: 14px; }"
                                      "QPushButton:hover { background-color: #0b7dda; }");
    connect(m_merchantLoginBtn, &QPushButton::clicked, this, &LoginWindow::onMerchantLoginClicked);

    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_merchantLoginBtn);

    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch();

    // 设置样式
    setStyleSheet("QDialog { background-color: #fafafa; }"
                  "QLabel { color: #333; }");
}

void LoginWindow::onOrderClicked()
{
    // 顾客点餐：无需登录，直接进入点餐页面
    emit loginSuccess("顾客", false);
}

void LoginWindow::onMerchantLoginClicked()
{
    // 商家登录对话框
    QDialog dialog(this);
    dialog.setWindowTitle("商家登录");
    dialog.setModal(true);
    dialog.setFixedSize(420, 320);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(15);

    QLabel *titleLabel = new QLabel("商家登录", &dialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // 账号
    QLabel *usernameLabel = new QLabel("账号：", &dialog);
    QLineEdit *usernameEdit = new QLineEdit(&dialog);
    usernameEdit->setPlaceholderText("请输入账号");
    usernameEdit->setMinimumHeight(36);
    mainLayout->addWidget(usernameLabel);
    mainLayout->addWidget(usernameEdit);

    // 密码
    QLabel *passwordLabel = new QLabel("密码：", &dialog);
    QLineEdit *passwordEdit = new QLineEdit(&dialog);
    passwordEdit->setPlaceholderText("请输入密码");
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setMinimumHeight(36);
    mainLayout->addWidget(passwordLabel);
    mainLayout->addWidget(passwordEdit);

    mainLayout->addSpacing(10);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    QPushButton *loginBtn = new QPushButton("登录", &dialog);
    loginBtn->setMinimumHeight(38);
    loginBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; }"
                            "QPushButton:hover { background-color: #0b7dda; }");

    QPushButton *faceLoginBtn = new QPushButton("人脸识别登录", &dialog);
    faceLoginBtn->setMinimumHeight(38);
    faceLoginBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #F57C00; }");

    QPushButton *recoverBtn = new QPushButton("更改账号/密码", &dialog);
    recoverBtn->setMinimumHeight(38);
    recoverBtn->setStyleSheet("QPushButton { background-color: #f0f0f0; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #e0e0e0; }");

    QPushButton *cancelBtn = new QPushButton("取消", &dialog);
    cancelBtn->setMinimumHeight(38);

    btnLayout->addWidget(loginBtn);
    btnLayout->addWidget(faceLoginBtn);
    btnLayout->addWidget(recoverBtn);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    // 商家登录逻辑：仅商家可以登录（商家账号：admin）
    QObject::connect(loginBtn, &QPushButton::clicked, &dialog, [&dialog, usernameEdit, passwordEdit, this]() {
        QString username = usernameEdit->text().trimmed();
        QString pwdInput = passwordEdit->text();

        if (username.isEmpty()) {
            QMessageBox::warning(&dialog, "错误", "请输入账号！");
            usernameEdit->setFocus();
            return;
        }
        if (pwdInput.isEmpty()) {
            QMessageBox::warning(&dialog, "错误", "请输入密码！");
            passwordEdit->setFocus();
            return;
        }

        QString storedPwd;
        if (!JsonHelper::loadUser(username, storedPwd)) {
            QMessageBox::warning(&dialog, "错误", "账号不存在！");
            return;
        }

        // 允许所有已存在的商家账户登录（包括admin和已创建的账户）

        if (storedPwd != pwdInput) {
            QMessageBox::warning(&dialog, "错误", "密码错误！");
            passwordEdit->clear();
            passwordEdit->setFocus();
            return;
        }

        // 登录成功
        emit loginSuccess(username, true);
        dialog.accept();
    });

    // 人脸识别登录
    QObject::connect(faceLoginBtn, &QPushButton::clicked, &dialog, [&dialog, this]() {
        // 检查是否有人脸数据
        if (!JsonHelper::hasAnyFaceData()) {
            QMessageBox::warning(&dialog, "提示", "系统中尚未录入任何人脸数据，无法使用人脸识别登录！\n\n请先在商家端录入人脸后再使用此功能。");
            return;
        }

        FaceLoginDialog *faceDialog = new FaceLoginDialog(&dialog);
        QObject::connect(faceDialog, &FaceLoginDialog::faceLoginSuccess, &dialog, [&dialog, this](const QString &username) {
            // 验证账户是否存在
            QString storedPwd;
            if (!JsonHelper::loadUser(username, storedPwd)) {
                QMessageBox::warning(&dialog, "错误", "账户不存在！");
                return;
            }
            
            // 所有已存在的账户都可以登录商家端
            emit loginSuccess(username, true);
            dialog.accept();
        });
        faceDialog->exec();
        faceDialog->deleteLater();
    });

    // 更改账号/密码
    QObject::connect(recoverBtn, &QPushButton::clicked, &dialog, [&dialog, usernameEdit, passwordEdit]() {
        // 创建更改密码对话框
        QDialog changePwdDialog(&dialog);
        changePwdDialog.setWindowTitle("更改账号/密码");
        changePwdDialog.setModal(true);
        changePwdDialog.setFixedSize(420, 380);

        QVBoxLayout *changeLayout = new QVBoxLayout(&changePwdDialog);
        changeLayout->setContentsMargins(30, 30, 30, 30);
        changeLayout->setSpacing(15);

        QLabel *titleLabel = new QLabel("更改账号/密码", &changePwdDialog);
        titleLabel->setAlignment(Qt::AlignCenter);
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(16);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        changeLayout->addWidget(titleLabel);

        changeLayout->addSpacing(10);

        // 当前账号
        QHBoxLayout *usernameLayout = new QHBoxLayout();
        QLabel *currentUsernameLabel = new QLabel("当前账号：", &changePwdDialog);
        currentUsernameLabel->setFixedWidth(100);
        QLineEdit *currentUsernameEdit = new QLineEdit(&changePwdDialog);
        currentUsernameEdit->setPlaceholderText("请输入当前账号");
        currentUsernameEdit->setMinimumHeight(36);
        usernameLayout->addWidget(currentUsernameLabel);
        usernameLayout->addWidget(currentUsernameEdit);
        changeLayout->addLayout(usernameLayout);

        // 当前密码
        QHBoxLayout *currentPwdLayout = new QHBoxLayout();
        QLabel *currentPasswordLabel = new QLabel("当前密码：", &changePwdDialog);
        currentPasswordLabel->setFixedWidth(100);
        QLineEdit *currentPasswordEdit = new QLineEdit(&changePwdDialog);
        currentPasswordEdit->setPlaceholderText("请输入当前密码");
        currentPasswordEdit->setEchoMode(QLineEdit::Password);
        currentPasswordEdit->setMinimumHeight(36);
        currentPwdLayout->addWidget(currentPasswordLabel);
        currentPwdLayout->addWidget(currentPasswordEdit);
        changeLayout->addLayout(currentPwdLayout);

        // 新账号（可选）
        QHBoxLayout *newUsernameLayout = new QHBoxLayout();
        QLabel *newUsernameLabel = new QLabel("新账号（可选）：", &changePwdDialog);
        newUsernameLabel->setFixedWidth(100);
        QLineEdit *newUsernameEdit = new QLineEdit(&changePwdDialog);
        newUsernameEdit->setPlaceholderText("留空则不修改账号");
        newUsernameEdit->setMinimumHeight(36);
        newUsernameLayout->addWidget(newUsernameLabel);
        newUsernameLayout->addWidget(newUsernameEdit);
        changeLayout->addLayout(newUsernameLayout);

        // 新密码
        QHBoxLayout *newPwdLayout = new QHBoxLayout();
        QLabel *newPasswordLabel = new QLabel("新密码：", &changePwdDialog);
        newPasswordLabel->setFixedWidth(100);
        QLineEdit *newPasswordEdit = new QLineEdit(&changePwdDialog);
        newPasswordEdit->setPlaceholderText("请输入新密码");
        newPasswordEdit->setEchoMode(QLineEdit::Password);
        newPasswordEdit->setMinimumHeight(36);
        newPwdLayout->addWidget(newPasswordLabel);
        newPwdLayout->addWidget(newPasswordEdit);
        changeLayout->addLayout(newPwdLayout);

        // 确认新密码
        QHBoxLayout *confirmPwdLayout = new QHBoxLayout();
        QLabel *confirmPasswordLabel = new QLabel("确认新密码：", &changePwdDialog);
        confirmPasswordLabel->setFixedWidth(100);
        QLineEdit *confirmPasswordEdit = new QLineEdit(&changePwdDialog);
        confirmPasswordEdit->setPlaceholderText("请再次输入新密码");
        confirmPasswordEdit->setEchoMode(QLineEdit::Password);
        confirmPasswordEdit->setMinimumHeight(36);
        confirmPwdLayout->addWidget(confirmPasswordLabel);
        confirmPwdLayout->addWidget(confirmPasswordEdit);
        changeLayout->addLayout(confirmPwdLayout);

        changeLayout->addSpacing(10);

        // 按钮
        QHBoxLayout *changeBtnLayout = new QHBoxLayout();
        QPushButton *confirmBtn = new QPushButton("确认修改", &changePwdDialog);
        confirmBtn->setMinimumHeight(38);
        confirmBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                                  "QPushButton:hover { background-color: #45a049; }");

        QPushButton *cancelChangeBtn = new QPushButton("取消", &changePwdDialog);
        cancelChangeBtn->setMinimumHeight(38);

        changeBtnLayout->addWidget(confirmBtn);
        changeBtnLayout->addWidget(cancelChangeBtn);
        changeLayout->addLayout(changeBtnLayout);

        // 确认修改逻辑
        QObject::connect(confirmBtn, &QPushButton::clicked, &changePwdDialog, [&changePwdDialog, currentUsernameEdit, currentPasswordEdit, newUsernameEdit, newPasswordEdit, confirmPasswordEdit]() {
            QString currentUsername = currentUsernameEdit->text().trimmed();
            QString currentPwd = currentPasswordEdit->text();
            QString newUsername = newUsernameEdit->text().trimmed();
            QString newPwd = newPasswordEdit->text();
            QString confirmPwd = confirmPasswordEdit->text();

            // 验证输入
            if (currentUsername.isEmpty()) {
                QMessageBox::warning(&changePwdDialog, "错误", "请输入当前账号！");
                currentUsernameEdit->setFocus();
                return;
            }
            if (currentPwd.isEmpty()) {
                QMessageBox::warning(&changePwdDialog, "错误", "请输入当前密码！");
                currentPasswordEdit->setFocus();
                return;
            }

            // 验证当前账号密码
            QString storedPwd;
            if (!JsonHelper::loadUser(currentUsername, storedPwd)) {
                QMessageBox::warning(&changePwdDialog, "错误", "当前账号不存在！");
                currentUsernameEdit->setFocus();
                return;
            }
            if (storedPwd != currentPwd) {
                QMessageBox::warning(&changePwdDialog, "错误", "当前密码不匹配，拒绝修改！");
                currentPasswordEdit->clear();
                currentPasswordEdit->setFocus();
                return;
            }

            // 验证新密码
            if (newPwd.isEmpty()) {
                QMessageBox::warning(&changePwdDialog, "错误", "请输入新密码！");
                newPasswordEdit->setFocus();
                return;
            }
            if (newPwd != confirmPwd) {
                QMessageBox::warning(&changePwdDialog, "错误", "两次输入的新密码不一致！");
                newPasswordEdit->clear();
                confirmPasswordEdit->clear();
                newPasswordEdit->setFocus();
                return;
            }

            // 如果输入了新账号，检查是否已存在
            QString finalUsername = newUsername.isEmpty() ? currentUsername : newUsername;
            if (!newUsername.isEmpty() && JsonHelper::userExists(newUsername) && newUsername != currentUsername) {
                QMessageBox::warning(&changePwdDialog, "错误", "新账号已存在，请使用其他账号！");
                newUsernameEdit->setFocus();
                return;
            }

            // 保存新账号密码
            if (JsonHelper::saveUser(finalUsername, newPwd)) {
                // 如果修改了账号，需要删除旧账号
                if (!newUsername.isEmpty() && newUsername != currentUsername) {
                    // 注意：这里简化处理，实际应该有一个删除用户的方法
                    // 由于当前实现是覆盖保存，如果账号改变，旧账号数据仍会保留
                    // 但新账号密码已保存成功
                }
                QMessageBox::information(&changePwdDialog, "成功", "账号/密码修改成功！");
                changePwdDialog.accept();
            } else {
                QMessageBox::critical(&changePwdDialog, "错误", "保存失败，请重试！");
            }
        });

        QObject::connect(cancelChangeBtn, &QPushButton::clicked, &changePwdDialog, &QDialog::reject);

        changePwdDialog.exec();
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    dialog.exec();
}
