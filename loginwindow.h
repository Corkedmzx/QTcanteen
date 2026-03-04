#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSuccess(const QString &username, bool isMerchant);

private slots:
    void onOrderClicked();
    void onMerchantLoginClicked();

private:
    QPushButton *m_loginBtn;
    QPushButton *m_merchantLoginBtn;

    void setupUI();
};

#endif // LOGINWINDOW_H
