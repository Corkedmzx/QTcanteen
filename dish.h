#ifndef DISH_H
#define DISH_H

#include <QString>

class Dish
{
public:
    Dish();
    Dish(const QString &name, const QString &category, double price, const QString &description = "", const QString &imagePath = "");

    QString getName() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    QString getCategory() const { return m_category; }
    void setCategory(const QString &category) { m_category = category; }

    double getPrice() const { return m_price; }
    void setPrice(double price) { m_price = price; }

    QString getDescription() const { return m_description; }
    void setDescription(const QString &description) { m_description = description; }

    QString getImagePath() const { return m_imagePath; }
    void setImagePath(const QString &imagePath) { m_imagePath = imagePath; }

    int getId() const { return m_id; }
    void setId(int id) { m_id = id; }

private:
    int m_id;
    QString m_name;
    QString m_category;
    double m_price;
    QString m_description;
    QString m_imagePath;  // 预留图片路径接口
};

#endif // DISH_H
