#include "neweventtoolbutton.h"
#include <QMenu>

// This event type should never be hidden. This is our initial and fallback selection.
const Event::Type defaultEventType = Event::Type::Object;

// Custom QToolButton which has a context menu that expands to allow
// selection of different types of map events.
NewEventToolButton::NewEventToolButton(QWidget *parent) :
    QToolButton(parent)
{
    setPopupMode(QToolButton::MenuButtonPopup);
    QObject::connect(this, &QToolButton::triggered, this, &QToolButton::setDefaultAction);

    this->menu = new QMenu(this);
    insertAction(Event::Type::Object);
    insertAction(Event::Type::CloneObject);
    insertAction(Event::Type::Warp);
    insertAction(Event::Type::Trigger);
    insertAction(Event::Type::WeatherTrigger);
    insertAction(Event::Type::Sign);
    insertAction(Event::Type::HiddenItem);
    insertAction(Event::Type::SecretBase);
    insertAction(Event::Type::HealLocation);

    setMenu(this->menu);
    setDefaultAction(defaultEventType);
}

void NewEventToolButton::insertAction(Event::Type type) {
    if (this->newEventActions.contains(type))
        return;

    auto action = new QAction(QString("New %1").arg(Event::typeToString(type)), this);
    action->setIcon(QIcon(":/icons/add.ico"));
    connect(action, &QAction::triggered, [this, type] {
        this->selectedEventType = type;
        emit newEventAdded(this->selectedEventType);
    });

    this->newEventActions.insert(type, action);
    this->menu->addAction(action);
}

void NewEventToolButton::setDefaultAction(Event::Type type) {
    if (this->newEventActions.contains(type) && this->newEventActions.value(type)->isVisible())
        QToolButton::setDefaultAction(this->newEventActions.value(type));
}

void NewEventToolButton::setActionVisible(Event::Type type, bool visible) {
    if (type != defaultEventType && this->newEventActions.contains(type)){
        this->newEventActions.value(type)->setVisible(visible);
        if (this->selectedEventType == type){ 
            setDefaultAction(defaultEventType);
        }
    }
}
