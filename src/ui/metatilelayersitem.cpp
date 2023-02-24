#include "config.h"
#include "metatilelayersitem.h"
#include "imageproviders.h"
#include <QPainter>

// TODO: Handle unused layer

void MetatileLayersItem::draw() {
    // TODO: Handle vertical layout
    QPixmap pixmap(this->layers.length() * 32, 32);

    QPainter painter(&pixmap);
    for (int i = 0; i < this->layers.length(); i++) {
        QImage layerImage = getMetatileLayerImage(this->metatile,
                                                  this->layers.at(i),
                                                  this->primaryTileset,
                                                  this->secondaryTileset,
                                                  1.0, // opacity
                                                  false, // allowTransparency
                                                  true // useTruePalettes
                                                  )
                            .scaled(32, 32);
        painter.drawImage(QPoint(i * 32, 0), layerImage);
    }

    this->setPixmap(pixmap);
}

void MetatileLayersItem::setMetatile(Metatile *metatile) {
    this->metatile = metatile;
    this->clearLastModifiedCoords();
}

void MetatileLayersItem::setTilesets(Tileset *primaryTileset, Tileset *secondaryTileset) {
    this->primaryTileset = primaryTileset;
    this->secondaryTileset = secondaryTileset;
    this->draw();
    this->clearLastModifiedCoords();
}

void MetatileLayersItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->buttons() & Qt::RightButton) {
        SelectablePixmapItem::mousePressEvent(event);
        QPoint selectionOrigin = this->getSelectionStart();
        QPoint dimensions = this->getSelectionDimensions();
        emit this->selectedTilesChanged(selectionOrigin, dimensions.x(), dimensions.y());
        this->drawSelection();
    } else {
        int x, y;
        this->getBoundedCoords(event->pos(), &x, &y);
        this->prevChangedTile.setX(x);
        this->prevChangedTile.setY(y);
        emit this->tileChanged(x, y);
    }
}

void MetatileLayersItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (event->buttons() & Qt::RightButton) {
        SelectablePixmapItem::mouseMoveEvent(event);
        QPoint selectionOrigin = this->getSelectionStart();
        QPoint dimensions = this->getSelectionDimensions();
        emit this->selectedTilesChanged(selectionOrigin, dimensions.x(), dimensions.y());
        this->drawSelection();
    } else {
        int x, y;
        this->getBoundedCoords(event->pos(), &x, &y);
        if (prevChangedTile.x() != x || prevChangedTile.y() != y) {
            this->prevChangedTile.setX(x);
            this->prevChangedTile.setY(y);
            emit this->tileChanged(x, y);
        }
    }
}

void MetatileLayersItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->buttons() & Qt::RightButton) {
        SelectablePixmapItem::mouseReleaseEvent(event);
        QPoint selectionOrigin = this->getSelectionStart();
        QPoint dimensions = this->getSelectionDimensions();
        emit this->selectedTilesChanged(selectionOrigin, dimensions.x(), dimensions.y());
    }

    this->draw();
}

void MetatileLayersItem::clearLastModifiedCoords() {
    this->prevChangedTile.setX(-1);
    this->prevChangedTile.setY(-1);
}

void MetatileLayersItem::getBoundedCoords(QPointF pos, int *x, int *y) {
    int maxX = (this->layers.length() * 2) - 1;
    *x = static_cast<int>(pos.x()) / 16;
    *y = static_cast<int>(pos.y()) / 16;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x > maxX) *x = maxX;
    if (*y > 1) *y = 1;
}
