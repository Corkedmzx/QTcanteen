#ifndef CLIENTMAINWINDOW_H
#define CLIENTMAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QTimer>
#include "dish.h"
#include "orderitem.h"
#include "networkclient.h"

class ClientMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ClientMainWindow(const QString &username, QWidget *parent = nullptr);
    ~ClientMainWindow();

private slots:
    void onCategoryChanged(int index);
    void onAddDishClicked();
    void onRemoveDishClicked();
    void onConfirmOrderClicked();
    void onClearAllClicked();
    void onOrderConfirmed();
    void onCheckoutSuccess(double totalAmount);
    void onNetworkError(const QString &error);
    void onMessageReceived(const QString &message);
    void onNetworkConnected();
    void onNetworkDisconnected();
    void onLogoutClicked();

private:
    void setupUI();
    void loadDishes();
    void updateMenuDisplay();
    void updateOrderDisplay();
    void updateOrderTabBadge(); // 更新订单标签页的红色圆点指示器
    bool eventFilter(QObject *obj, QEvent *event) override; // 事件过滤器，用于更新徽章位置
    double calculateTotal();
    QWidget* createDishWidget(const Dish &dish);

    QString m_username;
    QVector<Dish> m_allDishes;
    QVector<OrderItem> m_orderItems;
    NetworkClient *m_networkClient;

    // UI组件
    QTabWidget *m_tabWidget;
    QWidget *m_menuTab;
    QWidget *m_orderTab;
    QLabel *m_orderBadge; // 红色圆点徽章（浮动在标签页右上角）

    // 菜单界面
    QComboBox *m_categoryCombo;
    QWidget *m_dishContainer;
    QVBoxLayout *m_dishLayout;
    QGridLayout *m_menuGridLayout;
    QScrollArea *m_scrollArea;

    // 订单界面
    QTableWidget *m_orderTable;
    QLabel *m_totalLabel;
    QLineEdit *m_tableNumberEdit; // 桌编号输入
    QLineEdit *m_customerCountEdit; // 客户人数输入
    QPushButton *m_removeBtn;
    QPushButton *m_clearAllBtn;
    QPushButton *m_confirmBtn;
    QPushButton *m_logoutBtn;
};

#endif // CLIENTMAINWINDOW_H
