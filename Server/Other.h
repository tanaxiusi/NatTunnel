#pragma once
#include <QtGlobal>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QHostAddress>

#if defined(Q_OS_WIN)
#define U16(str)	QString::fromUtf16(u##str)
#elif defined(Q_OS_LINUX)
#define U16(str)	QString::fromUtf8(str)
#endif

enum NatType
{
	UnknownNatType = 0,
	PublicNetwork,
	FullOrRestrictedConeNat,
	PortRestrictedConeNat,
	SymmetricNat
};

quint32 rand_u32();

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument);
QByteArray serializeResponse(QByteArray type, QByteArrayMap argument);
QByteArray checksumThenUnpackUdpPackage(QByteArray package);

QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress);
QString getSocketPeerDescription(const QAbstractSocket * socket);
QString serializeQByteArrayMap(const QByteArrayMap & theMap);

QString getNatDescription(NatType natType);