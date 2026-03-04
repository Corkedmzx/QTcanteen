#ifndef REGISTERWINDOW_H
#define REGISTERWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class RegisterWindow : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterWindow(QWidget *parent = nullptr);
    ~RegisterWindow();

signals:
    void registerSuccess(const QString &username);

private slots:
    void onRegisterClicked();
    void onCancelClicked();

private:
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_confirmPasswordEdit;
    QPushButton *m_registerBtn;
    QPushButton *m_cancelBtn;

    void setupUI();
    bool validateInput();
};

#endif // REGISTERWINDOW_H
