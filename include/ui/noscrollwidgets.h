#ifndef NOSCROLLWIDGETS_H
#define NOSCROLLWIDGETS_H

#include "combobox.h"

#include <QSpinBox>
#include <QTextEdit>

/// Prevent widgets from stealing focus when a user scrolls past it.
/// Any QObject with this filter will never accept wheel events.
/// Any QWidget with this filter will only accept wheel events
/// if 'allowIfFocused' is 'true' and the widget has focus.
class NoScrollFilter : public QObject {
    Q_OBJECT
public:
    NoScrollFilter(QObject *parent, bool allowIfFocused)
        : QObject(parent), m_allowIfFocused(allowIfFocused) {}

    static NoScrollFilter* apply(QObject *target, bool allowIfFocused = true);
    bool eventFilter(QObject *obj, QEvent *event) override;

    void setAllowIfFocused(bool enabled) { m_allowIfFocused = enabled; }

private:
    bool m_allowIfFocused;
};



class NoScrollComboBox : public ComboBox {
    Q_OBJECT
public:
    explicit NoScrollComboBox(QWidget *parent = nullptr) : ComboBox(parent) {
        m_filter = NoScrollFilter::apply(this);
    }

    void setEditable(bool editable) override;
    void setLineEdit(QLineEdit *edit) override;

    void setAllowScrollingIfFocused(bool enabled) { if (m_filter) m_filter->setAllowIfFocused(enabled); }

private:
    NoScrollFilter* m_filter = nullptr;
};

class NoScrollAbstractSpinBox : public QAbstractSpinBox {
    Q_OBJECT
public:
    explicit NoScrollAbstractSpinBox(QWidget *parent = nullptr) : QAbstractSpinBox(parent) {
        NoScrollFilter::apply(this);
    }
};

class NoScrollDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT
public:
    explicit NoScrollDoubleSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent) {
        NoScrollFilter::apply(this);
    }
};

class NoScrollSpinBox : public QSpinBox {
    Q_OBJECT
public:
    explicit NoScrollSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) {
        NoScrollFilter::apply(this);
    }
};

class NoScrollTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit NoScrollTextEdit(const QString &text, QWidget *parent = nullptr) : QTextEdit(text, parent) {
        NoScrollFilter::apply(this);
    }
    explicit NoScrollTextEdit(QWidget *parent = nullptr) : NoScrollTextEdit(QString(), parent) {}
};

#endif // NOSCROLLWIDGETS_H
