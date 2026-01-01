#ifndef EDITHISTORYSPINBOX_H
#define EDITHISTORYSPINBOX_H

#include "noscrollwidgets.h"

class EditHistorySpinBox : public NoScrollSpinBox
{
    Q_OBJECT

public:
    explicit EditHistorySpinBox(QWidget *parent = nullptr) : NoScrollSpinBox(parent) {}

    void focusOutEvent(QFocusEvent *event) override {
        m_actionId++;
        NoScrollSpinBox::focusOutEvent(event);
    }

    unsigned getActionId() const { return m_actionId; }

private:
    unsigned m_actionId{0};
};

#endif // EDITHISTORYSPINBOX_H
