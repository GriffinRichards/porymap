#include "config.h"
#include "imageproviders.h"
#include "log.h"
#include <QPainter>

QImage getCollisionMetatileImage(Block block) {
    return getCollisionMetatileImage(block.collision, block.elevation);
}

QImage getCollisionMetatileImage(int collision, int elevation) {
    static const QImage collisionImage(":/images/collisions.png");
    int x = (collision != 0) * 16;
    int y = elevation * 16;
    return collisionImage.copy(x, y, 16, 16);
}

QImage getMetatileImage(
        uint16_t metatileId,
        Tileset *primaryTileset,
        Tileset *secondaryTileset,
        QList<int> layerOrder,
        QList<float> layerOpacity,
        bool useTruePalettes)
{
    Metatile* metatile = Tileset::getMetatile(metatileId, primaryTileset, secondaryTileset);
    return getMetatileImage(metatile, primaryTileset, secondaryTileset, layerOrder, layerOpacity, useTruePalettes);
}

QImage getMetatileImage(
        Metatile *metatile,
        Tileset *primaryTileset,
        Tileset *secondaryTileset,
        QList<int> layerOrder,
        QList<float> layerOpacity,
        bool useTruePalettes)
{
    QImage metatile_image(16, 16, QImage::Format_RGBA8888);
    if (!metatile) {
        metatile_image.fill(Qt::magenta);
        return metatile_image;
    }
    metatile_image.fill(Qt::black);

    QPainter painter(&metatile_image);
    const int numLayers = 3; // When rendering, metatiles always have 3 layers
    for (int i = 0; i < numLayers; i++){
        int layer = layerOrder.size() >= numLayers ? layerOrder[i] : i;
        int bottomLayer = layerOrder.size() >= numLayers ? layerOrder[0] : 0;
        float opacity = layerOpacity.size() >= numLayers ? layerOpacity[layer] : 1.0;
        bool allowTransparency = layer != bottomLayer;

        QImage tileImage = getMetatileLayerImage(metatile, layer, primaryTileset, secondaryTileset, opacity, allowTransparency, useTruePalettes);
        painter.drawImage(QPoint(0, 0), tileImage);
    }
    painter.end();

    return metatile_image;
}

QImage getMetatileLayerImage(
    Metatile *metatile,
    int layer,
    Tileset *primaryTileset,
    Tileset *secondaryTileset,
    qreal opacity,
    bool allowTransparency,
    bool useTruePalettes) {

    QImage layerImage(16, 16, QImage::Format_RGBA8888);
    if (!metatile) {
        layerImage.fill(Qt::magenta);
        return layerImage;
    }
    layerImage.fill(allowTransparency ? Qt::transparent : Qt::black);

    QPainter painter(&layerImage);
    int layerType = metatile->layerType;
    bool isTripleLayerMetatile = projectConfig.getTripleLayerMetatilesEnabled();
    for (int y = 0; y < 2; y++)
    for (int x = 0; x < 2; x++) {
        // Get the tile to render next
        Tile tile;
        int tileOffset = (y * 2) + x;
        if (isTripleLayerMetatile) {
            tile = metatile->tiles.value(tileOffset + (layer * 4));
        } else {
            // "Vanilla" metatiles only have 8 tiles, but render 12.
            // The remaining 4 tiles are rendered either as tile 0 or 0x3014 (tile 20, palette 3) depending on layer type.
            switch (layerType)
            {
            default:
            case METATILE_LAYER_MIDDLE_TOP:
                if (layer == 0)
                    tile = Tile(0x3014);
                else // Tiles are on layers 1 and 2
                    tile = metatile->tiles.value(tileOffset + ((layer - 1) * 4));
                break;
            case METATILE_LAYER_BOTTOM_MIDDLE:
                if (layer == 2)
                    tile = Tile();
                else // Tiles are on layers 0 and 1
                    tile = metatile->tiles.value(tileOffset + (layer * 4));
                break;
            case METATILE_LAYER_BOTTOM_TOP:
                if (layer == 1)
                    tile = Tile();
                else // Tiles are on layers 0 and 2
                    tile = metatile->tiles.value(tileOffset + ((layer == 0 ? 0 : 1) * 4));
                break;
            }
        }

        QPoint origin = QPoint(x*8, y*8);
        QImage tileImage = getPalettedTileImage(tile.tileId, primaryTileset, secondaryTileset, tile.palette, useTruePalettes);

        if (tileImage.isNull()) {
            // Some metatiles specify tiles that are outside the valid range.
            // These are treated as completely transparent, so they can be skipped without
            // being drawn unless they're on the bottom layer, in which case we need
            // a placeholder because garbage will be drawn otherwise.
            if (!allowTransparency) {
                QList<QList<QRgb>> palettes = useTruePalettes ? primaryTileset->palettes : primaryTileset->palettePreviews;
                painter.fillRect(origin.x(), origin.y(), 8, 8, palettes.value(0).value(0));
            }
            continue;
        }

        // Set opacity
        if (opacity < 1.0) {
            int alpha = 255 * opacity;
            for (int c = 0; c < tileImage.colorCount(); c++) {
                QColor color(tileImage.color(c));
                color.setAlpha(alpha);
                tileImage.setColor(c, color.rgba());
            }
        }

        // Set color 0 in the palette to transparent
        if (allowTransparency) {
            QColor color(tileImage.color(0));
            color.setAlpha(0);
            tileImage.setColor(0, color.rgba());
        }

        painter.drawImage(origin, tileImage.mirrored(tile.xflip, tile.yflip));
    }
    return layerImage;
}

QImage getTileImage(uint16_t tileId, Tileset *primaryTileset, Tileset *secondaryTileset) {
    Tileset *tileset = Tileset::getTileTileset(tileId, primaryTileset, secondaryTileset);
    int index = Tile::getIndexInTileset(tileId);
    if (!tileset) {
        return QImage();
    }
    return tileset->tiles.value(index, QImage());
}

QImage getColoredTileImage(uint16_t tileId, Tileset *primaryTileset, Tileset *secondaryTileset, QList<QRgb> palette) {
    QImage tileImage = getTileImage(tileId, primaryTileset, secondaryTileset);
    if (tileImage.isNull()) {
        tileImage = QImage(8, 8, QImage::Format_RGBA8888);
        QPainter painter(&tileImage);
        painter.fillRect(0, 0, 8, 8, palette.at(0));
    } else {
        for (int i = 0; i < 16; i++) {
            tileImage.setColor(i, palette.at(i));
        }
    }

    return tileImage;
}

QImage getPalettedTileImage(uint16_t tileId, Tileset *primaryTileset, Tileset *secondaryTileset, int paletteId, bool useTruePalettes) {
    QList<QRgb> palette = Tileset::getPalette(paletteId, primaryTileset, secondaryTileset, useTruePalettes);
    return getColoredTileImage(tileId, primaryTileset, secondaryTileset, palette);
}

QImage getGreyscaleTileImage(uint16_t tileId, Tileset *primaryTileset, Tileset *secondaryTileset) {
    return getColoredTileImage(tileId, primaryTileset, secondaryTileset, greyscalePalette);
}
