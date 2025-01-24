#include "draggablepixmapitem.h"
#include "editor.h"
#include "editcommands.h"
#include "mapruler.h"
#include "metatile.h"

static unsigned currentActionId = 0;


void DraggablePixmapItem::updatePosition() {
    int x = event->getPixelX();
    int y = event->getPixelY();
    setX(x);
    setY(y);
    if (editor->selected_events && editor->selected_events->contains(this)) {
        setZValue(event->getY() + 1);
    } else {
        setZValue(event->getY());
    }
    editor->updateWarpEventWarning(event);
}

void DraggablePixmapItem::emitPositionChanged() {
    emit xChanged(event->getX());
    emit yChanged(event->getY());
    emit elevationChanged(event->getElevation());
}

void DraggablePixmapItem::updatePixmap() {
    editor->project->setEventPixmap(event, true);
    this->updatePosition();
    editor->redrawObject(this);
    emit spriteChanged(event->getPixmap());
}

void DraggablePixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *mouse) {
    if (this->active)
        return;
    this->active = true;
    this->lastPos = Metatile::coordFromPixmapCoord(mouse->scenePos());

    bool selectionToggle = mouse->modifiers() & Qt::ControlModifier;
    if (selectionToggle || !editor->selected_events->contains(this)) {
        // User is either toggling this selection on/off as part of a group selection,
        // or they're newly selecting just this item.
        this->editor->selectMapEvent(this, selectionToggle);
    } else {
        // This item is already selected and the user isn't toggling the selection, so there are 4 possibilities:
        // 1. This is the only selected event, and the selection is pointless.
        // 2. This is the only selected event, and they want to drag the item around.
        // 3. There's a group selection, and they want to start a new selection with just this item.
        // 4. There's a group selection, and they want to drag the group around.
        // 'selectMapEvent' will immediately clear the rest of the selection, which supports #1-3 but prevents #4.
        // To support #4 we set the flag below, and we only call 'selectMapEvent' on mouse release if no move occurred.
        this->releaseSelectionQueued = true;
    }
    this->editor->selectingEvent = true;
}

void DraggablePixmapItem::move(int dx, int dy) {
    event->setX(event->getX() + dx);
    event->setY(event->getY() + dy);
    updatePosition();
    emitPositionChanged();
}

void DraggablePixmapItem::moveTo(const QPoint &pos) {
    event->setX(pos.x());
    event->setY(pos.y());
    updatePosition();
    emitPositionChanged();
}

void DraggablePixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *mouse) {
    if (!this->active)
        return;

    QPoint pos = Metatile::coordFromPixmapCoord(mouse->scenePos());
    if (pos == this->lastPos)
        return;

    QPoint moveDistance = pos - this->lastPos;
    this->lastPos = pos;
    emit this->editor->map_item->hoveredMapMetatileChanged(pos);

    QList <Event *> selectedEvents;
    if (editor->selected_events->contains(this)) {
        for (DraggablePixmapItem *item : *editor->selected_events) {
            selectedEvents.append(item->event);
        }
    } else {
        selectedEvents.append(this->event);
    }
    editor->map->commit(new EventMove(selectedEvents, moveDistance.x(), moveDistance.y(), currentActionId));
    this->releaseSelectionQueued = false;
}

void DraggablePixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouse) {
    if (!this->active)
        return;
    this->active = false;
    currentActionId++;
    if (this->releaseSelectionQueued) {
        this->releaseSelectionQueued = false;
        if (Metatile::coordFromPixmapCoord(mouse->scenePos()) == this->lastPos)
            this->editor->selectMapEvent(this);
    }
}

// Events with properties that specify a map will open that map when double-clicked.
void DraggablePixmapItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *) {
    Event::Type eventType = this->event->getEventType();
    if (eventType == Event::Type::Warp) {
        WarpEvent *warp = dynamic_cast<WarpEvent *>(this->event);
        QString destMap = warp->getDestinationMap();
        int warpId = ParseUtil::gameStringToInt(warp->getDestinationWarpID());
        emit editor->warpEventDoubleClicked(destMap, warpId, Event::Group::Warp);
    }
    else if (eventType == Event::Type::CloneObject) {
        CloneObjectEvent *clone = dynamic_cast<CloneObjectEvent *>(this->event);
        emit editor->warpEventDoubleClicked(clone->getTargetMap(), clone->getTargetID(), Event::Group::Object);
    }
    else if (eventType == Event::Type::SecretBase) {
        const QString mapPrefix = projectConfig.getIdentifier(ProjectIdentifier::define_map_prefix);
        SecretBaseEvent *base = dynamic_cast<SecretBaseEvent *>(this->event);
        QString baseId = base->getBaseID();
        QString destMap = editor->project->mapConstantsToMapNames.value(mapPrefix + baseId.left(baseId.lastIndexOf("_")));
        emit editor->warpEventDoubleClicked(destMap, 0, Event::Group::Warp);
    }
    else if (eventType == Event::Type::HealLocation && projectConfig.healLocationRespawnDataEnabled) {
        HealLocationEvent *heal = dynamic_cast<HealLocationEvent *>(this->event);
        emit editor->warpEventDoubleClicked(heal->getRespawnMapName(), heal->getRespawnNPC(), Event::Group::Object);
    }
}
