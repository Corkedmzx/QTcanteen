#include "dish.h"

Dish::Dish()
    : m_id(0), m_name(""), m_category(""), m_price(0.0), m_description(""), m_imagePath("")
{
}

Dish::Dish(const QString &name, const QString &category, double price, const QString &description, const QString &imagePath)
    : m_id(0), m_name(name), m_category(category), m_price(price), m_description(description), m_imagePath(imagePath)
{
}
