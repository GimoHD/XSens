#include "linksensorsscreen.h"
#include "ui_linksensorsscreen.h"

LinkSensorsScreen::LinkSensorsScreen(QHash<QString, QString> * qhash, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LinkSensorsScreen)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Save");
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("Don't save");
    hash  = qhash;
    addData();
    connect(ui->tableWidget, SIGNAL(cellChanged(int, int)), this,
            SLOT(changeValue(int, int)));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveFile()));

    ui->tableWidget->sortByColumn(1,Qt::AscendingOrder);
}

LinkSensorsScreen::~LinkSensorsScreen()
{
    delete ui;
}


void LinkSensorsScreen::addData()
{
    //Setup table and headers
    ui->tableWidget->setColumnCount(2);
    QTableWidgetItem *header1 = new QTableWidgetItem();
    header1->setText("ID");
    ui->tableWidget->setHorizontalHeaderItem(0,header1);
    QTableWidgetItem *header2 = new QTableWidgetItem();
    header2->setText("Name");
    ui->tableWidget->setHorizontalHeaderItem(1,header2);

    QHashIterator<QString, QString> i(*hash);
    while (i.hasNext()) {
        i.next();
        ui->tableWidget->insertRow ( ui->tableWidget->rowCount() );
        QTableWidgetItem *key = new QTableWidgetItem(i.key());
        QTableWidgetItem *value = new QTableWidgetItem(i.value());
        key->setFlags(key->flags() ^ Qt::ItemIsEditable);
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1, 0, key);
        ui->tableWidget->setItem(ui->tableWidget->rowCount()-1, 1, value);
    }
}

void LinkSensorsScreen::changeValue(int row, int column)
{
     QString key = ui->tableWidget->item(row, 0)->text();
     QString value = ui->tableWidget->item(row, column)->text();
     hash->insert(key, value);

}

void LinkSensorsScreen::saveFile()
{
    QHashIterator<QString, QString> i(*hash);
    QFile file("config.csv");
    if (file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
      QTextStream fileStream(&file);

    while (i.hasNext()) {
        i.next();
        fileStream << QString(i.key())
                       << ","
                       << QString(i.value()) << endl;

    }
    file.close();
    }
}


