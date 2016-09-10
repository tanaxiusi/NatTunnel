#pragma once
#include <QByteArray>
#include <QMap>
#include <QHostAddress>

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument);
QByteArray serializeResponse(QByteArray type, QByteArrayMap argument);
QByteArray checksumThenUnpackUdpPackage(QByteArray package);
bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b);