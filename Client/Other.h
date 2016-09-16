#pragma once
#include <QByteArray>
#include <QMap>
#include <QHostAddress>

#if defined(Q_OS_WIN)
#define U16(str)	QString::fromUtf16(u##str)
#elif defined(Q_OS_LINUX)
#define U16(str)	QString::fromUtf8(str)
#endif

#define mycompare(a,b)				((a)==(b) ? 0 : ((a)<(b) ? -1 : 1))

enum NatType
{
	UnknownNatType = 0,
	PublicNetwork,
	FullOrRestrictedConeNat,
	PortRestrictedConeNat,
	SymmetricNat
};

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument);
QByteArray serializeResponse(QByteArray type, QByteArrayMap argument);
QString argumentToString(QByteArrayMap argument);
QByteArray checksumThenUnpackPackage(QByteArray package);
QByteArray addChecksumInfo(QByteArray package);
bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b);
QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress);
QString getNatDescription(NatType natType);
bool isNatAddress(const QHostAddress & hostAddress);