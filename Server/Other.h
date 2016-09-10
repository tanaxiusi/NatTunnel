#pragma once
#include <QtGlobal>
#include <QString>
#include <QByteArray>
#include <QMap>

#if defined(Q_OS_WIN)
#define U16(str)	QString::fromUtf16(u##str)
#elif defined(Q_OS_LINUX)
#define U16(str)	QString::fromUtf8(str)
#endif

quint32 rand_u32();

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument);
QByteArray serializeResponse(QByteArray type, QByteArrayMap argument);
QByteArray checksumThenUnpackUdpPackage(QByteArray package);