#include "layoutpixmapitem.h"
#include "metatile.h"
#include "log.h"
#include "scripting.h"

#include "editcommands.h"

#define SWAP(a, b) do { if (a != b) { a ^= b; b ^= a; a ^= b; } } while (0)

void LayoutPixmapItem::paint(QGraphicsSceneMouseEvent *event) {
    if (layout) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            actionId_++;
        } else {
            QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

            // Set straight paths on/off and snap to the dominant axis when on
            if (event->modifiers() & Qt::ControlModifier) {
                this->lockNondominantAxis(event);
                pos = this->adjustCoords(pos);
            } else {
                this->prevStraightPathState = false;
                this->lockedAxis = LayoutPixmapItem::Axis::None;
            }

            // Paint onto the map.
            bool shiftPressed = event->modifiers() & Qt::ShiftModifier;
            QPoint selectionDimensions = this->metatileSelector->getSelectionDimensions();
            if (settings->smartPathsEnabled) {
                if (!shiftPressed && selectionDimensions.x() == 3 && selectionDimensions.y() == 3) {
                    paintSmartPath(pos.x(), pos.y());
                } else {
                    paintNormal(pos.x(), pos.y());
                }
            } else {
                if (shiftPressed && selectionDimensions.x() == 3 && selectionDimensions.y() == 3) {
                    paintSmartPath(pos.x(), pos.y());
                } else {
                    paintNormal(pos.x(), pos.y());
                }
            }
        }
    }
}

void LayoutPixmapItem::shift(QGraphicsSceneMouseEvent *event) {
    if (layout) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            actionId_++;
        } else {
            QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

            // Set straight paths on/off and snap to the dominant axis when on
            if (event->modifiers() & Qt::ControlModifier) {
                this->lockNondominantAxis(event);
                pos = this->adjustCoords(pos);
            } else {
                this->prevStraightPathState = false;
                this->lockedAxis = LayoutPixmapItem::Axis::None;
            }

            if (event->type() == QEvent::GraphicsSceneMousePress) {
                selection_origin = QPoint(pos.x(), pos.y());
                selection.clear();
            } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
                if (pos.x() != selection_origin.x() || pos.y() != selection_origin.y()) {
                    int xDelta = pos.x() - selection_origin.x();
                    int yDelta = pos.y() - selection_origin.y();
                    this->shift(xDelta, yDelta);
                    selection_origin = QPoint(pos.x(), pos.y());
                    selection.clear();
                    draw();
                }
            }
        }
    }
}

void LayoutPixmapItem::shift(int xDelta, int yDelta, bool fromScriptCall) {
    Blockdata oldMetatiles = this->layout->blockdata;

    for (int i = 0; i < this->layout->getWidth(); i++)
    for (int j = 0; j < this->layout->getHeight(); j++) {
        int destX = i + xDelta;
        int destY = j + yDelta;
        if (destX < 0)
            do { destX += this->layout->getWidth(); } while (destX < 0);
        if (destY < 0)
            do { destY += this->layout->getHeight(); } while (destY < 0);
        destX %= this->layout->getWidth();
        destY %= this->layout->getHeight();

        int blockIndex = j * this->layout->getWidth() + i;
        Block srcBlock = oldMetatiles.at(blockIndex);
        this->layout->setBlock(destX, destY, srcBlock);
    }

    if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
        this->layout->editHistory.push(new ShiftMetatiles(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
        Scripting::cb_MapShifted(xDelta, yDelta);
    }
}

void LayoutPixmapItem::paintNormal(int x, int y, bool fromScriptCall) {
    MetatileSelection selection = this->metatileSelector->getMetatileSelection();
    int initialX = fromScriptCall ? x : this->paint_tile_initial_x;
    int initialY = fromScriptCall ? y : this->paint_tile_initial_y;

    // Snap the selected position to the top-left of the block boundary.
    // This allows painting via dragging the mouse to tile the painted region.
    int xDiff = x - initialX;
    int yDiff = y - initialY;
    if (xDiff < 0 && xDiff % selection.dimensions.x() != 0) xDiff -= selection.dimensions.x();
    if (yDiff < 0 && yDiff % selection.dimensions.y() != 0) yDiff -= selection.dimensions.y();

    x = initialX + (xDiff / selection.dimensions.x()) * selection.dimensions.x();
    y = initialY + (yDiff / selection.dimensions.y()) * selection.dimensions.y();

    // for edit history
    Blockdata oldMetatiles = !fromScriptCall ? this->layout->blockdata : Blockdata();

    for (int i = 0; i < selection.dimensions.x() && i + x < this->layout->getWidth(); i++)
    for (int j = 0; j < selection.dimensions.y() && j + y < this->layout->getHeight(); j++) {
        int actualX = i + x;
        int actualY = j + y;
        Block block;
        if (this->layout->getBlock(actualX, actualY, &block)) {
            int index = j * selection.dimensions.x() + i;
            MetatileSelectionItem item = selection.metatileItems.at(index);
            if (!item.enabled)
                continue;
            block.setMetatileId(item.metatileId);
            if (selection.hasCollision && selection.collisionItems.length() == selection.metatileItems.length()) {
                CollisionSelectionItem collisionItem = selection.collisionItems.at(index);
                block.setCollision(collisionItem.collision);
                block.setElevation(collisionItem.elevation);
            }
            this->layout->setBlock(actualX, actualY, block, !fromScriptCall);
        }
    }

    if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
        this->layout->editHistory.push(new PaintMetatile(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
    }
}

// These are tile offsets from the top-left tile in the 3x3 smart path selection.
// Each entry is for one possibility from the marching squares value for a tile.
// (Marching Squares: https://en.wikipedia.org/wiki/Marching_squares)
QList<int> LayoutPixmapItem::smartPathTable = QList<int>({
    4, // 0000
    4, // 0001
    4, // 0010
    6, // 0011
    4, // 0100
    4, // 0101
    0, // 0110
    3, // 0111
    4, // 1000
    8, // 1001
    4, // 1010
    7, // 1011
    2, // 1100
    5, // 1101
    1, // 1110
    4, // 1111
});

#define IS_SMART_PATH_TILE(block) (selectedMetatiles->contains(block.metatileId))

bool isSmartPathTile(QList<MetatileSelectionItem> metatileItems, uint16_t metatileId) {
    for (int i = 0; i < metatileItems.length(); i++) {
        if (metatileItems.at(i).metatileId == metatileId) {
            return true;
        }
    }
    return false;
}

bool isValidSmartPathSelection(MetatileSelection selection) {
    if (selection.dimensions.x() != 3 || selection.dimensions.y() != 3)
        return false;

    for (int i = 0; i < selection.metatileItems.length(); i++) {
        if (!selection.metatileItems.at(i).enabled)
            return false;
    }

    return true;
}

void LayoutPixmapItem::paintSmartPath(int x, int y, bool fromScriptCall) {
    MetatileSelection selection = this->metatileSelector->getMetatileSelection();
    if (!isValidSmartPathSelection(selection))
        return;

    // Shift to the middle tile of the smart path selection.
    uint16_t openMetatileId = selection.metatileItems.at(4).metatileId;
    uint16_t openCollision = 0;
    uint16_t openElevation = 0;
    bool setCollisions = false;
    if (selection.hasCollision && selection.collisionItems.length() == selection.metatileItems.length()) {
        openCollision = selection.collisionItems.at(4).collision;
        openElevation = selection.collisionItems.at(4).elevation;
        setCollisions = true;
    }

    // for edit history
    Blockdata oldMetatiles = !fromScriptCall ? this->layout->blockdata : Blockdata();

    // Fill the region with the open tile.
    for (int i = 0; i <= 1; i++)
    for (int j = 0; j <= 1; j++) {
        if (!this->layout->isWithinBounds(x + i, y + j))
            continue;
        int actualX = i + x;
        int actualY = j + y;
        Block block;
        if (this->layout->getBlock(actualX, actualY, &block)) {
            block.setMetatileId(openMetatileId);
            if (setCollisions) {
                block.setCollision(openCollision);
                block.setElevation(openElevation);
            }
            this->layout->setBlock(actualX, actualY, block, !fromScriptCall);
        }
    }

    // Go back and resolve the edge tiles
    for (int i = -1; i <= 2; i++)
    for (int j = -1; j <= 2; j++) {
        if (!this->layout->isWithinBounds(x + i, y + j))
            continue;
        // Ignore the corners, which can't possible be affected by the smart path.
        if ((i == -1 && j == -1) || (i == 2 && j == -1) ||
            (i == -1 && j ==  2) || (i == 2 && j ==  2))
            continue;

        // Ignore tiles that aren't part of the smart path set.
        int actualX = i + x;
        int actualY = j + y;
        Block block;
        if (!this->layout->getBlock(actualX, actualY, &block) || !isSmartPathTile(selection.metatileItems, block.metatileId())) {
            continue;
        }

        int id = 0;
        Block top;
        Block right;
        Block bottom;
        Block left;

        // Get marching squares value, to determine which tile to use.
        if (this->layout->getBlock(actualX, actualY - 1, &top) && isSmartPathTile(selection.metatileItems, top.metatileId()))
            id += 1;
        if (this->layout->getBlock(actualX + 1, actualY, &right) && isSmartPathTile(selection.metatileItems, right.metatileId()))
            id += 2;
        if (this->layout->getBlock(actualX, actualY + 1, &bottom) && isSmartPathTile(selection.metatileItems, bottom.metatileId()))
            id += 4;
        if (this->layout->getBlock(actualX - 1, actualY, &left) && isSmartPathTile(selection.metatileItems, left.metatileId()))
            id += 8;

        block.setMetatileId(selection.metatileItems.at(smartPathTable[id]).metatileId);
        if (setCollisions) {
            CollisionSelectionItem collisionItem = selection.collisionItems.at(smartPathTable[id]);
            block.setCollision(collisionItem.collision);
            block.setElevation(collisionItem.elevation);
        }
        this->layout->setBlock(actualX, actualY, block, !fromScriptCall);
    }

    if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
        this->layout->editHistory.push(new PaintMetatile(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
    }
}

void LayoutPixmapItem::lockNondominantAxis(QGraphicsSceneMouseEvent *event) {
    /* Return if an axis is already locked, or if the mouse has been released. The mouse release check is necessary
     * because LayoutPixmapItem::mouseReleaseEvent seems to get called before this function, which would unlock the axis
     * and then get immediately re-locked here until the next ctrl-click. */
    if (this->lockedAxis != LayoutPixmapItem::Axis::None || event->type() == QEvent::GraphicsSceneMouseRelease)
        return;

    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (!this->prevStraightPathState) {
        this->prevStraightPathState = true;
        this->straight_path_initial_x = pos.x();
        this->straight_path_initial_y = pos.y();
    }

    // Only lock an axis when the current position != initial
    int xDiff = pos.x() - this->straight_path_initial_x;
    int yDiff = pos.y() - this->straight_path_initial_y;
    if (xDiff || yDiff) {
        if (abs(xDiff) < abs(yDiff)) {
            this->lockedAxis = LayoutPixmapItem::Axis::X;
        } else {
            this->lockedAxis = LayoutPixmapItem::Axis::Y;
        }
    }
}

// Adjust the cooresponding coordinate when it is locked
QPoint LayoutPixmapItem::adjustCoords(QPoint pos) {
    if (this->lockedAxis == LayoutPixmapItem::Axis::X) {
        pos.setX(this->straight_path_initial_x);
    } else if (this->lockedAxis == LayoutPixmapItem::Axis::Y) {
        pos.setY(this->straight_path_initial_y);
    }
    return pos;
}

void LayoutPixmapItem::updateMetatileSelection(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());

    // Snap point to within layout bounds.
    if (pos.x() < 0) pos.setX(0);
    if (pos.x() >= this->layout->getWidth()) pos.setX(this->layout->getWidth() - 1);
    if (pos.y() < 0) pos.setY(0);
    if (pos.y() >= this->layout->getHeight()) pos.setY(this->layout->getHeight() - 1);

    // Update/apply copied metatiles.
    if (event->type() == QEvent::GraphicsSceneMousePress) {
        this->lastMetatileSelectionPos = pos;
        selection_origin = QPoint(pos.x(), pos.y());
        selection.clear();
        selection.append(QPoint(pos.x(), pos.y()));
        Block block;
        if (this->layout->getBlock(pos.x(), pos.y(), &block)) {
            this->metatileSelector->selectFromMap(block.metatileId(), block.collision(), block.elevation());
        }
    } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
        if (pos == this->lastMetatileSelectionPos)
            return;
        this->lastMetatileSelectionPos = pos;
        int x1 = selection_origin.x();
        int y1 = selection_origin.y();
        int x2 = pos.x();
        int y2 = pos.y();
        if (x1 > x2) SWAP(x1, x2);
        if (y1 > y2) SWAP(y1, y2);
        selection.clear();
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                selection.append(QPoint(x, y));
            }
        }

        QList<uint16_t> metatiles;
        QList<QPair<uint16_t, uint16_t>> collisions;
        for (QPoint point : selection) {
            int x = point.x();
            int y = point.y();
            Block block;
            if (this->layout->getBlock(x, y, &block)) {
                metatiles.append(block.metatileId());
            }
            int blockIndex = y * this->layout->getWidth() + x;
            block = this->layout->blockdata.at(blockIndex);
            auto collision = block.collision();
            auto elevation = block.elevation();
            collisions.append(QPair<uint16_t, uint16_t>(collision, elevation));
        }

        this->metatileSelector->setExternalSelection(x2 - x1 + 1, y2 - y1 + 1, metatiles, collisions);
    }
}

void LayoutPixmapItem::floodFill(QGraphicsSceneMouseEvent *event) {
    if (this->layout) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            actionId_++;
        } else {
            QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
            Block block;
            MetatileSelection selection = this->metatileSelector->getMetatileSelection();
            int metatileId = selection.metatileItems.first().metatileId;
            if (selection.metatileItems.count() > 1 || (this->layout->getBlock(pos.x(), pos.y(), &block) && block.metatileId() != metatileId)) {
                bool smartPathsEnabled = event->modifiers() & Qt::ShiftModifier;
                if ((this->settings->smartPathsEnabled || smartPathsEnabled) && selection.dimensions.x() == 3 && selection.dimensions.y() == 3)
                    this->floodFillSmartPath(pos.x(), pos.y());
                else
                    this->floodFill(pos.x(), pos.y());
            }
        }
    }
}

void LayoutPixmapItem::magicFill(QGraphicsSceneMouseEvent *event) {
    if (this->layout) {
        if (event->type() == QEvent::GraphicsSceneMouseRelease) {
            actionId_++;
        } else {
            QPoint initialPos = Metatile::coordFromPixmapCoord(event->pos());
            this->magicFill(initialPos.x(), initialPos.y());
        }
    }
}

void LayoutPixmapItem::magicFill(int x, int y, uint16_t metatileId, bool fromScriptCall) {
    QPoint selectionDimensions(1, 1);
    QList<MetatileSelectionItem> selectedMetatiles = QList<MetatileSelectionItem>({MetatileSelectionItem{ true, metatileId }});
    this->magicFill(x, y, selectionDimensions, selectedMetatiles, QList<CollisionSelectionItem>(), fromScriptCall);
}

void LayoutPixmapItem::magicFill(int x, int y, bool fromScriptCall) {
    MetatileSelection selection = this->metatileSelector->getMetatileSelection();
    this->magicFill(x, y, selection.dimensions, selection.metatileItems, selection.collisionItems, fromScriptCall);
}

void LayoutPixmapItem::magicFill(
        int initialX,
        int initialY,
        QPoint selectionDimensions,
        QList<MetatileSelectionItem> selectedMetatiles,
        QList<CollisionSelectionItem> selectedCollisions,
        bool fromScriptCall) {
    Block block;
    if (this->layout->getBlock(initialX, initialY, &block)) {
        if (selectedMetatiles.length() == 1 && selectedMetatiles.at(0).metatileId == block.metatileId()) {
            return;
        }

        Blockdata oldMetatiles = !fromScriptCall ? this->layout->blockdata : Blockdata();

        bool setCollisions = selectedCollisions.length() == selectedMetatiles.length();
        uint16_t metatileId = block.metatileId();
        for (int y = 0; y < this->layout->getHeight(); y++) {
            for (int x = 0; x < this->layout->getWidth(); x++) {
                if (this->layout->getBlock(x, y, &block) && block.metatileId() == metatileId) {
                    int xDiff = x - initialX;
                    int yDiff = y - initialY;
                    int i = xDiff % selectionDimensions.x();
                    int j = yDiff % selectionDimensions.y();
                    if (i < 0) i = selectionDimensions.x() + i;
                    if (j < 0) j = selectionDimensions.y() + j;
                    int index = j * selectionDimensions.x() + i;
                    if (selectedMetatiles.at(index).enabled) {
                        block.setMetatileId(selectedMetatiles.at(index).metatileId);
                        if (setCollisions) {
                            CollisionSelectionItem item = selectedCollisions.at(index);
                            block.setCollision(item.collision);
                            block.setElevation(item.elevation);
                        }
                        this->layout->setBlock(x, y, block, !fromScriptCall);
                    }
                }
            }
        }

        if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
            this->layout->editHistory.push(new MagicFillMetatile(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
        }
    }
}

void LayoutPixmapItem::floodFill(int initialX, int initialY, bool fromScriptCall) {
    MetatileSelection selection = this->metatileSelector->getMetatileSelection();
    this->floodFill(initialX, initialY, selection.dimensions, selection.metatileItems, selection.collisionItems, fromScriptCall);
}

void LayoutPixmapItem::floodFill(int initialX, int initialY, uint16_t metatileId, bool fromScriptCall) {
    QPoint selectionDimensions(1, 1);
    QList<MetatileSelectionItem> selectedMetatiles = QList<MetatileSelectionItem>({MetatileSelectionItem{true, metatileId}});
    this->floodFill(initialX, initialY, selectionDimensions, selectedMetatiles, QList<CollisionSelectionItem>(), fromScriptCall);
}

void LayoutPixmapItem::floodFill(
        int initialX,
        int initialY,
        QPoint selectionDimensions,
        QList<MetatileSelectionItem> selectedMetatiles,
        QList<CollisionSelectionItem> selectedCollisions,
        bool fromScriptCall) {
    bool setCollisions = selectedCollisions.length() == selectedMetatiles.length();
    Blockdata oldMetatiles = !fromScriptCall ? this->layout->blockdata : Blockdata();

    QSet<int> visited;
    QList<QPoint> todo;
    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();
        Block block;
        if (!this->layout->getBlock(x, y, &block)) {
            continue;
        }

        visited.insert(x + y * this->layout->getWidth());
        int xDiff = x - initialX;
        int yDiff = y - initialY;
        int i = xDiff % selectionDimensions.x();
        int j = yDiff % selectionDimensions.y();
        if (i < 0) i = selectionDimensions.x() + i;
        if (j < 0) j = selectionDimensions.y() + j;
        int index = j * selectionDimensions.x() + i;
        uint16_t metatileId = selectedMetatiles.at(index).metatileId;
        uint16_t old_metatileId = block.metatileId();
        if (selectedMetatiles.at(index).enabled && (selectedMetatiles.count() != 1 || old_metatileId != metatileId)) {
            block.setMetatileId(metatileId);
            if (setCollisions) {
                CollisionSelectionItem item = selectedCollisions.at(index);
                block.setCollision(item.collision);
                block.setElevation(item.elevation);
            }
            this->layout->setBlock(x, y, block, !fromScriptCall);
        }
        if (!visited.contains(x + 1 + y * this->layout->getWidth()) && this->layout->getBlock(x + 1, y, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x + 1, y));
            visited.insert(x + 1 + y * this->layout->getWidth());
        }
        if (!visited.contains(x - 1 + y * this->layout->getWidth()) && this->layout->getBlock(x - 1, y, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x - 1, y));
            visited.insert(x - 1 + y * this->layout->getWidth());
        }
        if (!visited.contains(x + (y + 1) * this->layout->getWidth()) && this->layout->getBlock(x, y + 1, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x, y + 1));
            visited.insert(x + (y + 1) * this->layout->getWidth());
        }
        if (!visited.contains(x + (y - 1) * this->layout->getWidth()) && this->layout->getBlock(x, y - 1, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x, y - 1));
            visited.insert(x + (y - 1) * this->layout->getWidth());
        }
    }

    if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
        this->layout->editHistory.push(new BucketFillMetatile(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
    }
}

void LayoutPixmapItem::floodFillSmartPath(int initialX, int initialY, bool fromScriptCall) {
    MetatileSelection selection = this->metatileSelector->getMetatileSelection();
    if (!isValidSmartPathSelection(selection))
        return;

    // Shift to the middle tile of the smart path selection.
    uint16_t openMetatileId = selection.metatileItems.at(4).metatileId;
    uint16_t openCollision = 0;
    uint16_t openElevation = 0;
    bool setCollisions = false;
    if (selection.hasCollision && selection.collisionItems.length() == selection.metatileItems.length()) {
        CollisionSelectionItem item = selection.collisionItems.at(4);
        openCollision = item.collision;
        openElevation = item.elevation;
        setCollisions = true;
    }

    Blockdata oldMetatiles = !fromScriptCall ? this->layout->blockdata : Blockdata();

    // Flood fill the region with the open tile.
    QList<QPoint> todo;
    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();
        Block block;
        if (!this->layout->getBlock(x, y, &block)) {
            continue;
        }

        uint16_t old_metatileId = block.metatileId();
        if (old_metatileId == openMetatileId) {
            continue;
        }

        block.setMetatileId(openMetatileId);
        if (setCollisions) {
            block.setCollision(openCollision);
            block.setElevation(openElevation);
        }
        this->layout->setBlock(x, y, block, !fromScriptCall);
        if (this->layout->getBlock(x + 1, y, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x + 1, y));
        }
        if (this->layout->getBlock(x - 1, y, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x - 1, y));
        }
        if (this->layout->getBlock(x, y + 1, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x, y + 1));
        }
        if (this->layout->getBlock(x, y - 1, &block) && block.metatileId() == old_metatileId) {
            todo.append(QPoint(x, y - 1));
        }
    }

    // Go back and resolve the flood-filled edge tiles.
    // Mark tiles as visited while we go.
    QSet<int> visited;
    todo.append(QPoint(initialX, initialY));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        int x = point.x();
        int y = point.y();
        Block block;
        if (!this->layout->getBlock(x, y, &block)) {
            continue;
        }

        visited.insert(x + y * this->layout->getWidth());
        int id = 0;
        Block top;
        Block right;
        Block bottom;
        Block left;

        // Get marching squares value, to determine which tile to use.
        if (this->layout->getBlock(x, y - 1, &top) && isSmartPathTile(selection.metatileItems, top.metatileId()))
            id += 1;
        if (this->layout->getBlock(x + 1, y, &right) && isSmartPathTile(selection.metatileItems, right.metatileId()))
            id += 2;
        if (this->layout->getBlock(x, y + 1, &bottom) && isSmartPathTile(selection.metatileItems, bottom.metatileId()))
            id += 4;
        if (this->layout->getBlock(x - 1, y, &left) && isSmartPathTile(selection.metatileItems, left.metatileId()))
            id += 8;

        block.setMetatileId(selection.metatileItems.at(smartPathTable[id]).metatileId);
        if (setCollisions) {
            CollisionSelectionItem item = selection.collisionItems.at(smartPathTable[id]);
            block.setCollision(item.collision);
            block.setElevation(item.elevation);
        }
        this->layout->setBlock(x, y, block, !fromScriptCall);

        // Visit neighbors if they are smart-path tiles, and don't revisit any.
        if (!visited.contains(x + 1 + y * this->layout->getWidth()) && this->layout->getBlock(x + 1, y, &block) && isSmartPathTile(selection.metatileItems, block.metatileId())) {
            todo.append(QPoint(x + 1, y));
            visited.insert(x + 1 + y * this->layout->getWidth());
        }
        if (!visited.contains(x - 1 + y * this->layout->getWidth()) && this->layout->getBlock(x - 1, y, &block) && isSmartPathTile(selection.metatileItems, block.metatileId())) {
            todo.append(QPoint(x - 1, y));
            visited.insert(x - 1 + y * this->layout->getWidth());
        }
        if (!visited.contains(x + (y + 1) * this->layout->getWidth()) && this->layout->getBlock(x, y + 1, &block) && isSmartPathTile(selection.metatileItems, block.metatileId())) {
            todo.append(QPoint(x, y + 1));
            visited.insert(x + (y + 1) * this->layout->getWidth());
        }
        if (!visited.contains(x + (y - 1) * this->layout->getWidth()) && this->layout->getBlock(x, y - 1, &block) && isSmartPathTile(selection.metatileItems, block.metatileId())) {
            todo.append(QPoint(x, y - 1));
            visited.insert(x + (y - 1) * this->layout->getWidth());
        }
    }

    if (!fromScriptCall && this->layout->blockdata != oldMetatiles) {
        this->layout->editHistory.push(new BucketFillMetatile(this->layout, oldMetatiles, this->layout->blockdata, actionId_));
    }
}

void LayoutPixmapItem::pick(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    Block block;
    if (this->layout->getBlock(pos.x(), pos.y(), &block)) {
        this->metatileSelector->selectFromMap(block.metatileId(), block.collision(), block.elevation());
    }
}

void LayoutPixmapItem::select(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (event->type() == QEvent::GraphicsSceneMousePress) {
        selection_origin = QPoint(pos.x(), pos.y());
        selection.clear();
    } else if (event->type() == QEvent::GraphicsSceneMouseMove) {
        if (event->buttons() & Qt::LeftButton) {
            selection.clear();
            selection.append(QPoint(pos.x(), pos.y()));
        }
    } else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
        if (!selection.isEmpty()) {
            QPoint pos = selection.last();
            int x1 = selection_origin.x();
            int y1 = selection_origin.y();
            int x2 = pos.x();
            int y2 = pos.y();
            if (x1 > x2) SWAP(x1, x2);
            if (y1 > y2) SWAP(y1, y2);
            selection.clear();
            for (int y = y1; y <= y2; y++) {
                for (int x = x1; x <= x2; x++) {
                    selection.append(QPoint(x, y));
                }
            }
        }
    }
}

void LayoutPixmapItem::draw(bool ignoreCache) {
    if (this->layout) {
        layout->setLayoutItem(this);
        setPixmap(this->layout->render(ignoreCache));
    }
}

void LayoutPixmapItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (pos != this->metatilePos) {
        this->metatilePos = pos;
        emit this->hoverChanged(pos);
    }
}

void LayoutPixmapItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    this->has_mouse = true;
    this->metatilePos = Metatile::coordFromPixmapCoord(event->pos());
    emit this->hoverEntered(this->metatilePos);
}

void LayoutPixmapItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    this->has_mouse = false;
    emit this->hoverCleared();
}

void LayoutPixmapItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    this->metatilePos = Metatile::coordFromPixmapCoord(event->pos());
    this->paint_tile_initial_x = this->straight_path_initial_x = this->metatilePos.x();
    this->paint_tile_initial_y = this->straight_path_initial_y = this->metatilePos.y();
    emit startPaint(event, this);
    emit mouseEvent(event, this);
}

void LayoutPixmapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    QPoint pos = Metatile::coordFromPixmapCoord(event->pos());
    if (pos == this->metatilePos)
        return;

    this->metatilePos = pos;
    emit hoverChanged(pos);
    emit mouseEvent(event, this);
}

void LayoutPixmapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    this->lockedAxis = LayoutPixmapItem::Axis::None;
    emit endPaint(event, this);
    emit mouseEvent(event, this);
}
