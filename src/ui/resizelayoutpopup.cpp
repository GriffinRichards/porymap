#include "resizelayoutpopup.h"
#include "editor.h"
#include "movablerect.h"
#include "config.h"
#include "utility.h"
#include "message.h"

#include "ui_resizelayoutpopup.h"

BoundedPixmapItem::BoundedPixmapItem(const QPixmap &pixmap, const QSize &cellSize, QGraphicsItem *parent) : QGraphicsPixmapItem(pixmap, parent) {
    setFlags(this->flags() | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsSelectable);
    this->cellSize = cellSize;
}

void BoundedPixmapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    // Draw the pixmap darkened in the background
    painter->fillRect(this->boundingRect().toAlignedRect(), QColor(0x444444));
    painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    painter->drawPixmap(this->boundingRect().toAlignedRect(), this->pixmap());

    // draw the normal pixmap on top, cropping to validRect as needed
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    QRect intersection = this->mapRectFromScene(this->boundary->rect()).toAlignedRect() & this->boundingRect().toAlignedRect();
    QPixmap cropped = this->pixmap().copy(intersection);
    painter->drawPixmap(intersection, cropped);
}

QVariant BoundedPixmapItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionChange && scene()) {
        QPointF newPos = value.toPointF();
        return QPointF(Util::roundUpToMultiple(newPos.x(), this->cellSize.width()),
                       Util::roundUpToMultiple(newPos.y(), this->cellSize.height()));
    }
    else
        return QGraphicsItem::itemChange(change, value);
}

/******************************************************************************
    ************************************************************************
 ******************************************************************************/

ResizeLayoutPopup::ResizeLayoutPopup(QWidget *parent, Layout *layout, Project *project) :
    QDialog(parent),
    parent(parent),
    layout(layout),
    project(project),
    ui(new Ui::ResizeLayoutPopup)
{
    ui->setupUi(this);
    this->resetPosition();
    this->setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
    this->setWindowModality(Qt::ApplicationModal);

    this->scene = new CheckeredBgScene(Metatile::pixelSize(), this);
    this->ui->graphicsView->setScene(this->scene);
    this->ui->graphicsView->setRenderHints(QPainter::Antialiasing);
    this->ui->graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

ResizeLayoutPopup::~ResizeLayoutPopup() {
    delete ui;
}

/// Reset position of the dialog to cover the MainWindow's layout metatile scene
void ResizeLayoutPopup::resetPosition() {
    this->setGeometry(QRect(parent->mapToGlobal(QPoint(0, 0)), parent->size()));
}

void ResizeLayoutPopup::on_buttonBox_clicked(QAbstractButton *button) {
    if (button == this->ui->buttonBox->button(QDialogButtonBox::Reset) ) {
        this->scene->clear();
        setupLayoutView();
    }
}

/// Custom scene contains
///   (1) pixmap representing the current layout / not resizable / drag-movable
///   (1) layout outline / resizable / not movable
void ResizeLayoutPopup::setupLayoutView() {
    if (!this->layout || !this->project) return;

    // Border stuff
    if (projectConfig.useCustomBorderSize) {
        this->ui->spinBox_borderWidth->setMinimum(1);
        this->ui->spinBox_borderHeight->setMinimum(1);
        this->ui->spinBox_borderWidth->setMaximum(MAX_BORDER_WIDTH);
        this->ui->spinBox_borderHeight->setMaximum(MAX_BORDER_HEIGHT);
    } else {
        this->ui->frame_border->setVisible(false);
    }
    this->ui->spinBox_borderWidth->setValue(this->layout->getBorderWidth());
    this->ui->spinBox_borderHeight->setValue(this->layout->getBorderHeight());

    // Layout stuff
    this->layoutPixmap = new BoundedPixmapItem(this->layout->pixmap, Metatile::pixelSize());
    this->scene->addItem(layoutPixmap);
    int maxWidth = this->project->getMaxMapWidth();
    int maxHeight = this->project->getMaxMapHeight();
    int maxPixelWidth = maxWidth * Metatile::pixelWidth() * 2; // *2 to allow reaching max dimension by expanding from 0,0 in either direction
    int maxPixelHeight = maxHeight * Metatile::pixelHeight() * 2;
    QGraphicsRectItem *cover = new QGraphicsRectItem(-(maxPixelWidth / 2), -(maxPixelHeight / 2), maxPixelWidth, maxPixelHeight);
    this->scene->addItem(cover);

    this->ui->spinBox_width->setMinimum(1);
    this->ui->spinBox_width->setMaximum(maxWidth);
    this->ui->spinBox_height->setMinimum(1);
    this->ui->spinBox_height->setMaximum(maxHeight);

    this->outline = new ResizableRect(this, Metatile::pixelSize(), this->layout->pixelSize(), qRgb(255, 0, 255));
    this->outline->setZValue(Editor::ZValue::ResizeLayoutPopup); // Ensure on top of view
    this->outline->setLimit(cover->rect().toAlignedRect());
    connect(outline, &ResizableRect::rectUpdated, [=](QRect rect){
        // Note: this extra limit check needs access to the project values, so it is done here and not ResizableRect::mouseMoveEvent
        int metatilesWide = rect.width() / Metatile::pixelWidth();
        int metatilesTall = rect.height() / Metatile::pixelHeight();
        int size = this->project->getMapDataSize(metatilesWide, metatilesTall);
        int maxSize = this->project->getMaxMapDataSize();
        if (size > maxSize) {
            QSize addition = this->project->getMapSizeAddition();
            WarningMessage::show(QStringLiteral("The specified width and height are too large."),
                                 QString("The maximum layout width and height is the following: (width + %1) * (height + %2) <= %3\n"
                                         "The specified layout width and height was: (%4 + %1) * (%5 + %2) = %6")
                                            .arg(addition.width())
                                            .arg(addition.height())
                                            .arg(maxSize)
                                            .arg(metatilesWide)
                                            .arg(metatilesTall)
                                            .arg(size),
                                this);
            // adjust rect to last accepted size
            rect = this->scene->getValidRect();
        }
        this->scene->setValidRect(rect);
        this->outline->setRect(rect);

        const QSignalBlocker b_Width(this->ui->spinBox_width);
        const QSignalBlocker b_Height(this->ui->spinBox_height);
        this->ui->spinBox_width->setValue(metatilesWide);
        this->ui->spinBox_height->setValue(metatilesTall);
    });
    scene->addItem(outline);

    layoutPixmap->setBoundary(outline);
    emit this->outline->rectUpdated(outline->rect().toAlignedRect());

    this->scale = 1.0;

    QRectF rect = this->outline->rect();
    const int marginSize = 10 * Metatile::pixelWidth(); // Leave a margin of 10 metatiles around the map
    rect += QMargins(marginSize, marginSize, marginSize, marginSize);
    this->ui->graphicsView->fitInView(rect, Qt::KeepAspectRatio);
}

void ResizeLayoutPopup::on_spinBox_width_valueChanged(int value) {
    if (!this->outline) return;
    QRectF rect = this->outline->rect();
    this->outline->updatePosFromRect(QRect(rect.x(), rect.y(), value * Metatile::pixelWidth(), rect.height()));
}

void ResizeLayoutPopup::on_spinBox_height_valueChanged(int value) {
    if (!this->outline) return;
    QRectF rect = this->outline->rect();
    this->outline->updatePosFromRect(QRect(rect.x(), rect.y(), rect.width(), value * Metatile::pixelHeight()));
}

/// Result is the number of metatiles to add (or subtract) to each side of the map after dimension changes
QMargins ResizeLayoutPopup::getResult() {
    QMargins result = QMargins();
    result.setLeft((this->layoutPixmap->x() - this->outline->rect().left()) / Metatile::pixelWidth());
    result.setTop((this->layoutPixmap->y() - this->outline->rect().top()) / Metatile::pixelHeight());
    result.setRight((this->outline->rect().right() - (this->layoutPixmap->x() + this->layoutPixmap->pixmap().width())) / Metatile::pixelWidth());
    result.setBottom((this->outline->rect().bottom() - (this->layoutPixmap->y() + this->layoutPixmap->pixmap().height())) / Metatile::pixelHeight());
    return result;
}

QSize ResizeLayoutPopup::getBorderResult() {
    return QSize(this->ui->spinBox_borderWidth->value(), this->ui->spinBox_borderHeight->value());
}

void ResizeLayoutPopup::zoom(qreal delta) {
    if (this->scale + delta < 0.05 || this->scale + delta > 3.0)
        return;
    this->scale += delta;
    this->ui->graphicsView->scale(1.0 + delta, 1.0 + delta);
}

void ResizeLayoutPopup::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
        zoom(+0.1);
    } else if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore) {
        zoom(-0.1);
    } else {
        QDialog::keyPressEvent(event);
    }
}
