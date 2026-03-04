#ifndef ORDERDETAILDIALOG_H
#define ORDERDETAILDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "order.h"

class OrderDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrderDetailDialog(const Order &order, QWidget *parent = nullptr);
    ~OrderDetailDialog() = default;

private:
    void setupUI();
    void populateTable();

    Order m_order;
    QTableWidget *m_table;
    QLabel *m_totalLabel;
    QPushButton *m_checkoutBtn;
    QPushButton *m_backBtn;
};

#endif // ORDERDETAILDIALOG_H

