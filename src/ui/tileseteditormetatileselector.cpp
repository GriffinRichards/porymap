#include "tileseteditormetatileselector.h"
#include "imageproviders.h"
#include "project.h"
#include <QPainter>

static const int mWidth = 32;
static const int mHeight = 32;

TilesetEditorMetatileSelector::TilesetEditorMetatileSelector(Tileset *primaryTileset, Tileset *secondaryTileset, Map *map)
  : SelectablePixmapItem(mWidth, mHeight, 1, 1) {
    this->setTilesets(primaryTileset, secondaryTileset, false);
    this->numMetatilesWide = 8;
    this->map = map;
    this->layerView = MetatileLayerView::Combined;
    setAcceptHoverEvents(true);
    this->usedMetatiles.resize(Project::getNumMetatilesTotal());
}

QImage TilesetEditorMetatileSelector::buildAllMetatilesImage() {
    // TODO: Add toggle to hide primary/secodary
    QImage primaryImage = this->buildPrimaryMetatilesImage();
    QImage secondaryImage = this->buildSecondaryMetatilesImage();
    QImage image(this->numMetatilesWide * mWidth, primaryImage.height() + secondaryImage.height(), QImage::Format_RGBA8888);
    image.fill(Qt::magenta);
    QPainter painter(&image);
    painter.drawImage(QPoint(0, 0), primaryImage);
    painter.drawImage(QPoint(0, primaryImage.height()), secondaryImage);
    return image;
}

QImage TilesetEditorMetatileSelector::buildPrimaryMetatilesImage() {
    return this->buildImage(this->primaryTileset);
}

QImage TilesetEditorMetatileSelector::buildSecondaryMetatilesImage() {
    return this->buildImage(this->secondaryTileset);
}

QImage TilesetEditorMetatileSelector::buildImage(Tileset * tileset) {
    if (!tileset)
        return QImage();

    int numMetatilesHigh = tileset->metatiles.length() / this->numMetatilesWide;
    if (tileset->metatiles.length() % this->numMetatilesWide != 0) {
        // Round up height for incomplete last row
        numMetatilesHigh++;
    }

    QImage image(this->numMetatilesWide * mWidth, numMetatilesHigh * mHeight, QImage::Format_RGBA8888);
    image.fill(Qt::magenta);
    QPainter painter(&image);
    for (int i = 0; i < tileset->metatiles.length(); i++) {
        QImage metatile_image;

        // TODO: Do this more gracefully
        if (this->layerView == MetatileLayerView::Combined) {
            metatile_image = getMetatileImage(
                        tileset->metatiles.at(i),
                        this->primaryTileset,
                        this->secondaryTileset,
                        map->metatileLayerOrder,
                        map->metatileLayerOpacity,
                        true)
                    .scaled(mWidth, mHeight);
        } else {
            // TODO: Test triple layer metatiles
            // TODO: Handle layer ghosts
            int layer = getLayerToDraw(tileset->metatiles.at(i));
            qreal opacity = map->metatileLayerOpacity.value(layer, 1.0);
            metatile_image = getMetatileLayerImage(
                        tileset->metatiles.at(i),
                        layer,
                        this->primaryTileset,
                        this->secondaryTileset,
                        opacity,
                        false,
                        true
                    ).scaled(mWidth, mHeight);
        }

        int map_y = i / this->numMetatilesWide;
        int map_x = i % this->numMetatilesWide;
        QPoint metatile_origin = QPoint(map_x * mWidth, map_y * mHeight);
        painter.drawImage(metatile_origin, metatile_image);
    }
    if (this->showGrid) {
        for (int column = 0; column < this->numMetatilesWide; column++) {
            int x = column * mWidth;
            int y = numMetatilesHigh * mHeight;
            painter.drawLine(x, 0, x, y);
        }
        for (int row = 0; row < numMetatilesHigh; row++) {
            int x = this->numMetatilesWide * mWidth;
            int y = row * mHeight;
            painter.drawLine(0, y, x, y);
        }
    }
    painter.end();
    return image;
}

// When triple layer metatiles is enabled, the layer view name (Top/Middle/Bottom) corresponds to the actual rendered layer.
// When it's not enabled, "Top"/"Bottom" refer to the upper/lowermost layers, i.e. "Top" may be Top or Middle and "Bottom" may be Middle or Bottom
// TODO: Refactor this garbage away
int TilesetEditorMetatileSelector::getLayerToDraw(Metatile * metatile) {
    const QHash<int, QHash<MetatileLayerView, int>> viewToLayer_Normal = {
        {METATILE_LAYER_MIDDLE_TOP, {
            {MetatileLayerView::Bottom, 1}, // Middle
            {MetatileLayerView::Top, 2},    // Top
        }},
        {METATILE_LAYER_BOTTOM_MIDDLE, {
            {MetatileLayerView::Bottom, 0}, // Bottom
            {MetatileLayerView::Top, 1},    // Middle
        }},
        {METATILE_LAYER_BOTTOM_TOP, {
            {MetatileLayerView::Bottom, 0}, // Bottom
            {MetatileLayerView::Top, 2},    // Top
        }},
    };
    const QHash<MetatileLayerView, int> viewToLayer_TripleLayer = {
        {MetatileLayerView::Bottom, 0},
        {MetatileLayerView::Middle, 1},
        {MetatileLayerView::Top, 2},
    };

    return projectConfig.getTripleLayerMetatilesEnabled()
         ? viewToLayer_TripleLayer.value(this->layerView, 0)
         : viewToLayer_Normal.value(metatile->layerType).value(this->layerView, 0);
}

void TilesetEditorMetatileSelector::draw() {
    this->setPixmap(QPixmap::fromImage(this->buildAllMetatilesImage()));
    this->drawSelection();

    drawFilters();
}

bool TilesetEditorMetatileSelector::select(uint16_t metatileId) {
    if (!Tileset::metatileIsValid(metatileId, this->primaryTileset, this->secondaryTileset)) return false;
    QPoint coords = this->getMetatileIdCoords(metatileId);
    SelectablePixmapItem::select(coords.x(), coords.y(), 0, 0);
    this->selectedMetatile = metatileId;
    emit selectedMetatileChanged(metatileId);
    return true;
}

void TilesetEditorMetatileSelector::setTilesets(Tileset *primaryTileset, Tileset *secondaryTileset, bool draw) {
    this->primaryTileset = primaryTileset;
    this->secondaryTileset = secondaryTileset;

    if (draw) this->draw();
}

void TilesetEditorMetatileSelector::updateSelectedMetatile() {
    QPoint origin = this->getSelectionStart();
    uint16_t metatileId = this->getMetatileId(origin.x(), origin.y());
    if (Tileset::metatileIsValid(metatileId, this->primaryTileset, this->secondaryTileset))
        this->selectedMetatile = metatileId;
    else
        this->selectedMetatile = Project::getNumMetatilesPrimary() + this->secondaryTileset->metatiles.length() - 1;
    emit selectedMetatileChanged(this->selectedMetatile);
}

uint16_t TilesetEditorMetatileSelector::getSelectedMetatileId() {
    return this->selectedMetatile;
}

uint16_t TilesetEditorMetatileSelector::getMetatileId(int x, int y) {
    int index = y * this->numMetatilesWide + x;
    if (index < this->primaryTileset->metatiles.length()) {
        return static_cast<uint16_t>(index);
    } else {
        return static_cast<uint16_t>(Project::getNumMetatilesPrimary() + index - this->primaryTileset->metatiles.length());
    }
}

bool TilesetEditorMetatileSelector::shouldAcceptEvent(QGraphicsSceneMouseEvent *event) {
    QPoint pos = this->getCellPos(event->pos());
    return Tileset::metatileIsValid(getMetatileId(pos.x(), pos.y()), this->primaryTileset, this->secondaryTileset);
}

void TilesetEditorMetatileSelector::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (!shouldAcceptEvent(event)) return;
    SelectablePixmapItem::mousePressEvent(event);
    this->updateSelectedMetatile();
}

void TilesetEditorMetatileSelector::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (!shouldAcceptEvent(event)) return;
    SelectablePixmapItem::mouseMoveEvent(event);
    this->updateSelectedMetatile();
    emit hoveredMetatileChanged(this->selectedMetatile);
}

void TilesetEditorMetatileSelector::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (!shouldAcceptEvent(event)) return;
    SelectablePixmapItem::mouseReleaseEvent(event);
    this->updateSelectedMetatile();
}

void TilesetEditorMetatileSelector::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPoint pos = this->getCellPos(event->pos());
    uint16_t metatileId = this->getMetatileId(pos.x(), pos.y());
    emit this->hoveredMetatileChanged(metatileId);
}

void TilesetEditorMetatileSelector::hoverLeaveEvent(QGraphicsSceneHoverEvent*) {
    emit this->hoveredMetatileCleared();
}

QPoint TilesetEditorMetatileSelector::getMetatileIdCoords(uint16_t metatileId) {
    if (!Tileset::metatileIsValid(metatileId, this->primaryTileset, this->secondaryTileset))
    {
        // Invalid metatile id.
        return QPoint(0, 0);
    }
    int index = metatileId < Project::getNumMetatilesPrimary()
                ? metatileId
                : metatileId - Project::getNumMetatilesPrimary() + this->primaryTileset->metatiles.length();
    return QPoint(index % this->numMetatilesWide, index / this->numMetatilesWide);
}

QPoint TilesetEditorMetatileSelector::getMetatileIdCoordsOnWidget(uint16_t metatileId) {
    QPoint pos = getMetatileIdCoords(metatileId);
    pos.rx() = (pos.x() * this->cellWidth) + (this->cellWidth / 2);
    pos.ry() = (pos.y() * this->cellHeight) + (this->cellHeight / 2);
    return pos;
}

void TilesetEditorMetatileSelector::drawFilters() {
    if (selectorShowUnused) {
        drawUnused();
    }
    if (selectorShowCounts) {
        drawCounts();
    }
}

void TilesetEditorMetatileSelector::drawUnused() {
    // setup the circle with a line through it image to layer above unused metatiles
    QPixmap redX(mWidth, mHeight);
    redX.fill(Qt::transparent);

    QPen whitePen(Qt::white);
    whitePen.setWidth(1);
    QPen pinkPen(Qt::magenta);
    pinkPen.setWidth(1);

    QPainter oPainter(&redX);

    oPainter.setPen(whitePen);
    oPainter.drawEllipse(QRect(1, 1, 30, 30));
    oPainter.setPen(pinkPen);
    oPainter.drawEllipse(QRect(2, 2, 28, 28));
    oPainter.drawEllipse(QRect(3, 3, 26, 26));

    oPainter.setPen(whitePen);
    oPainter.drawEllipse(QRect(4, 4, 24, 24));

    whitePen.setWidth(5);
    oPainter.setPen(whitePen);
    oPainter.drawLine(0, 0, 31, 31);

    pinkPen.setWidth(3);
    oPainter.setPen(pinkPen);
    oPainter.drawLine(2, 2, 29, 29);

    oPainter.end();

    // draw symbol on unused metatiles
    QPixmap metatilesPixmap = this->pixmap();

    QPainter unusedPainter(&metatilesPixmap);
    unusedPainter.setOpacity(0.5);

    int primaryLength = this->primaryTileset->metatiles.length();
    int length_ = primaryLength + this->secondaryTileset->metatiles.length();
    int height_ = length_ / this->numMetatilesWide;
    if (length_ % this->numMetatilesWide != 0) {
        height_++;
    }

    for (int i = 0; i < length_; i++) {
        int tile = i;
        if (i >= primaryLength) {
            tile += Project::getNumMetatilesPrimary() - primaryLength;
        }
        if (!usedMetatiles[tile]) {
            unusedPainter.drawPixmap((i % 8) * mWidth, (i / 8) * mHeight, redX);
        }
    }

    unusedPainter.end();

    this->setPixmap(metatilesPixmap);
}

void TilesetEditorMetatileSelector::drawCounts() {
    QPen blackPen(Qt::black);
    blackPen.setWidth(1);

    QPixmap metatilesPixmap = this->pixmap();

    QPainter countPainter(&metatilesPixmap);
    countPainter.setPen(blackPen);

    for (int tile = 0; tile < this->usedMetatiles.size(); tile++) {
        int count = usedMetatiles[tile];
        QString countText = QString::number(count);
        if (count > 1000) countText = ">1k";
        countPainter.drawText((tile % 8) * 32, (tile / 8) * 32 + 32, countText);
    }

    // write in white and black for contrast
    QPen whitePen(Qt::white);
    whitePen.setWidth(1);
    countPainter.setPen(whitePen);

    int primaryLength = this->primaryTileset->metatiles.length();
    int length_ = primaryLength + this->secondaryTileset->metatiles.length();
    int height_ = length_ / this->numMetatilesWide;
    if (length_ % this->numMetatilesWide != 0) {
        height_++;
    }

    for (int i = 0; i < length_; i++) {
        int tile = i;
        if (i >= primaryLength) {
            tile += Project::getNumMetatilesPrimary() - primaryLength;
        }
        int count = usedMetatiles[tile];
        QString countText = QString::number(count);
        if (count > 1000) countText = ">1k";
        countPainter.drawText((i % 8) * 32 + 1, (i / 8) * 32 + 32 - 1, countText);
    }

    countPainter.end();

    this->setPixmap(metatilesPixmap);
}
