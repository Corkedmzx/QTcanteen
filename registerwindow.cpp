#include "registerwindow.h"
#include "jsonhelper.h"
#include <QMessageBox>

RegisterWindow::RegisterWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("注册");
    setFixedSize(400, 350);
    setModal(true);

    setupUI();
}

RegisterWindow::~RegisterWindow()
{
}

void RegisterWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // 标题
    QLabel *titleLabel = new QLabel("用户注册", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    mainLayout->addSpacing(10);

    // 用户名
    QLabel *usernameLabel = new QLabel("用户名:", this);
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText("请输入用户名");
    m_usernameEdit->setMinimumHeight(35);
    mainLayout->addWidget(usernameLabel);
    mainLayout->addWidget(m_usernameEdit);

    // 密码
    QLabel *passwordLabel = new QLabel("密码:", this);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText("请输入密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumHeight(35);
    mainLayout->addWidget(passwordLabel);
    mainLayout->addWidget(m_passwordEdit);

    // 确认密码
    QLabel *confirmPasswordLabel = new QLabel("确认密码:", this);
    m_confirmPasswordEdit = new QLineEdit(this);
    m_confirmPasswordEdit->setPlaceholderText("请再次输入密码");
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setMinimumHeight(35);
    mainLayout->addWidget(confirmPasswordLabel);
    mainLayout->addWidget(m_confirmPasswordEdit);

    mainLayout->addSpacing(10);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_registerBtn = new QPushButton("注册", this);
    m_registerBtn->setMinimumHeight(40);
    m_registerBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                                 "QPushButton:hover { background-color: #45a049; }");
    connect(m_registerBtn, &QPushButton::clicked, this, &RegisterWindow::onRegisterClicked);

    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setMinimumHeight(40);
    m_cancelBtn->setStyleSheet("QPushButton { background-color: #f0f0f0; border-radius: 5px; }"
                                "QPushButton:hover { background-color: #e0e0e0; }");
    connect(m_cancelBtn, &QPushButton::clicked, this, &RegisterWindow::onCancelClicked);

    btnLayout->addWidget(m_registerBtn);
    btnLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(btnLayout);

    // 设置样式
    setStyleSheet("QDialog { background-color: #fafafa; }"
                  "QLabel { color: #333; }"
                  "QLineEdit { border: 1px solid #ddd; border-radius: 4px; padding: 5px; }"
                  "QLineEdit:focus { border: 1px solid #4CAF50; }");
}

bool RegisterWindow::validateInput()
{
    if (m_usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入用户名！");
        m_usernameEdit->setFocus();
        return false;
    }
    if (m_passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入密码！");
        m_passwordEdit->setFocus();
        return false;
    }
    if (m_passwordEdit->text().length() < 6) {
        QMessageBox::warning(this, "错误", "密码长度至少6位！");
        m_passwordEdit->setFocus();
        return false;
    }
    if (m_passwordEdit->text() != m_confirmPasswordEdit->text()) {
        QMessageBox::warning(this, "错误", "两次输入的密码不一致！");
        m_confirmPasswordEdit->clear();
        m_confirmPasswordEdit->setFocus();
        return false;
    }
    return true;
}

void RegisterWindow::onRegisterClicked()
{
    if (!validateInput()) {
        return;
    }

    QString username = m_usernameEdit->text().trimmed();
    if (JsonHelper::userExists(username)) {
        QMessageBox::warning(this, "错误", "用户名已存在！");
        m_usernameEdit->clear();
        m_usernameEdit->setFocus();
        return;
    }

    if (JsonHelper::saveUser(username, m_passwordEdit->text())) {
        QMessageBox::information(this, "成功", "注册成功！");
        emit registerSuccess(username);
        accept();
    } else {
        QMessageBox::critical(this, "错误", "注册失败，请重试！");
    }
}

void RegisterWindow::onCancelClicked()
{
    reject();
}
