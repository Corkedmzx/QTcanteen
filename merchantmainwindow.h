#ifndef MERCHANTMAINWINDOW_H
#define MERCHANTMAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QTabWidget>
#include <QTimer>
#include "dish.h"
#include "order.h"
#include "networkserver.h"

class MerchantMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MerchantMainWindow(const QString &username, QWidget *parent = nullptr);
    ~MerchantMainWindow();

private slots:
    void onAddDishClicked();
    void onEditDishClicked();
    void onDeleteDishClicked();
    void onTableSelectionChanged();
    void onLogoutClicked();
    void refreshDishList();
    void refreshOrders();
    void deleteCompletedOrders();
    void onNewOrderReceived(const QJsonObject &orderJson);
    void onCreateAccountClicked();
    void onEnrollFaceClicked();
    void onFaceEnrolled(const QString &username, const QString &faceImagePath);
    void onCompleteOrderClicked(const QString &orderNo);

private:
    void setupUI();
    void setupDishManagementTab();
    void setupOrderManagementTab();
    void setupAccountManagementTab();
    void loadDishes();
    void loadOrders();
    void updateDishTable();
    void updateOrderTable();
    void clearForm();
    void fillForm(const Dish &dish);
    Dish getDishFromForm();
    bool validateForm();
    void sendOrderCompletedNotification(const QString &orderNo);
    void loadAccountList();

    QString m_username;
    QVector<Dish> m_dishes;
    QVector<Order> m_orders;
    int m_lastOrderCount;

    // UI组件
    QTabWidget *m_tabWidget;
    QWidget *m_dishTab;
    QWidget *m_orderTab;
    QWidget *m_accountTab;
    
    // 菜品管理
    QTableWidget *m_dishTable;
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_logoutBtn;

    // 表单组件
    QLineEdit *m_nameEdit;
    QComboBox *m_categoryCombo;
    QDoubleSpinBox *m_priceSpin;
    QTextEdit *m_descriptionEdit;
    QLineEdit *m_imagePathEdit;
    
    // 订单管理
    QTableWidget *m_orderTable;
    QPushButton *m_refreshOrderBtn;
    QLabel *m_newOrderLabel;
    QTimer *m_refreshTimer;
    NetworkServer *m_networkServer;
    
    // 账户下发
    QLineEdit *m_newUsernameEdit;
    QLineEdit *m_newPasswordEdit;
    QLineEdit *m_confirmPasswordEdit;
    QPushButton *m_createAccountBtn;
    QTableWidget *m_accountTable;
    QPushButton *m_enrollFaceBtn;
    QComboBox *m_faceAccountCombo;
};

#endif // MERCHANTMAINWINDOW_H
