#ifndef EVENTCOMBOBOX_H
#define EVENTCOMBOBOX_H

#include "noscrollwidgets.h"

#include <QRegularExpression>

class EventComboBox : public NoScrollComboBox
{
    Q_OBJECT
public:
    explicit EventComboBox(QWidget *parent = nullptr) : NoScrollComboBox(parent) {
        // Make speed a priority when loading comboboxes.
        setMinimumContentsLength(24);// an arbitrary limit
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

        // In general, event combo boxes are always editable.
        setEditable(true);
    }
};

#endif // EVENTCOMBOBOX_H
