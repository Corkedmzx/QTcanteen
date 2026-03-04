#ifndef ORDERITEM_H
#define ORDERITEM_H

#include "dish.h"

class OrderItem
{
public:
    OrderItem();
    OrderItem(const Dish &dish, int quantity);

    Dish getDish() const { return m_dish; }
    void setDish(const Dish &dish) { m_dish = dish; }

    int getQuantity() const { return m_quantity; }
    void setQuantity(int quantity) { m_quantity = quantity; }

    double getTotalPrice() const { return m_dish.getPrice() * m_quantity; }

private:
    Dish m_dish;
    int m_quantity;
};

#endif // ORDERITEM_H
