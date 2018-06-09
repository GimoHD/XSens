#ifndef LINKSENSORSSCREEN_H
#define LINKSENSORSSCREEN_H

#include <QPushButton>
#include <QDialog>
#include <QFile>
#include <QString>
#include <QTextStream>

namespace Ui {
class LinkSensorsScreen;
}

class LinkSensorsScreen : public QDialog
{
    Q_OBJECT

public:
    explicit LinkSensorsScreen(QHash<QString, QString>*, QWidget *parent = 0);
    ~LinkSensorsScreen();
    /*!
    \fn void addData()

    Add the sensor data to the table.
    */
    void addData();
    private slots:
        /*!
        \fn void changeValue(int row, int column)

        Slot that will be called if one of the cells has cellChanged(int,int)
        Will modify the QHash to the inserted name
        */
        void changeValue(int row, int column);

        /*!
        \fn void saveFile()

        Saves the names from the MTw's to the config.csv file.
        */
        void saveFile();

private:
    Ui::LinkSensorsScreen *ui;

    QHash<QString, QString> * hash;
};

#endif // LINKSENSORSSCREEN_H
