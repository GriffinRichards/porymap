#pragma once
#ifndef EVENTFACTORY_H
#define EVENTFACTORY_H

#include "events.h"
#include "eventframes.h"

namespace EventFactory {
    Event* create(Event::Type type);
    EventFrame* createFrame(Event *event, QWidget *parent = nullptr);
};

#endif // EVENTFRAMEFACTORY_H