TEMPLATE = app
TARGET = Client
VERSION = 2017.6.6.0
QT += core network xml gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
INCLUDEPATH += . \
	./../Shared
include(Client.pri)
