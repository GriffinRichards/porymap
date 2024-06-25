#ifndef NEWMAPPOPUP_H
#define NEWMAPPOPUP_H

#include <QMainWindow>
#include <QString>
#include "editor.h"
#include "project.h"
#include "map.h"

namespace Ui {
class NewMapPopup;
}

class NewMapPopup : public QMainWindow
{
    Q_OBJECT
public:
    explicit NewMapPopup(QWidget *parent = nullptr, Project *project = nullptr);
    ~NewMapPopup();
    Map *map;
    int group;
    bool existingLayout;
    bool importedMap;
    QString layoutId;
    void init();
    void init(MapSortOrder type, QVariant data);
    void init(MapLayout *);
    static void setDefaultSettings(Project *project);

signals:
    void applied();

private:
    Ui::NewMapPopup *ui;
    Project *project;
    bool checkNewMapDimensions();
    bool checkNewMapGroup();
    bool checkNewMapName();
    void clearErrorMessage();
    void addErrorMessage(QString message);
    bool hasErrorMessage();
    void saveSettings();
    void useLayout(QString layoutId);
    void useLayoutSettings(MapLayout *mapLayout);

    struct Settings {
        QString group;
        int width;
        int height;
        int borderWidth;
        int borderHeight;
        QString primaryTilesetLabel;
        QString secondaryTilesetLabel;
        QString type;
        QString location;
        QString song;
        bool canFlyTo;
        bool showLocationName;
        bool allowRunning;
        bool allowBiking;
        bool allowEscaping;
        int floorNumber;
    };
    static struct Settings settings;

private slots:
    void on_pushButton_NewMap_Accept_clicked();
};

#endif // NEWMAPPOPUP_H
