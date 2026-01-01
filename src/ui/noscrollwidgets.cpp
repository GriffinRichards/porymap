#include "noscrollwidgets.h"

#include <QWheelEvent>

bool NoScrollFilter::eventFilter(QObject *object, QEvent *event) {
    if (event->type() != QEvent::Wheel) return false;

    if (m_allowIfFocused) {
        auto widget = qobject_cast<QWidget*>(object);
        if (widget && widget->hasFocus()) return false;
    }
    event->ignore();
    return true;
}

NoScrollFilter* NoScrollFilter::apply(QObject *target, bool allowIfFocused) {
    if (!target) return nullptr;

    auto widget = qobject_cast<QWidget*>(target);
    if (widget) widget->setFocusPolicy(Qt::StrongFocus);

    auto filter = new NoScrollFilter(target, allowIfFocused);
    target->installEventFilter(filter);
    return filter;
}

// On macOS QComboBox::setEditable and QComboBox::setLineEdit will override our changes to the focus policy, so we enforce it here.
void NoScrollComboBox::setEditable(bool editable) {
    auto policy = focusPolicy();
    ComboBox::setEditable(editable);
    setFocusPolicy(policy);
}
void NoScrollComboBox::setLineEdit(QLineEdit *edit) {
    auto policy = focusPolicy();
    ComboBox::setLineEdit(edit);
    setFocusPolicy(policy);
}
