#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QMessageBox>

#include "movablerect.h"
#include "utility.h"

MovableRect::MovableRect(const QRectF &rect, const QSize &cellSize, const QRgb &color)
  : QGraphicsRectItem(rect),
    baseRect(rect),
    cellSize(cellSize),
    color(color)
{ }

/// Center rect on grid position (x, y)
void MovableRect::updateLocation(int x, int y) {
    setRect(this->baseRect.x() + (x * this->cellSize.width()),
            this->baseRect.y() + (y * this->cellSize.height()),
            this->baseRect.width(),
            this->baseRect.height());
}


/******************************************************************************
    ************************************************************************
 ******************************************************************************/

ResizableRect::ResizableRect(QObject *parent, const QSize &cellSize, const QSize &size, const QRgb &color)
  : QObject(parent),
    MovableRect(QRect(0, 0, size.width(), size.height()), cellSize, color)
{
    setAcceptHoverEvents(true);
    setFlags(this->flags() | QGraphicsItem::ItemIsMovable);
}

ResizableRect::Edge ResizableRect::detectEdge(int x, int y) {
    QRectF edge = this->boundingRect();
    if (x <= edge.left() + this->lineWidth) {
        if (y >= edge.top() + 2 * this->lineWidth) {
            if (y <= edge.bottom() - 2 * this->lineWidth) {
                return ResizableRect::Edge::Left;
            }
            else {
                return ResizableRect::Edge::BottomLeft;
            }
        }
        else {
            return ResizableRect::Edge::TopLeft;
        }
    }
    else if (x >= edge.right() - this->lineWidth) {
        if (y >= edge.top() + 2 * this->lineWidth) {
            if (y <= edge.bottom() - 2 * this->lineWidth) {
                return ResizableRect::Edge::Right;
            }
            else {
                return ResizableRect::Edge::BottomRight;
            }
        }
        else {
            return ResizableRect::Edge::TopRight;
        }
    }
    else {
        if (y <= edge.top() + this->lineWidth) {
            return ResizableRect::Edge::Top;
        }
        else if (y >= edge.bottom() - this->lineWidth) {
            return ResizableRect::Edge::Bottom;
        }
    }
    return ResizableRect::Edge::None;
}

void ResizableRect::updatePosFromRect(QRect newRect) {
    prepareGeometryChange();
    this->setRect(newRect);
    emit this->rectUpdated(newRect);
}

void ResizableRect::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    switch (this->detectEdge(event->pos().x(), event->pos().y())) {
    case ResizableRect::Edge::None:
    default:
        break;
    case ResizableRect::Edge::Left:
    case ResizableRect::Edge::Right:
        this->setCursor(Qt::SizeHorCursor);
        break;
    case ResizableRect::Edge::Top:
    case ResizableRect::Edge::Bottom:
        this->setCursor(Qt::SizeVerCursor);
        break;
    case ResizableRect::Edge::TopRight:
    case ResizableRect::Edge::BottomLeft:
        this->setCursor(Qt::SizeBDiagCursor);
        break;
    case ResizableRect::Edge::TopLeft:
    case ResizableRect::Edge::BottomRight:
        this->setCursor(Qt::SizeFDiagCursor);
        break;
    }
}

void ResizableRect::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    this->unsetCursor();
}

void ResizableRect::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    int x = event->pos().x();
    int y = event->pos().y();
    this->clickedPos = event->scenePos();
    this->clickedRect = this->rect().toAlignedRect();
    this->clickedEdge = this->detectEdge(x, y);
}

void ResizableRect::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    int dx = Util::roundUpToMultiple(event->scenePos().x() - this->clickedPos.x(), this->cellSize.width());
    int dy = Util::roundUpToMultiple(event->scenePos().y() - this->clickedPos.y(), this->cellSize.height());

    QRect resizedRect = this->clickedRect;

    switch (this->clickedEdge) {
    case ResizableRect::Edge::None:
    default:
        return;
    case ResizableRect::Edge::Left:
        resizedRect.adjust(dx, 0, 0, 0);
        break;
    case ResizableRect::Edge::Right:
        resizedRect.adjust(0, 0, dx, 0);
        break;
    case ResizableRect::Edge::Top:
        resizedRect.adjust(0, dy, 0, 0);
        break;
    case ResizableRect::Edge::Bottom:
        resizedRect.adjust(0, 0, 0, dy);
        break;
    case ResizableRect::Edge::TopRight:
        resizedRect.adjust(0, dy, dx, 0);
        break;
    case ResizableRect::Edge::BottomLeft:
        resizedRect.adjust(dx, 0, 0, dy);
        break;
    case ResizableRect::Edge::TopLeft:
        resizedRect.adjust(dx, dy, 0, 0);
        break;
    case ResizableRect::Edge::BottomRight:
        resizedRect.adjust(0, 0, dx, dy);
        break;
    }

    // Lower limits: smallest possible size is 1 cell
    if (resizedRect.width() < this->cellSize.width()) {
        if (dx < 0) { // right sided adjustment made
            resizedRect.setWidth(this->cellSize.width());
        } else { // left sided adjustment slightly more complicated
            int dxMax = this->clickedRect.right() - this->clickedRect.left() - this->cellSize.width();
            resizedRect.adjust(dxMax - dx, 0, 0, 0);
        }
    }
    if (resizedRect.height() < this->cellSize.height()) {
        if (dy < 0) { // bottom
            resizedRect.setHeight(this->cellSize.height());
        } else { // top
            int dyMax = this->clickedRect.bottom() - this->clickedRect.top() - this->cellSize.height();
            resizedRect.adjust(0, dyMax - dy, 0, 0);
        }
    }

    // Upper limits: clip resized to limit rect
    this->updatePosFromRect(resizedRect & this->limit);
}
