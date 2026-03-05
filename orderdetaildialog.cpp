#include "orderdetaildialog.h"
#include <QHeaderView>

OrderDetailDialog::OrderDetailDialog(const Order &order, QWidget *parent)
    : QDialog(parent)
    , m_order(order)
{
    // 设置窗口标题、大小、模态属性
    setWindowTitle("订单详情");
    setFixedSize(600, 400);
    setModal(true);

    setupUI(); // 设置UI
    populateTable(); // 填充表格
}

void OrderDetailDialog::setupUI() // 设置UI
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this); // 创建主布局
    mainLayout->setContentsMargins(15, 15, 15, 15); // 设置主布局的边距
    mainLayout->setSpacing(10); // 设置主布局的间距

    QString titleText = QString("订单号：%1    用户：%2") // 创建标题文本
                       .arg(m_order.getOrderNo())
                       .arg(m_order.getUsername()); // 获取订单号和用户名
    if (!m_order.getTableNumber().isEmpty()) { // 如果桌号不为空，则添加桌号
        titleText += QString("    桌号：%1").arg(m_order.getTableNumber());
    }
    if (m_order.getCustomerCount() > 0) { // 如果客户人数大于0，则添加客户人数
        titleText += QString("    人数：%1").arg(m_order.getCustomerCount());
    }
    QLabel *titleLabel = new QLabel(titleText, this); // 创建标题标签
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << "菜品名称" << "单价" << "数量" << "小计");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet("QTableWidget { border: 1px solid #ddd; border-radius: 4px; gridline-color: #e0e0e0; }"
                           "QTableWidget::item { padding: 6px; }"
                           "QHeaderView::section { background-color: #f5f5f5; padding: 6px; border: none; }");
    mainLayout->addWidget(m_table);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    m_totalLabel = new QLabel(this);
    QFont totalFont = m_totalLabel->font();
    totalFont.setPointSize(14);
    totalFont.setBold(true);
    m_totalLabel->setFont(totalFont);
    m_totalLabel->setStyleSheet("color: #f44336;");
    bottomLayout->addWidget(m_totalLabel);
    bottomLayout->addStretch();

    m_backBtn = new QPushButton("返回修改", this);
    m_backBtn->setFixedSize(100, 36);
    m_backBtn->setStyleSheet("QPushButton { background-color: #9e9e9e; color: white; border-radius: 5px; }"
                             "QPushButton:hover { background-color: #757575; }");
    connect(m_backBtn, &QPushButton::clicked, this, &OrderDetailDialog::reject);

    m_checkoutBtn = new QPushButton("结账", this);
    m_checkoutBtn->setFixedSize(100, 36);
    m_checkoutBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; }"
                                 "QPushButton:hover { background-color: #45a049; }");
    connect(m_checkoutBtn, &QPushButton::clicked, this, &OrderDetailDialog::accept);

    bottomLayout->addWidget(m_backBtn);
    bottomLayout->addWidget(m_checkoutBtn);

    mainLayout->addLayout(bottomLayout);

    setStyleSheet("QDialog { background-color: #fafafa; }"
                  "QLabel { color: #333; }");
}

void OrderDetailDialog::populateTable()
{
    // 填充表格
    const QVector<OrderItem> items = m_order.getItems();
    m_table->setRowCount(items.size());

    // 遍历订单项
    for (int i = 0; i < items.size(); ++i) {
        const OrderItem &item = items[i];
        const Dish &dish = item.getDish();

        m_table->setItem(i, 0, new QTableWidgetItem(dish.getName()));
        m_table->setItem(i, 1, new QTableWidgetItem(QString("¥%1").arg(dish.getPrice(), 0, 'f', 2)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(item.getQuantity())));
        m_table->setItem(i, 3, new QTableWidgetItem(QString("¥%1").arg(item.getTotalPrice(), 0, 'f', 2)));

        for (int j = 0; j < 4; ++j) {
            QTableWidgetItem *tableItem = m_table->item(i, j);
            if (!tableItem) continue;
            if (j == 1 || j == 3) {
                tableItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            } else {
                tableItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
    }

    double total = m_order.getTotalAmount(); // 获取总金额
    m_totalLabel->setText(QString("总计：¥%1").arg(total, 0, 'f', 2));

    m_table->resizeColumnsToContents(); // 调整列宽
    m_table->setColumnWidth(0, 200); // 设置第一列的宽度
}

