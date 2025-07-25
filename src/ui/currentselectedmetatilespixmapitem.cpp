#include "currentselectedmetatilespixmapitem.h"
#include "imageproviders.h"
#include <QPainter>

QPixmap drawMetatileSelection(MetatileSelection selection, Layout *layout) {
    int width = selection.dimensions.x() * Metatile::pixelWidth();
    int height = selection.dimensions.y() * Metatile::pixelHeight();
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(QColor(0, 0, 0, 0));
    QPainter painter(&image);

    for (int i = 0; i < selection.dimensions.x(); i++) {
        for (int j = 0; j < selection.dimensions.y(); j++) {
            int x = i * Metatile::pixelWidth();
            int y = j * Metatile::pixelHeight();
            QPoint metatile_origin = QPoint(x, y);
            int index = j * selection.dimensions.x() + i;
            MetatileSelectionItem item = selection.metatileItems.at(index);
            if (item.enabled) {
                QImage metatile_image = getMetatileImage(item.metatileId, layout);
                painter.drawImage(metatile_origin, metatile_image);
            }
        }
    }

    painter.end();
    return QPixmap::fromImage(image);
}

void CurrentSelectedMetatilesPixmapItem::draw() {
    MetatileSelection selection = metatileSelector->getMetatileSelection();
    setPixmap(drawMetatileSelection(selection, this->layout));
}
