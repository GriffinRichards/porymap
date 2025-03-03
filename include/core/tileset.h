#pragma once
#ifndef TILESET_H
#define TILESET_H

#include "metatile.h"
#include "tile.h"
#include <QImage>
#include <QHash>

struct MetatileLabelPair {
    QString owned;
    QString shared;
};

class Tileset
{
public:
    Tileset() = default;
    Tileset(const Tileset &other);
    Tileset &operator=(const Tileset &other);
    ~Tileset();

public:
    void setName(const QString &name);
    QString name() const { return m_name; }
    QString metatileLabelPrefix() const { return m_metatileLabelPrefix; }

    bool is_secondary;
    QString tiles_label;
    QString palettes_label;
    QString metatiles_label;
    QString metatiles_path;
    QString metatile_attrs_label;
    QString metatile_attrs_path;
    QString tilesImagePath;
    QImage tilesImage;
    QStringList palettePaths;
    bool assetsLoaded = false;

    QList<QImage> tiles;
    QHash<int, QString> metatileLabels;
    QList<QList<QRgb>> palettes;
    QList<QList<QRgb>> palettePreviews;

    static Tileset* getMetatileTileset(int, Tileset*, Tileset*);
    static Tileset* getTileTileset(int, Tileset*, Tileset*);
    static Metatile* getMetatile(int, Tileset*, Tileset*);
    static Tileset* getMetatileLabelTileset(int, Tileset*, Tileset*);
    static QString getMetatileLabel(int, Tileset *, Tileset *);
    static QString getOwnedMetatileLabel(int, Tileset *, Tileset *);
    static MetatileLabelPair getMetatileLabelPair(int metatileId, Tileset *primaryTileset, Tileset *secondaryTileset);
    static bool setMetatileLabel(int, QString, Tileset *, Tileset *);
    static QList<QList<QRgb>> getBlockPalettes(Tileset*, Tileset*, bool useTruePalettes = false);
    static QList<QRgb> getPalette(int, Tileset*, Tileset*, bool useTruePalettes = false);
    static bool metatileIsValid(uint16_t metatileId, Tileset *, Tileset *);
    static QHash<int, QString> getHeaderMemberMap();
    static QString getExpectedDir(QString tilesetName, bool isSecondary);
    QString getExpectedDir();

    void load();
    void loadMetatiles();
    void loadMetatileAttributes();
    void loadTilesImage(QImage *importedImage = nullptr);
    void loadPalettes();

    void save();
    void saveMetatileAttributes();
    void saveMetatiles();
    void saveTilesImage();
    void savePalettes();

    bool appendToHeaders(const QString &root, const QString &friendlyName);
    bool appendToGraphics(const QString &root, const QString &friendlyName);
    bool appendToMetatiles(const QString &root, const QString &friendlyName);

    void setTilesImage(const QImage &image);

    void setMetatiles(const QList<Metatile*> &metatiles);
    void addMetatile(Metatile* metatile);

    QList<Metatile*> metatiles() const { return m_metatiles; }
    Metatile* metatileAt(unsigned int i) const { return m_metatiles.at(i); }

    void clearMetatiles();
    void resizeMetatiles(int newNumMetatiles);
    int numMetatiles() const { return m_metatiles.length(); }

private:
    QString m_name;
    QString m_metatileLabelPrefix;
    QList<Metatile*> m_metatiles;
    bool m_hasUnsavedTilesImage = false;
};

#endif // TILESET_H
