#include "history.h"
#include "map.h"
#include "imageproviders.h"
#include "scripting.h"

#include "editcommands.h"

#include <QTime>
#include <QPainter>
#include <QImage>
#include <QRegularExpression>


Map::Map(QObject *parent) : QObject(parent)
{
    editHistory.setClean();

    // Initialize events map with empty lists
    for (int i = 0; i < static_cast<int>(Event::Group::None); i++) {
        events[static_cast<Event::Group>(i)];
    }
}

Map::~Map() {
    qDeleteAll(ownedEvents);
    ownedEvents.clear();
    deleteConnections();
}

void Map::setName(QString mapName) {
    name = mapName;
    scriptsLoaded = false;
}

QString Map::mapConstantFromName(QString mapName, bool includePrefix) {
    // Transform map names of the form 'GraniteCave_B1F` into map constants like 'MAP_GRANITE_CAVE_B1F'.
    static const QRegularExpression caseChange("([a-z])([A-Z])");
    QString nameWithUnderscores = mapName.replace(caseChange, "\\1_\\2");
    const QString prefix = includePrefix ? projectConfig.getIdentifier(ProjectIdentifier::define_map_prefix) : "";
    QString withMapAndUppercase = prefix + nameWithUnderscores.toUpper();
    static const QRegularExpression underscores("_+");
    return withMapAndUppercase.replace(underscores, "_");
}

int Map::getWidth() {
    return layout->getWidth();
}

int Map::getHeight() {
    return layout->getHeight();
}

int Map::getBorderWidth() {
    return layout->getBorderWidth();
}

int Map::getBorderHeight() {
    return layout->getBorderHeight();
}

bool Map::mapBlockChanged(int i, const Blockdata &cache) {
    if (cache.length() <= i)
        return true;
    if (layout->blockdata.length() <= i)
        return true;

    return layout->blockdata.at(i) != cache.at(i);
}

bool Map::borderBlockChanged(int i, const Blockdata &cache) {
    if (cache.length() <= i)
        return true;
    if (layout->border.length() <= i)
        return true;

    return layout->border.at(i) != cache.at(i);
}

void Map::clearBorderCache() {
    layout->cached_border.clear();
}

void Map::cacheBorder() {
    layout->cached_border.clear();
    for (const auto &block : layout->border)
        layout->cached_border.append(block);
}

void Map::cacheBlockdata() {
    layout->cached_blockdata.clear();
    for (const auto &block : layout->blockdata)
        layout->cached_blockdata.append(block);
}

void Map::cacheCollision() {
    layout->cached_collision.clear();
    for (const auto &block : layout->blockdata)
        layout->cached_collision.append(block);
}

QPixmap Map::renderCollision(bool ignoreCache) {
    bool changed_any = false;
    int width_ = getWidth();
    int height_ = getHeight();
    if (collision_image.isNull() || collision_image.width() != width_ * 16 || collision_image.height() != height_ * 16) {
        collision_image = QImage(width_ * 16, height_ * 16, QImage::Format_RGBA8888);
        changed_any = true;
    }
    if (layout->blockdata.isEmpty() || !width_ || !height_) {
        collision_pixmap = collision_pixmap.fromImage(collision_image);
        return collision_pixmap;
    }
    QPainter painter(&collision_image);
    for (int i = 0; i < layout->blockdata.length(); i++) {
        if (!ignoreCache && !mapBlockChanged(i, layout->cached_collision)) {
            continue;
        }
        changed_any = true;
        Block block = layout->blockdata.at(i);
        QImage collision_metatile_image = getCollisionMetatileImage(block);
        int map_y = width_ ? i / width_ : 0;
        int map_x = width_ ? i % width_ : 0;
        QPoint metatile_origin = QPoint(map_x * 16, map_y * 16);
        painter.drawImage(metatile_origin, collision_metatile_image);
    }
    painter.end();
    cacheCollision();
    if (changed_any) {
        collision_pixmap = collision_pixmap.fromImage(collision_image);
    }
    return collision_pixmap;
}

QPixmap Map::render(bool ignoreCache, MapLayout * fromLayout, QRect bounds) {
    bool changed_any = false;
    int width_ = getWidth();
    int height_ = getHeight();
    if (image.isNull() || image.width() != width_ * 16 || image.height() != height_ * 16) {
        image = QImage(width_ * 16, height_ * 16, QImage::Format_RGBA8888);
        changed_any = true;
    }
    if (layout->blockdata.isEmpty() || !width_ || !height_) {
        pixmap = pixmap.fromImage(image);
        return pixmap;
    }

    QPainter painter(&image);
    for (int i = 0; i < layout->blockdata.length(); i++) {
        if (!ignoreCache && !mapBlockChanged(i, layout->cached_blockdata)) {
            continue;
        }
        changed_any = true;
        int map_y = width_ ? i / width_ : 0;
        int map_x = width_ ? i % width_ : 0;
        if (bounds.isValid() && !bounds.contains(map_x, map_y)) {
            continue;
        }
        QPoint metatile_origin = QPoint(map_x * 16, map_y * 16);
        Block block = layout->blockdata.at(i);
        QImage metatile_image = getMetatileImage(
            block.metatileId(),
            fromLayout ? fromLayout->tileset_primary   : layout->tileset_primary,
            fromLayout ? fromLayout->tileset_secondary : layout->tileset_secondary,
            metatileLayerOrder,
            metatileLayerOpacity
        );
        painter.drawImage(metatile_origin, metatile_image);
    }
    painter.end();
    if (changed_any) {
        cacheBlockdata();
        pixmap = pixmap.fromImage(image);
    }

    return pixmap;
}

QPixmap Map::renderBorder(bool ignoreCache) {
    bool changed_any = false, border_resized = false;
    int width_ = getBorderWidth();
    int height_ = getBorderHeight();
    if (layout->border_image.isNull()) {
        layout->border_image = QImage(width_ * 16, height_ * 16, QImage::Format_RGBA8888);
        changed_any = true;
    }
    if (layout->border_image.width() != width_ * 16 || layout->border_image.height() != height_ * 16) {
        layout->border_image = QImage(width_ * 16, height_ * 16, QImage::Format_RGBA8888);
        border_resized = true;
    }
    if (layout->border.isEmpty()) {
        layout->border_pixmap = layout->border_pixmap.fromImage(layout->border_image);
        return layout->border_pixmap;
    }
    QPainter painter(&layout->border_image);
    for (int i = 0; i < layout->border.length(); i++) {
        if (!ignoreCache && (!border_resized && !borderBlockChanged(i, layout->cached_border))) {
            continue;
        }

        changed_any = true;
        Block block = layout->border.at(i);
        uint16_t metatileId = block.metatileId();
        QImage metatile_image = getMetatileImage(metatileId, layout->tileset_primary, layout->tileset_secondary, metatileLayerOrder, metatileLayerOpacity);
        int map_y = width_ ? i / width_ : 0;
        int map_x = width_ ? i % width_ : 0;
        painter.drawImage(QPoint(map_x * 16, map_y * 16), metatile_image);
    }
    painter.end();
    if (changed_any) {
        cacheBorder();
        layout->border_pixmap = layout->border_pixmap.fromImage(layout->border_image);
    }
    return layout->border_pixmap;
}

// Get the portion of the map that can be rendered when rendered as a map connection.
// Cardinal connections render the nearest segment of their map and within the bounds of the border draw distance,
// Dive/Emerge connections are rendered normally within the bounds of their parent map.
QRect Map::getConnectionRect(const QString &direction, MapLayout * fromLayout) {
    int x = 0, y = 0;
    int w = getWidth(), h = getHeight();

    if (direction == "up") {
        h = qMin(h, BORDER_DISTANCE);
        y = getHeight() - h;
    } else if (direction == "down") {
        h = qMin(h, BORDER_DISTANCE);
    } else if (direction == "left") {
        w = qMin(w, BORDER_DISTANCE);
        x = getWidth() - w;
    } else if (direction == "right") {
        w = qMin(w, BORDER_DISTANCE);
    } else if (MapConnection::isDiving(direction)) {
        if (fromLayout) {
            w = qMin(w, fromLayout->getWidth());
            h = qMin(h, fromLayout->getHeight());
        }
    } else {
        // Unknown direction
        return QRect();
    }
    return QRect(x, y, w, h);
}

QPixmap Map::renderConnection(const QString &direction, MapLayout * fromLayout) {
    QRect bounds = getConnectionRect(direction, fromLayout);
    if (!bounds.isValid())
        return QPixmap();

    // 'fromLayout' will be used in 'render' to get the palettes from the parent map.
    // Dive/Emerge connections render normally with their own palettes, so we ignore this.
    if (MapConnection::isDiving(direction))
        fromLayout = nullptr;

    render(true, fromLayout, bounds);
    QImage connection_image = image.copy(bounds.x() * 16, bounds.y() * 16, bounds.width() * 16, bounds.height() * 16);
    return QPixmap::fromImage(connection_image);
}

void Map::setNewDimensionsBlockdata(int newWidth, int newHeight) {
    int oldWidth = getWidth();
    int oldHeight = getHeight();

    Blockdata newBlockdata;

    for (int y = 0; y < newHeight; y++)
    for (int x = 0; x < newWidth; x++) {
        if (x < oldWidth && y < oldHeight) {
            int index = y * oldWidth + x;
            newBlockdata.append(layout->blockdata.value(index));
        } else {
            newBlockdata.append(0);
        }
    }

    layout->blockdata = newBlockdata;
}

void Map::setNewBorderDimensionsBlockdata(int newWidth, int newHeight) {
    int oldWidth = getBorderWidth();
    int oldHeight = getBorderHeight();

    Blockdata newBlockdata;

    for (int y = 0; y < newHeight; y++)
    for (int x = 0; x < newWidth; x++) {
        if (x < oldWidth && y < oldHeight) {
            int index = y * oldWidth + x;
            newBlockdata.append(layout->border.value(index));
        } else {
            newBlockdata.append(0);
        }
    }

    layout->border = newBlockdata;
}

void Map::setDimensions(int newWidth, int newHeight, bool setNewBlockdata, bool enableScriptCallback) {
    if (setNewBlockdata) {
        setNewDimensionsBlockdata(newWidth, newHeight);
    }

    int oldWidth = layout->width;
    int oldHeight = layout->height;
    layout->width = newWidth;
    layout->height = newHeight;

    if (enableScriptCallback && (oldWidth != newWidth || oldHeight != newHeight)) {
        Scripting::cb_MapResized(oldWidth, oldHeight, newWidth, newHeight);
    }

    emit mapDimensionsChanged(QSize(getWidth(), getHeight()));
    modify();
}

void Map::setBorderDimensions(int newWidth, int newHeight, bool setNewBlockdata, bool enableScriptCallback) {
    if (setNewBlockdata) {
        setNewBorderDimensionsBlockdata(newWidth, newHeight);
    }

    int oldWidth = layout->border_width;
    int oldHeight = layout->border_height;
    layout->border_width = newWidth;
    layout->border_height = newHeight;

    if (enableScriptCallback && (oldWidth != newWidth || oldHeight != newHeight)) {
        Scripting::cb_BorderResized(oldWidth, oldHeight, newWidth, newHeight);
    }

    modify();
}

void Map::openScript(QString label) {
    emit openScriptRequested(label);
}

bool Map::getBlock(int x, int y, Block *out) {
    if (isWithinBounds(x, y)) {
        int i = y * getWidth() + x;
        *out = layout->blockdata.value(i);
        return true;
    }
    return false;
}

void Map::setBlock(int x, int y, Block block, bool enableScriptCallback) {
    if (!isWithinBounds(x, y)) return;
    int i = y * getWidth() + x;
    if (i < layout->blockdata.size()) {
        Block prevBlock = layout->blockdata.at(i);
        layout->blockdata.replace(i, block);
        if (enableScriptCallback) {
            Scripting::cb_MetatileChanged(x, y, prevBlock, block);
        }
    }
}

void Map::setBlockdata(Blockdata blockdata, bool enableScriptCallback) {
    int width = getWidth();
    int size = qMin(blockdata.size(), layout->blockdata.size());
    for (int i = 0; i < size; i++) {
        Block prevBlock = layout->blockdata.at(i);
        Block newBlock = blockdata.at(i);
        if (prevBlock != newBlock) {
            layout->blockdata.replace(i, newBlock);
            if (enableScriptCallback)
                Scripting::cb_MetatileChanged(i % width, i / width, prevBlock, newBlock);
        }
    }
}

uint16_t Map::getBorderMetatileId(int x, int y) {
    int i = y * getBorderWidth() + x;
    return layout->border[i].metatileId();
}

void Map::setBorderMetatileId(int x, int y, uint16_t metatileId, bool enableScriptCallback) {
    int i = y * getBorderWidth() + x;
    if (i < layout->border.size()) {
        uint16_t prevMetatileId = layout->border[i].metatileId();
        layout->border[i].setMetatileId(metatileId);
        if (prevMetatileId != metatileId && enableScriptCallback) {
            Scripting::cb_BorderMetatileChanged(x, y, prevMetatileId, metatileId);
        }
    }
}

void Map::setBorderBlockData(Blockdata blockdata, bool enableScriptCallback) {
    int width = getBorderWidth();
    int size = qMin(blockdata.size(), layout->border.size());
    for (int i = 0; i < size; i++) {
        Block prevBlock = layout->border.at(i);
        Block newBlock = blockdata.at(i);
        if (prevBlock != newBlock) {
            layout->border.replace(i, newBlock);
            if (enableScriptCallback)
                Scripting::cb_BorderMetatileChanged(i % width, i / width, prevBlock.metatileId(), newBlock.metatileId());
        }
    }
}

void Map::_floodFillCollisionElevation(int x, int y, uint16_t collision, uint16_t elevation) {
    QList<QPoint> todo;
    todo.append(QPoint(x, y));
    while (todo.length()) {
        QPoint point = todo.takeAt(0);
        x = point.x();
        y = point.y();
        Block block;
        if (!getBlock(x, y, &block)) {
            continue;
        }

        uint old_coll = block.collision();
        uint old_elev = block.elevation();
        if (old_coll == collision && old_elev == elevation) {
            continue;
        }

        block.setCollision(collision);
        block.setElevation(elevation);
        setBlock(x, y, block, true);
        if (getBlock(x + 1, y, &block) && block.collision() == old_coll && block.elevation() == old_elev) {
            todo.append(QPoint(x + 1, y));
        }
        if (getBlock(x - 1, y, &block) && block.collision() == old_coll && block.elevation() == old_elev) {
            todo.append(QPoint(x - 1, y));
        }
        if (getBlock(x, y + 1, &block) && block.collision() == old_coll && block.elevation() == old_elev) {
            todo.append(QPoint(x, y + 1));
        }
        if (getBlock(x, y - 1, &block) && block.collision() == old_coll && block.elevation() == old_elev) {
            todo.append(QPoint(x, y - 1));
        }
    }
}

void Map::floodFillCollisionElevation(int x, int y, uint16_t collision, uint16_t elevation) {
    Block block;
    if (getBlock(x, y, &block) && (block.collision() != collision || block.elevation() != elevation)) {
        _floodFillCollisionElevation(x, y, collision, elevation);
    }
}

void Map::magicFillCollisionElevation(int initialX, int initialY, uint16_t collision, uint16_t elevation) {
    Block block;
    if (getBlock(initialX, initialY, &block) && (block.collision() != collision || block.elevation() != elevation)) {
        uint old_coll = block.collision();
        uint old_elev = block.elevation();

        for (int y = 0; y < getHeight(); y++) {
            for (int x = 0; x < getWidth(); x++) {
                if (getBlock(x, y, &block) && block.collision() == old_coll && block.elevation() == old_elev) {
                    block.setCollision(collision);
                    block.setElevation(elevation);
                    setBlock(x, y, block, true);
                }
            }
        }
    }
}

bool Map::hasEvents() const {
    for (const auto &event_list : events) {
        if (event_list.length())
            return true;
    }
    return false;
}

QList<Event *> Map::getAllEvents() const {
    QList<Event *> all_events;
    for (const auto &event_list : events) {
        all_events << event_list;
    }
    return all_events;
}

QStringList Map::getScriptLabels(Event::Group group) {
    if (!this->scriptsLoaded) {
        this->scriptsFileLabels = ParseUtil::getGlobalScriptLabels(this->getScriptsFilePath());
        this->scriptsLoaded = true;
    }

    QStringList scriptLabels;

    // Get script labels currently in-use by the map's events
    if (group == Event::Group::None) {
        ScriptTracker scriptTracker;
        for (Event *event : this->getAllEvents()) {
            event->accept(&scriptTracker);
        }
        scriptLabels = scriptTracker.getScripts();
    } else {
        ScriptTracker scriptTracker;
        for (Event *event : events.value(group)) {
            event->accept(&scriptTracker);
        }
        scriptLabels = scriptTracker.getScripts();
    }

    // Add scripts from map's scripts file, and empty names.
    scriptLabels.append(this->scriptsFileLabels);
    scriptLabels.sort(Qt::CaseInsensitive);
    scriptLabels.prepend("0x0");
    scriptLabels.prepend("NULL");

    scriptLabels.removeAll("");
    scriptLabels.removeDuplicates();

    return scriptLabels;
}

QString Map::getScriptsFilePath() const {
    const bool usePoryscript = projectConfig.usePoryScript;
    auto path = QDir::cleanPath(QString("%1/%2/%3/scripts")
                                        .arg(projectConfig.projectDir)
                                        .arg(projectConfig.getFilePath(ProjectFilePath::data_map_folders))
                                        .arg(this->name));
    auto extension = Project::getScriptFileExtension(usePoryscript);
    if (usePoryscript && !QFile::exists(path + extension))
        extension = Project::getScriptFileExtension(false);
    path += extension;
    return path;
}

void Map::removeEvent(Event *event) {
    for (Event::Group key : events.keys()) {
        events[key].removeAll(event);
    }
}

void Map::addEvent(Event *event) {
    event->setMap(this);
    events[event->getEventGroup()].append(event);
    if (!ownedEvents.contains(event)) ownedEvents.append(event);
}

void Map::deleteConnections() {
    qDeleteAll(this->ownedConnections);
    this->ownedConnections.clear();
    this->connections.clear();
}

QList<MapConnection*> Map::getConnections() const {
    return this->connections;
}

void Map::addConnection(MapConnection *connection) {
    if (!connection || this->connections.contains(connection))
        return;

    // Maps should only have one Dive/Emerge connection at a time.
    // (Users can technically have more by editing their data manually, but we will only display one at a time)
    // Any additional connections being added (this can happen via mirroring) are tracked for deleting but otherwise ignored.
    if (MapConnection::isDiving(connection->direction())) {
        for (auto i : this->connections) {
            if (i->direction() == connection->direction()) {
                trackConnection(connection);
                return;
            }
        }
    }

    loadConnection(connection);
    modify();
    emit connectionAdded(connection);
    return;
}

void Map::loadConnection(MapConnection *connection) {
    if (!connection)
        return;

    if (!this->connections.contains(connection))
        this->connections.append(connection);

    trackConnection(connection);
}

void Map::trackConnection(MapConnection *connection) {
    connection->setParentMap(this, false);

    if (!this->ownedConnections.contains(connection)) {
        this->ownedConnections.insert(connection);
        connect(connection, &MapConnection::parentMapChanged, [=](Map *, Map *after) {
            if (after != this && after != nullptr) {
                // MapConnection's parent has been reassigned, it's no longer our responsibility
                this->ownedConnections.remove(connection);
                QObject::disconnect(connection, &MapConnection::parentMapChanged, this, nullptr);
            }
        });
    }
}

// We retain ownership of this MapConnection until it's assigned to a new parent map.
void Map::removeConnection(MapConnection *connection) {
    if (!this->connections.removeOne(connection))
        return;
    connection->setParentMap(nullptr, false);
    modify();
    emit connectionRemoved(connection);
}

void Map::modify() {
    emit modified();
}

void Map::clean() {
    this->hasUnsavedDataChanges = false;
}

bool Map::hasUnsavedChanges() {
    return !editHistory.isClean() || hasUnsavedDataChanges || !isPersistedToFile;
}

void Map::pruneEditHistory() {
    // Edit history for map connections gets messy because edits on other maps can affect the current map.
    // To avoid complications we clear MapConnection edit history when the user opens a different map.
    // No other edits within a single map depend on MapConnections so they can be pruned safely.
    static const QSet<int> mapConnectionIds = {
        ID_MapConnectionMove,
        ID_MapConnectionChangeDirection,
        ID_MapConnectionChangeMap,
        ID_MapConnectionAdd,
        ID_MapConnectionRemove
    };
    for (int i = 0; i < this->editHistory.count(); i++) {
        // Qt really doesn't expect editing commands in the stack to be valid (fair).
        // A better future design might be to have separate edit histories per map tab,
        // and dumping the entire Connections tab history with QUndoStack::clear.
        auto command = const_cast<QUndoCommand*>(this->editHistory.command(i));
        if (mapConnectionIds.contains(command->id()))
            command->setObsolete(true);
    }
}

bool Map::isWithinBounds(int x, int y) {
    return (x >= 0 && x < this->getWidth() && y >= 0 && y < this->getHeight());
}

bool Map::isWithinBorderBounds(int x, int y) {
    return (x >= 0 && x < this->getBorderWidth() && y >= 0 && y < this->getBorderHeight());
}
