#include "loginwindow.h"
#include "clientmainwindow.h"
#include "merchantmainwindow.h"
#include "jsonhelper.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QDebug>
#include <exception>
#include <cstdlib>

int main(int argc, char *argv[])
{
    // 设置异常处理
    std::set_terminate([]() {
        qCritical() << "程序异常终止！";
        std::abort();
    });
    
    // 设置未捕获异常处理
    try {
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "QTcanteen_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // 初始化默认用户（如果用户文件不存在）
    JsonHelper::initializeDefaultUsers();

    // 创建入口窗口
    LoginWindow *loginWindow = new LoginWindow();

    // 连接信号：根据入口打开客户或商家窗口
    QObject::connect(loginWindow, &LoginWindow::loginSuccess, [=](const QString &username, bool isMerchant) {
        // 不隐藏登录窗口，允许同时打开多个窗口进行测试
        if (isMerchant) {
            MerchantMainWindow *merchantWindow = new MerchantMainWindow(username);
            merchantWindow->show();
        } else {
            ClientMainWindow *clientWindow = new ClientMainWindow(username);
            clientWindow->show();
        }
    });

    loginWindow->show();

    return a.exec();
    } catch (const std::exception &e) {
        qCritical() << "程序异常：" << e.what();
        return 1;
    } catch (...) {
        qCritical() << "程序未知异常";
        return 1;
    }
}
