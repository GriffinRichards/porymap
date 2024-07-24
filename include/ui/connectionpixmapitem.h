#ifndef CONNECTIONPIXMAPITEM_H
#define CONNECTIONPIXMAPITEM_H

#include "mapconnection.h"
#include <QGraphicsPixmapItem>
#include <QPainter>

class ConnectionPixmapItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT
public:
    ConnectionPixmapItem(QPixmap pixmap, MapConnection* connection, int x, int y)
    : QGraphicsPixmapItem(pixmap)
    {
        this->basePixmap = pixmap;
        this->connection = connection;
        setFlag(ItemIsMovable);
        setFlag(ItemSendsGeometryChanges);
        this->initialX = x;
        this->initialY = y;
        this->initialOffset = connection->offset;
        this->setX(x);
        this->setY(y);
    }
    QPixmap basePixmap;
    MapConnection* connection;
    int initialX;
    int initialY;
    int initialOffset;

    void setEditable(bool editable);
    bool getEditable();
    void setSelected(bool selected);
    void render();

private:
    bool selected = false;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value);
    void mousePressEvent(QGraphicsSceneMouseEvent*);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent*);

signals:
    void connectionItemDoubleClicked(ConnectionPixmapItem* connectionItem);
    void connectionMoved(MapConnection *, int newOffset);
    void selectionChanged(bool selected);
};

#endif // CONNECTIONPIXMAPITEM_H
