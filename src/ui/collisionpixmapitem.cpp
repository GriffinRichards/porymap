#include "collisionpixmapitem.h"
#include "editcommands.h"
#include "metatile.h"

void CollisionPixmapItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (pos != this->previousPos) {
        this->previousPos = pos;
        emit this->hoverChanged(pos);
    }
}

void CollisionPixmapItem::hoverEnterEvent(QGraphicsSceneHoverEvent * event) {
    this->has_mouse = true;
    this->previousPos = Metatile::coordFromPixmapCoord(event->pos());
    emit this->hoverEntered(this->previousPos);
}

void CollisionPixmapItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    this->has_mouse = false;
    emit this->hoverCleared();
}

void CollisionPixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    this->paint_tile_initial_x = this->straight_path_initial_x = pos.x();
    this->paint_tile_initial_y = this->straight_path_initial_y = pos.y();
    emit mouseEvent(event, this);
}

void CollisionPixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (pos != this->previousPos) {
        this->previousPos = pos;
        emit this->hoverChanged(pos);
    }
    emit mouseEvent(event, this);
}

void CollisionPixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    this->lockedAxis = CollisionPixmapItem::Axis::None;
    emit mouseEvent(event, this);
}

void CollisionPixmapItem::draw(bool ignoreCache) {
    if (this->layout) {
        this->layout->setCollisionItem(this);
        setPixmap(this->layout->renderCollision(ignoreCache));
        setOpacity(*this->opacity);
    }
}

void CollisionPixmapItem::paint(QGraphicsSceneMouseEvent *event) {
    if (event->type() == QEvent::GraphicsSceneMouseRelease) {
        actionId_++;
    } else if (this->layout) {
        Blockdata oldCollision = this->layout->blockdata;

        QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

        // Set straight paths on/off and snap to the dominant axis when on
        if (event->modifiers() & Qt::ControlModifier) {
            this->lockNondominantAxis(event);
            pos = this->adjustCoords(pos);
        } else {
            this->prevStraightPathState = false;
            this->lockedAxis = LayoutPixmapItem::Axis::None;
        }

        Block block;
        if (this->layout->getBlock(pos.x(), pos.y(), &block)) {
            block.setCollision(this->selectedCollision->value());
            block.setElevation(this->selectedElevation->value());
            this->layout->setBlock(pos.x(), pos.y(), block, true);
        }

        if (this->layout->blockdata != oldCollision) {
            this->layout->editHistory.push(new PaintCollision(this->layout, oldCollision, this->layout->blockdata, actionId_));
        }
    }
}

void CollisionPixmapItem::floodFill(QGraphicsSceneMouseEvent *event) {
    if (event->type() == QEvent::GraphicsSceneMouseRelease) {
        this->actionId_++;
    } else if (this->layout) {
        Blockdata oldCollision = this->layout->blockdata;

        QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
        uint16_t collision = this->selectedCollision->value();
        uint16_t elevation = this->selectedElevation->value();
        this->layout->floodFillCollisionElevation(pos.x(), pos.y(), collision, elevation);

        if (this->layout->blockdata != oldCollision) {
            this->layout->editHistory.push(new BucketFillCollision(this->layout, oldCollision, this->layout->blockdata));
        }
    }
}

void CollisionPixmapItem::magicFill(QGraphicsSceneMouseEvent *event) {
    if (event->type() == QEvent::GraphicsSceneMouseRelease) {
        this->actionId_++;
    } else if (this->layout) {
        Blockdata oldCollision = this->layout->blockdata;
        QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
        uint16_t collision = this->selectedCollision->value();
        uint16_t elevation = this->selectedElevation->value();
        this->layout->magicFillCollisionElevation(pos.x(), pos.y(), collision, elevation);

        if (this->layout->blockdata != oldCollision) {
            this->layout->editHistory.push(new MagicFillCollision(this->layout, oldCollision, this->layout->blockdata));
        }
    }
}

void CollisionPixmapItem::pick(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    this->updateSelection(pos);
}

void CollisionPixmapItem::updateMovementPermissionSelection(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

    // Snap point to within map bounds.
    if (pos.x() < 0) pos.setX(0);
    if (pos.x() >= this->layout->getWidth()) pos.setX(this->layout->getWidth() - 1);
    if (pos.y() < 0) pos.setY(0);
    if (pos.y() >= this->layout->getHeight()) pos.setY(this->layout->getHeight() - 1);
    this->updateSelection(pos);
}

void CollisionPixmapItem::updateSelection(QPoint pos) {
    Block block;
    if (this->layout->getBlock(pos.x(), pos.y(), &block)) {
        this->selectedCollision->setValue(block.collision());
        this->selectedElevation->setValue(block.elevation());
    }
}
