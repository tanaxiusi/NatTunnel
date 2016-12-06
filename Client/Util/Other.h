#pragma once
#include <QByteArray>
#include <QMap>
#include <QHostAddress>
#include <QStringList>

#if defined(Q_OS_WIN)
#define U16(str)	QString::fromUtf16((const ushort *)L##str)
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

enum UpnpStatus
{
	UpnpUnknownStatus = 0,
	UpnpDiscovering,
	UpnpUnneeded,
	UpnpQueryingExternalAddress,
	UpnpOk,
	UpnpFailed
};

#define strcpy_(dest,src)			strcopy((dest),sizeof(dest),(src))
char * strcopy(char * dst, size_t SizeInBytes, const char * src);

quint32 rand_u32();

QByteArray boolToQByteArray(bool value);
bool QByteArrayToBool(const QByteArray & value);

QByteArray checksumThenUnpackPackage(QByteArray package);
QByteArray checksumThenUnpackPackage(QByteArray package, QByteArray userName);
QByteArray addChecksumInfo(QByteArray package);
QByteArray addChecksumInfo(QByteArray package, QByteArray userName);
bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b);
QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress);
QString getNatDescription(NatType natType);
QString getUpnpStatusDescription(UpnpStatus upnpStatus);
bool isNatAddress(const QHostAddress & hostAddress);
QString getNetworkInterfaceHardwareAddress(QHostAddress localAddress);
QStringList getGatewayAddress(QString localAddress);
QString arpGetHardwareAddress(QString targetAddress, QString localAddress);
QByteArray readFile(const QString fileName);
bool writeFile(const QString & fileName, const QByteArray & content);