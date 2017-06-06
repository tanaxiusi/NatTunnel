TEMPLATE = app
TARGET = Server
VERSION = 2017.6.6.0
QT += core network sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += console
INCLUDEPATH += . \
	./../Shared
include(Server.pri)
