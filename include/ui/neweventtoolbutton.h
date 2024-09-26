#ifndef NEWEVENTTOOLBUTTON_H
#define NEWEVENTTOOLBUTTON_H

#include "events.h"
#include <QToolButton>

class NewEventToolButton : public QToolButton
{
    Q_OBJECT
public:
    explicit NewEventToolButton(QWidget *parent = nullptr);

    Event::Type getSelectedEventType() const { return this->selectedEventType; }
    QAction* getAction(Event::Type type) const { return this->newEventActions.value(type); }

    void setActionVisible(Event::Type type, bool visible);
    void setDefaultAction(Event::Type type);

signals:
    void newEventAdded(Event::Type type);

private:
    Event::Type selectedEventType;
    QMenu* menu;
    QMap<Event::Type, QAction*> newEventActions;

    void insertAction(Event::Type type);
};

#endif // NEWEVENTTOOLBUTTON_H
