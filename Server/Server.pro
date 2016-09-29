TEMPLATE = app
TARGET = Server
QT += core network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += console
INCLUDEPATH += ./../Shared
include(Server.pri)
