#pragma once
#include <QtGlobal>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QHostAddress>

#if defined(Q_OS_WIN)
#define U16(str)	QString::fromUtf16((const ushort *)L##str)
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

QByteArray boolToQByteArray(bool value);
bool QByteArrayToBool(const QByteArray & value);

QByteArray checksumThenUnpackPackage(QByteArray package);

QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress);
QString getSocketPeerDescription(const QAbstractSocket * socket);

QString getNatDescription(NatType natType);
bool generalNameCheck(const QString & name);

QByteArray readFile(const QString fileName);