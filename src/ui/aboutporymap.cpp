#include "aboutporymap.h"
#include "ui_aboutporymap.h"
#include "log.h"

AboutPorymap::AboutPorymap(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AboutPorymap)
{
    ui->setupUi(this);
    ui->label_Version->setText(QString("Version %1 - %2").arg(PORYMAP_VERSION).arg(QStringLiteral(__DATE__)));
}

AboutPorymap::~AboutPorymap()
{
    delete ui;
}
