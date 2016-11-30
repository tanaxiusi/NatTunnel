TEMPLATE = app
TARGET = Client
QT += core network xml gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
INCLUDEPATH += . \
	./../Shared
include(Client.pri)
