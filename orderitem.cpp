#include "orderitem.h"

OrderItem::OrderItem()
    : m_quantity(0)
{
}

OrderItem::OrderItem(const Dish &dish, int quantity)
    : m_dish(dish), m_quantity(quantity)
{
}
