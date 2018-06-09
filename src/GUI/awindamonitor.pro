#-------------------------------------------------
#
# Project created by QtCreator 2011-12-02T11:26:58
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = awindamonitor
TEMPLATE = app

SOURCES +=  main.cpp \
		   mainwindow.cpp \
		   myxda.cpp \
		   connectedmtwdata.cpp \
    linksensorsscreen.cpp

HEADERS  += mainwindow.h \
		   myxda.h \
                   connectedmtwdata.h \
    linksensorsscreen.h

FORMS   += mainwindow.ui \
    linksensorsscreen.ui

RESOURCES += \
	awindamonitor.qrc

win32:RC_FILE = icon.rc



unix|win32: LIBS += -L$$PWD/../../x64/lib/ -lxstypes64

INCLUDEPATH += $$PWD/../../x64/include
DEPENDPATH += $$PWD/../../x64/include

unix|win32: LIBS += -L$$PWD/../../x64/lib/ -lxsensdeviceapi64

INCLUDEPATH += $$PWD/../../x64/include
DEPENDPATH += $$PWD/../../x64/include


INCLUDEPATH += $$PWD/../../x64/include
DEPENDPATH += $$PWD/../../x64/include

unix|win32: LIBS += -L$$PWD/../../../../../MATLAB/R2017b/extern/lib/win64/microsoft/ -llibmat
unix|win32: LIBS += -L$$PWD/../../../../../MATLAB/R2017b/bin/win64 -llibmat

INCLUDEPATH += $$PWD/../../../../../MATLAB/R2017b/extern/include
DEPENDPATH += $$PWD/../../../../../MATLAB/R2017b/extern/include

