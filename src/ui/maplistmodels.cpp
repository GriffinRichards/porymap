#include "maplistmodels.h"

#include <QMouseEvent>
#include <QLineEdit>

#include "project.h"
#include "filterchildrenproxymodel.h"



void MapTree::removeSelected() {
    while (!this->selectedIndexes().isEmpty()) {
        QModelIndex i = this->selectedIndexes().takeLast();
        this->model()->removeRow(i.row(), i.parent());
    }
}

void MapTree::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Delete selected items in the tree
        auto selectionModel = this->selectionModel();
        if (!selectionModel->hasSelection())
            return;

        auto model = static_cast<FilterChildrenProxyModel*>(this->model());
        auto sourceModel = static_cast<MapListModel*>(model->sourceModel());

        QModelIndexList selectedIndexes = selectionModel->selectedRows();
        QList<QPersistentModelIndex> persistentIndexes;
        for (const auto &index : selectedIndexes) {
            persistentIndexes.append(model->mapToSource(index));
        }
        for (const auto &index : persistentIndexes) {
            sourceModel->removeItemAt(index);
        }
    } else {
        QWidget::keyPressEvent(event);
    }
}



MapListModel::MapListModel(Project *project, QObject *parent) : QStandardItemModel(parent) {
    this->project = project;
    this->root = invisibleRootItem();

    this->mapGrayIcon = QIcon(QStringLiteral(":/icons/map_grayed.ico"));
    this->mapIcon = QIcon(QStringLiteral(":/icons/map.ico"));
    this->mapEditedIcon = QIcon(QStringLiteral(":/icons/map_edited.ico"));
    this->mapOpenedIcon = QIcon(QStringLiteral(":/icons/map_opened.ico"));

    this->mapFolderIcon.addFile(QStringLiteral(":/icons/folder_closed_map.ico"), QSize(), QIcon::Normal, QIcon::Off);
    this->mapFolderIcon.addFile(QStringLiteral(":/icons/folder_map.ico"), QSize(), QIcon::Normal, QIcon::On);

    this->emptyMapFolderIcon.addFile(QStringLiteral(":/icons/folder_closed.ico"), QSize(), QIcon::Normal, QIcon::Off);
    this->emptyMapFolderIcon.addFile(QStringLiteral(":/icons/folder.ico"), QSize(), QIcon::Normal, QIcon::On);
}

QStandardItem *MapListModel::getItem(const QModelIndex &index) const {
    if (index.isValid()) {
        QStandardItem *item = static_cast<QStandardItem*>(index.internalPointer());
        if (item)
            return item;
    }
    return this->root;
}

QModelIndex MapListModel::indexOf(const QString &itemName) const {
    if (this->mapItems.contains(itemName))
        return this->mapItems.value(itemName)->index();

    if (this->mapFolderItems.contains(itemName))
        return this->mapFolderItems.value(itemName)->index();

    return QModelIndex();
}

void MapListModel::removeItemAt(const QModelIndex &index) {
    QStandardItem *item = this->getItem(index)->child(index.row(), index.column());
    if (!item)
        return;

    const QString type = item->data(MapListUserRoles::TypeRole).toString();
    if (type == "map_name") {
        // TODO: No support for deleting maps
    } else {
        // TODO: Because there's no support for deleting maps we can only delete empty folders
        if (!item->hasChildren()) {
            removeItem(item);
        }
    }
}

QStandardItem *MapListModel::createMapItem(const QString &mapName, QStandardItem *map) {
    if (!map) map = new QStandardItem;
    map->setText(mapName);
    map->setData(mapName, MapListUserRoles::NameRole);
    map->setData("map_name", MapListUserRoles::TypeRole);
    map->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren);
    map->setEditable(this->editable); // Will override flags if necessary
    this->mapItems.insert(mapName, map);
    return map;
}

QStandardItem *MapListModel::createMapFolderItem(const QString &folderName, QStandardItem *folder) {
    if (!folder) folder = new QStandardItem;
    folder->setText(folderName);
    folder->setData(folderName, MapListUserRoles::NameRole);
    folder->setData(this->folderTypeName, MapListUserRoles::TypeRole);
    folder->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    folder->setEditable(this->editable); // Will override flags if necessary
    this->mapFolderItems.insert(folderName, folder);
    return folder;
}

QStandardItem *MapListModel::insertMapItem(const QString &mapName, const QString &folderName) {
    // Disallow adding MAP_DYNAMIC to the map list.
    if (mapName == this->project->getDynamicMapName())
        return nullptr;

    QStandardItem *folder = this->mapFolderItems[folderName];
    if (!folder) folder = insertMapFolderItem(folderName);

    QStandardItem *map = createMapItem(mapName);
    folder->appendRow(map);
    if (this->sortingEnabled)
        this->sort(0, Qt::AscendingOrder);
    return map;
}

QStandardItem *MapListModel::insertMapFolderItem(const QString &folderName) {
    QStandardItem *item = createMapFolderItem(folderName);
    this->root->appendRow(item);
    if (this->sortingEnabled)
        this->sort(0, Qt::AscendingOrder);
    return item;
}

QVariant MapListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int col = index.column();

    const QStandardItem *item = this->getItem(index)->child(row, col);
    const QString type = item->data(MapListUserRoles::TypeRole).toString();
    const QString name = item->data(MapListUserRoles::NameRole).toString();

    if (type == "map_name") {
        // Data for maps in the map list
        if (role == Qt::DecorationRole) {
            if (name == this->activeItemName)
                return this->mapOpenedIcon;

            const Map* map = this->project->mapCache.value(name);
            if (!map)
                return this->mapGrayIcon;
            return map->hasUnsavedChanges() ? this->mapEditedIcon : this->mapIcon; 
        } else if (role == Qt::ToolTipRole) {
            return this->project->mapNamesToMapConstants.value(name);
        }
    } else if (type == this->folderTypeName) {
        // Data for map folders in the map list
        if (role == Qt::DecorationRole) {
            return item->hasChildren() ? this->mapFolderIcon : this->emptyMapFolderIcon;
        }
    }
    return QStandardItemModel::data(index, role);
}



QWidget *GroupNameDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const {
    QLineEdit *editor = new QLineEdit(parent);
    static const QRegularExpression expression("[A-Za-z_]+[\\w]*");
    editor->setPlaceholderText("gMapGroup_");
    editor->setValidator(new QRegularExpressionValidator(expression, parent));
    editor->setFrame(false);
    return editor;
}

void GroupNameDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    QString groupName = index.data(MapListUserRoles::NameRole).toString();
    QLineEdit *le = static_cast<QLineEdit *>(editor);
    le->setText(groupName);
}

void GroupNameDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    QLineEdit *le = static_cast<QLineEdit *>(editor);
    QString groupName = le->text();
    model->setData(index, groupName, MapListUserRoles::NameRole);
}

void GroupNameDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const {
    editor->setGeometry(option.rect);
}



MapGroupModel::MapGroupModel(Project *project, QObject *parent) : MapListModel(project, parent) {
    this->folderTypeName = "map_group";
    this->editable = true;

    for (const auto &groupName : this->project->groupNames) {
        for (const auto &mapName : this->project->groupNameToMapNames.value(groupName)) {
            insertMapItem(mapName, groupName);
        }
    }
}

Qt::DropActions MapGroupModel::supportedDropActions() const {
    return Qt::MoveAction;
}

QStringList MapGroupModel::mimeTypes() const {
    QStringList types;
    types << "application/porymap.mapgroupmodel.map"
          << "application/porymap.mapgroupmodel.group"
          << "application/porymap.mapgroupmodel.source.row"
          << "application/porymap.mapgroupmodel.source.column";
    return types;
}

QMimeData *MapGroupModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = QStandardItemModel::mimeData(indexes);
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    // if dropping a selection containing a group(s) and map(s), clear all selection but first group.
    for (const QModelIndex &index : indexes) {
        if (index.isValid() && data(index, MapListUserRoles::TypeRole).toString() == "map_group") {
            QString groupName = data(index, MapListUserRoles::NameRole).toString();
            stream << groupName;
            mimeData->setData("application/porymap.mapgroupmodel.group", encodedData);
            mimeData->setData("application/porymap.mapgroupmodel.source.row", QByteArray::number(index.row()));
            return mimeData;
        }
    }

    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            QString mapName = data(index, MapListUserRoles::NameRole).toString();
            stream << mapName;
        }
    }

    mimeData->setData("application/porymap.mapgroupmodel.map", encodedData);
    return mimeData;
}

bool MapGroupModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int, const QModelIndex &parentIndex) {
    if (action == Qt::IgnoreAction)
        return true;

    if (!parentIndex.isValid() && !data->hasFormat("application/porymap.mapgroupmodel.group"))
        return false;

    int firstRow = 0;
 
    if (row != -1) {
        firstRow = row;
    }
    else if (parentIndex.isValid()) {
        firstRow = rowCount(parentIndex);
    }

    if (data->hasFormat("application/porymap.mapgroupmodel.group")) {
        if (parentIndex.row() != -1 || parentIndex.column() != -1) {
            return false;
        }
        QByteArray encodedData = data->data("application/porymap.mapgroupmodel.group");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QString groupName;

        while (!stream.atEnd()) {
            stream >> groupName;
        }

        this->insertRow(row, parentIndex);

        // copy children to new node
        int sourceRow = data->data("application/porymap.mapgroupmodel.source.row").toInt();
        QModelIndex originIndex = this->index(sourceRow, 0);
        QModelIndexList children;
        QStringList mapsToMove;
        for (int i = 0; i < this->rowCount(originIndex); ++i ) {
            children << this->index( i, 0, originIndex);
            mapsToMove << this->index( i, 0 , originIndex).data(MapListUserRoles::NameRole).toString();
        }

        QModelIndex groupIndex = index(row, 0, parentIndex);
        QStandardItem *groupItem = this->itemFromIndex(groupIndex);
        createMapFolderItem(groupName, groupItem);

        for (QString mapName : mapsToMove) {
            QStandardItem *mapItem = createMapItem(mapName);
            groupItem->appendRow(mapItem);
        }
    }
    else if (data->hasFormat("application/porymap.mapgroupmodel.map")) {
        QByteArray encodedData = data->data("application/porymap.mapgroupmodel.map");
        QDataStream stream(&encodedData, QIODevice::ReadOnly);
        QStringList droppedMaps;
        int rowCount = 0;

        while (!stream.atEnd()) {
            QString mapName;
            stream >> mapName;
            droppedMaps << mapName;
            rowCount++;
        }

        QStandardItem *groupItem = this->itemFromIndex(parentIndex);
        if (groupItem->hasChildren()) {
            this->insertRows(firstRow, rowCount, parentIndex);
            for (QString mapName : droppedMaps) {
                QModelIndex mapIndex = index(firstRow, 0, parentIndex);
                QStandardItem *mapItem = this->itemFromIndex(mapIndex);
                createMapItem(mapName, mapItem);
                firstRow++;
            }
        }
        // for whatever reason insertRows doesn't work as I expected with childless items
        // so just append all the new maps instead
        else {
            for (QString mapName : droppedMaps) {
                QStandardItem *mapItem = createMapItem(mapName);
                groupItem->appendRow(mapItem);
                firstRow++;
            }
        }

    }

    emit dragMoveCompleted();
    updateProject();

    return true;
}

void MapGroupModel::updateProject() {
    if (!this->project) return;

    // Temporary objects in case of failure, so we won't modify the project unless it succeeds.
    QStringList mapNames;
    QStringList groupNames;
    QMap<QString, QStringList> groupNameToMapNames;

    for (int g = 0; g < this->root->rowCount(); g++) {
        const QStandardItem *groupItem = this->item(g);
        QString groupName = groupItem->data(MapListUserRoles::NameRole).toString();
        groupNames.append(groupName);
        for (int m = 0; m < groupItem->rowCount(); m++) {
            const QStandardItem *mapItem = groupItem->child(m);
            if (!mapItem) {
                logError("An error occured while trying to apply updates to map group structure.");
                return;
            }
            QString mapName = mapItem->data(MapListUserRoles::NameRole).toString();
            groupNameToMapNames[groupName].append(mapName);
            mapNames.append(mapName);
        }
    }

    this->project->mapNames = mapNames;
    this->project->groupNames = groupNames;
    this->project->groupNameToMapNames = groupNameToMapNames;
    this->project->hasUnsavedDataChanges = true;
}

void MapGroupModel::removeItem(QStandardItem *item) {
    this->removeRow(item->row());
    this->updateProject();
}

QVariant MapGroupModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int col = index.column();

    const QStandardItem *item = this->getItem(index)->child(row, col);
    const QString type = item->data(MapListUserRoles::TypeRole).toString();
    const QString name = item->data(MapListUserRoles::NameRole).toString();

    if (role == Qt::DisplayRole) {
        if (type == "map_name") {
            return QString("[%1.%2] ").arg(this->getItem(index)->row()).arg(row, 2, 10, QLatin1Char('0')) + name;
        }
        else if (type == this->folderTypeName) {
            return name;
        }
    }
    return MapListModel::data(index, role);
}

bool MapGroupModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (role == MapListUserRoles::NameRole && data(index, MapListUserRoles::TypeRole).toString() == "map_group") {
        if (!this->project->isIdentifierUnique(value.toString())) {
            return false;
        }
    }
    if (QStandardItemModel::setData(index, value, role)) {
        this->updateProject();
    }
    return true;
}



MapAreaModel::MapAreaModel(Project *project, QObject *parent) : MapListModel(project, parent) {
    this->folderTypeName = "map_section";

    for (const auto &mapName : this->project->mapNames) {
        insertMapItem(mapName, this->project->mapNameToMapSectionName.value(mapName));
    }

    this->sortingEnabled = true;
    sort(0, Qt::AscendingOrder);
}

void MapAreaModel::removeItem(QStandardItem *item) {
    this->project->removeMapsec(item->data(MapListUserRoles::NameRole).toString());
    this->removeRow(item->row());
}



LayoutTreeModel::LayoutTreeModel(Project *project, QObject *parent) : MapListModel(project, parent) {
    this->folderTypeName = "map_layout";

    for (const auto &mapName : this->project->mapNames) {
        insertMapItem(mapName, this->project->mapNameToLayoutId.value(mapName));
    }

    this->sortingEnabled = true;
    sort(0, Qt::AscendingOrder);
}

void LayoutTreeModel::removeItem(QStandardItem *) {
    // TODO: Deleting layouts not supported
}

QVariant LayoutTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    int col = index.column();

    const QStandardItem *item = this->getItem(index)->child(row, col);
    const QString type = item->data(MapListUserRoles::TypeRole).toString();
    const QString name = item->data(MapListUserRoles::NameRole).toString();

    if (type == this->folderTypeName) {
        const Layout* layout = this->project->mapLayouts.value(name);

        if (role == Qt::DecorationRole) {
            // Map layouts are used as folders, but we display them with the same icons as maps.
            if (name == this->activeItemName)
                return this->mapOpenedIcon;

            if (!layout || !layout->loaded)
                return this->mapGrayIcon;
            return layout->hasUnsavedChanges() ? this->mapEditedIcon : this->mapIcon;
        }
        else if (role == Qt::DisplayRole) {
            // Despite using layout IDs internally, the Layouts map list shows layouts using their file path name.
            if (layout) return layout->name;
        } else if (role == Qt::ToolTipRole) {
            if (layout) return layout->id;
        }
    }
    return MapListModel::data(index, role);
}