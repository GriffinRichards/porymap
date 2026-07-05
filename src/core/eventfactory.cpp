#include "eventfactory.h"

Event* EventFactory::create(Event::Type type) {
    switch (type) {
    case Event::Type::None: return nullptr;
    case Event::Type::Object: return new ObjectEvent();
    case Event::Type::CloneObject: return new CloneObjectEvent();
    case Event::Type::Warp: return new WarpEvent();
    case Event::Type::Trigger: return new TriggerEvent();
    case Event::Type::WeatherTrigger: return new WeatherTriggerEvent();
    case Event::Type::Sign: return new SignEvent();
    case Event::Type::HiddenItem: return new HiddenItemEvent();
    case Event::Type::SecretBase: return new SecretBaseEvent();
    case Event::Type::HealLocation: return new HealLocationEvent();
    }
    return nullptr;
}

EventFrame* EventFactory::createFrame(Event *event, QWidget *parent) {
    if (!event) return nullptr;
    switch (event->getEventType()) {
    case Event::Type::None: return nullptr;
    case Event::Type::Object: return new ObjectFrame(qobject_cast<ObjectEvent*>(event), parent);
    case Event::Type::CloneObject: return new CloneObjectFrame(qobject_cast<CloneObjectEvent*>(event), parent);
    case Event::Type::Warp: return new WarpFrame(qobject_cast<WarpEvent*>(event), parent);
    case Event::Type::Trigger: return new TriggerFrame(qobject_cast<TriggerEvent*>(event), parent);
    case Event::Type::WeatherTrigger: return new WeatherTriggerFrame(qobject_cast<WeatherTriggerEvent*>(event), parent);
    case Event::Type::Sign: return new SignFrame(qobject_cast<SignEvent*>(event), parent);
    case Event::Type::HiddenItem: return new HiddenItemFrame(qobject_cast<HiddenItemEvent*>(event), parent);
    case Event::Type::SecretBase: return new SecretBaseFrame(qobject_cast<SecretBaseEvent*>(event), parent);
    case Event::Type::HealLocation: return new HealLocationFrame(qobject_cast<HealLocationEvent*>(event), parent);
    }
    return nullptr;
}
