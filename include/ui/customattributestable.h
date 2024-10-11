#ifndef CUSTOMATTRIBUTESTABLE_H
#define CUSTOMATTRIBUTESTABLE_H

#include <QObject>
#include <QJsonValue>
#include <QTableWidget>

class CustomAttributesTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit CustomAttributesTable(QWidget *parent = nullptr);
    ~CustomAttributesTable() {};

    QMap<QString, QJsonValue> getAttributes() const;
    void setAttributes(const QMap<QString, QJsonValue> &attributes);

    void addNewAttribute(const QString &key, const QJsonValue &value, bool setAsDefault = false);
    bool deleteSelectedAttributes();

    bool isEmpty() const;
    bool isSelectionEmpty() const;

    QSet<QString> keys() const;
    QSet<QString> defaultKeys() const;
    QSet<QString> restrictedKeys() const;
    void setDefaultKeys(const QSet<QString> &keys);
    void setRestrictedKeys(const QSet<QString> &keys);

signals:
    void edited();
    void defaultSet(QString key, QJsonValue value);
    void defaultRemoved(QString key);

protected:
    virtual void resizeEvent(QResizeEvent *event) override;

private:
    QSet<QString> m_keys;           // All keys currently in the table
    QSet<QString> m_defaultKeys;    // All keys that are in this table by default (whether or not they're present)
    QSet<QString> m_restrictedKeys; // All keys not allowed in the table

    QPair<QString, QJsonValue> getAttribute(int row) const;
    int addAttribute(const QString &key, const QJsonValue &value);
    void removeAttribute(const QString &key);
    void setDefaultAttribute(const QString &key, const QJsonValue &value);
    void unsetDefaultAttribute(const QString &key);
    void resizeVertically();
};

#endif // CUSTOMATTRIBUTESTABLE_H
