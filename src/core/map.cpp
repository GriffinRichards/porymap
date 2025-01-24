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
    m_editHistory = new QUndoStack(this);
    resetEvents();

    m_header = new MapHeader(this);
    connect(m_header, &MapHeader::modified, this, &Map::modified);
}

Map::Map(const Map &other, QObject *parent) : Map(parent) {
    m_name = other.m_name;
    m_constantName = other.m_constantName;
    m_layoutId = other.m_layoutId;
    m_sharedEventsMap = other.m_sharedEventsMap;
    m_sharedScriptsMap = other.m_sharedScriptsMap;
    m_customAttributes = other.m_customAttributes;
    *m_header = *other.m_header;
    m_layout = other.m_layout;
    m_isPersistedToFile = false;
    m_metatileLayerOrder = other.m_metatileLayerOrder;
    m_metatileLayerOpacity = other.m_metatileLayerOpacity;

    // Copy events
    for (auto i = other.m_events.constBegin(); i != other.m_events.constEnd(); i++) {
        QList<Event*> newEvents;
        for (const auto &event : i.value()) {
            auto newEvent = event->duplicate();
            m_ownedEvents.insert(newEvent);
            newEvents.append(newEvent);
        }
        m_events[i.key()] = newEvents;
    }

    // Duplicating the map connections is probably not desirable, so we skip them.
}

Map::~Map() {
    qDeleteAll(m_ownedEvents);
    m_ownedEvents.clear();
    deleteConnections();
}

// Note: Map does not take ownership of layout
void Map::setLayout(Layout *layout) {
    m_layout = layout;
    if (layout) {
        m_layoutId = layout->id;
    }
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

int Map::getWidth() const {
    return m_layout->getWidth();
}

int Map::getHeight() const {
    return m_layout->getHeight();
}

int Map::getBorderWidth() const {
    return m_layout->getBorderWidth();
}

int Map::getBorderHeight() const {
    return m_layout->getBorderHeight();
}

// Get the portion of the map that can be rendered when rendered as a map connection.
// Cardinal connections render the nearest segment of their map and within the bounds of the border draw distance,
// Dive/Emerge connections are rendered normally within the bounds of their parent map.
QRect Map::getConnectionRect(const QString &direction, Layout * fromLayout) {
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

QPixmap Map::renderConnection(const QString &direction, Layout * fromLayout) {
    QRect bounds = getConnectionRect(direction, fromLayout);
    if (!bounds.isValid())
        return QPixmap();

    // 'fromLayout' will be used in 'render' to get the palettes from the parent map.
    // Dive/Emerge connections render normally with their own palettes, so we ignore this.
    if (MapConnection::isDiving(direction))
        fromLayout = nullptr;

    QPixmap connectionPixmap = m_layout->render(true, fromLayout, bounds);
    return connectionPixmap.copy(bounds.x() * 16, bounds.y() * 16, bounds.width() * 16, bounds.height() * 16);
}

void Map::openScript(QString label) {
    emit openScriptRequested(label);
}

QStringList Map::getScriptLabels(Event::Group group) {
    if (!m_scriptsLoaded) {
        m_scriptsFileLabels = ParseUtil::getGlobalScriptLabels(getScriptsFilePath());
        m_scriptsLoaded = true;
    }

    QStringList scriptLabels;

    // Get script labels currently in-use by the map's events
    if (group == Event::Group::None) {
        ScriptTracker scriptTracker;
        for (const auto &event : getEvents()) {
            event->accept(&scriptTracker);
        }
        scriptLabels = scriptTracker.getScripts();
    } else {
        ScriptTracker scriptTracker;
        for (const auto &event : m_events.value(group)) {
            event->accept(&scriptTracker);
        }
        scriptLabels = scriptTracker.getScripts();
    }

    // Add scripts from map's scripts file, and empty names.
    scriptLabels.append(m_scriptsFileLabels);
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
                                        .arg(m_name));
    auto extension = Project::getScriptFileExtension(usePoryscript);
    if (usePoryscript && !QFile::exists(path + extension))
        extension = Project::getScriptFileExtension(false);
    path += extension;
    return path;
}

void Map::resetEvents() {
    m_events[Event::Group::Object].clear();
    m_events[Event::Group::Warp].clear();
    m_events[Event::Group::Coord].clear();
    m_events[Event::Group::Bg].clear();
    m_events[Event::Group::Heal].clear();
}

QList<Event *> Map::getEvents(Event::Group group) const {
    if (group == Event::Group::None) {
        // Get all events
        QList<Event *> all_events;
        for (const auto &event_list : m_events) {
            all_events << event_list;
        }
        return all_events;
    }
    return m_events[group];
}

Event* Map::getEvent(Event::Group group, int index) const {
    return m_events[group].value(index, nullptr);
}

int Map::getNumEvents(Event::Group group) const {
    if (group == Event::Group::None) {
        // Total number of events
        int numEvents = 0;
        for (auto i = m_events.constBegin(); i != m_events.constEnd(); i++) {
            numEvents += i.value().length();
        }
        return numEvents;
    }
    return m_events[group].length();
}

void Map::removeEvent(Event *event) {
    for (auto i = m_events.begin(); i != m_events.end(); i++) {
        i.value().removeAll(event);
    }
}

void Map::addEvent(Event *event) {
    event->setMap(this);
    m_events[event->getEventGroup()].append(event);
    if (!m_ownedEvents.contains(event)) m_ownedEvents.insert(event);
}

int Map::getIndexOfEvent(const Event *event) const {
    return m_events.value(event->getEventGroup()).indexOf(event);
}

void Map::deleteConnections() {
    qDeleteAll(m_ownedConnections);
    m_ownedConnections.clear();
    m_connections.clear();
}

QList<MapConnection*> Map::getConnections() const {
    return m_connections;
}

void Map::addConnection(MapConnection *connection) {
    if (!connection || m_connections.contains(connection))
        return;

    // Maps should only have one Dive/Emerge connection at a time.
    // (Users can technically have more by editing their data manually, but we will only display one at a time)
    // Any additional connections being added (this can happen via mirroring) are tracked for deleting but otherwise ignored.
    if (MapConnection::isDiving(connection->direction())) {
        for (const auto &i : m_connections) {
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

    if (!m_connections.contains(connection))
        m_connections.append(connection);

    trackConnection(connection);
}

void Map::trackConnection(MapConnection *connection) {
    connection->setParentMap(this, false);

    if (!m_ownedConnections.contains(connection)) {
        m_ownedConnections.insert(connection);
        connect(connection, &MapConnection::parentMapChanged, [=](Map *, Map *after) {
            if (after != this && after != nullptr) {
                // MapConnection's parent has been reassigned, it's no longer our responsibility
                m_ownedConnections.remove(connection);
                QObject::disconnect(connection, &MapConnection::parentMapChanged, this, nullptr);
            }
        });
    }
}

// We retain ownership of this MapConnection until it's assigned to a new parent map.
void Map::removeConnection(MapConnection *connection) {
    if (!m_connections.removeOne(connection))
        return;
    connection->setParentMap(nullptr, false);
    modify();
    emit connectionRemoved(connection);
}

void Map::commit(QUndoCommand *cmd) {
    m_editHistory->push(cmd);
}

void Map::modify() {
    emit modified();
}

void Map::setClean() {
    m_editHistory->setClean();
    m_hasUnsavedDataChanges = false;
    m_isPersistedToFile = true;
}

bool Map::hasUnsavedChanges() const {
    return !m_editHistory->isClean() || m_layout->hasUnsavedChanges() || m_hasUnsavedDataChanges || !m_isPersistedToFile;
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
    for (int i = 0; i < m_editHistory->count(); i++) {
        // Qt really doesn't expect editing commands in the stack to be valid (fair).
        // A better future design might be to have separate edit histories per map tab,
        // and dumping the entire Connections tab history with QUndoStack::clear.
        auto command = const_cast<QUndoCommand*>(m_editHistory->command(i));
        if (mapConnectionIds.contains(command->id()))
            command->setObsolete(true);
    }
}
