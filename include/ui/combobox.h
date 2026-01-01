#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>

// Contains our general-purpose additions to QComboBox

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr);

    void setTextItem(const QString &text);
    void setNumberItem(int value);
    void setHexItem(uint32_t value);

    virtual void setEditable(bool editable);
    virtual void setLineEdit(QLineEdit *edit);

    void setClearButtonEnabled(bool enabled);

signals:
    void editingFinished();

private:
    void setItem(int index, const QString &text);

    void initLineEdit(QLineEdit *edit);
};

#endif // COMBOBOX_H
