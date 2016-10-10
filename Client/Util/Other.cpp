#include "Other.h"
#include "crc32/crc32.h"
#include <QNetworkInterface>
#include <QFile>

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <iphlpapi.h>
#pragma comment(lib,"ws2_32")
#pragma comment(lib,"Iphlpapi")
#endif

static const int _typeId_NatType = qRegisterMetaType<NatType>("NatType");
static const int _typeId_UpnpStatus = qRegisterMetaType<UpnpStatus>("UpnpStatus");
static const int _typeId_QHostAddress = qRegisterMetaType<QHostAddress>("QHostAddress");

char * strcopy(char * dst, size_t SizeInBytes, const char * src)
{
	char * cp = dst;
	while (*cp++ = *src++)
		if (cp - dst == SizeInBytes - 1)
		{
			*cp = '\0';
			break;
		}

	return(dst);
}

quint32 rand_u32()
{
	quint32 randValueList[] = { (quint32)rand() & 0x7FFF, (quint32)rand() & 0x7FFF, (quint32)rand() & 3 };
	return randValueList[0] | (randValueList[1] << 15) | (randValueList[2] << 30);
}

QByteArray checksumThenUnpackPackage(QByteArray package)
{
	if (package.size() < 4)
		return QByteArray();
	const QByteArray content = QByteArray::fromRawData(package.constData() + 4, package.size() - 4);
	const quint32 receivedCrc = *(quint32*)package.constData();
	const quint32 actualCrc = crc32(content.constData(), content.size());
	if (receivedCrc != actualCrc)
		return QByteArray();
	return content;
}

QByteArray checksumThenUnpackPackage(QByteArray package, QByteArray userName)
{
	if (package.size() < 4)
		return QByteArray();
	const QByteArray content = QByteArray::fromRawData(package.constData() + 4, package.size() - 4);
	const quint32 receivedCrc = *(quint32*)package.constData();
	quint32 actualCrc = crc32(content.constData(), content.size());
	actualCrc = crc32(actualCrc, userName.constData(), userName.size());
	if (receivedCrc != actualCrc)
		return QByteArray();
	return content;
}

QByteArray addChecksumInfo(QByteArray package)
{
	quint32 crc = crc32(package.constData(), package.size());
	package.insert(0, (const char*)&crc, 4);
	return package;
}

QByteArray addChecksumInfo(QByteArray package, QByteArray userName)
{
	quint32 crc = crc32(package.constData(), package.size());
	crc = crc32(crc, userName.constData(), userName.size());
	package.insert(0, (const char*)&crc, 4);
	return package;
}

bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b)
{
	if (a.protocol() == b.protocol())
		return a == b;

	if (a.protocol() == QAbstractSocket::IPv4Protocol && b.protocol() == QAbstractSocket::IPv6Protocol)
	{
		quint32 bIpv4 = b.toIPv4Address();
		return bIpv4 && QHostAddress(bIpv4) == a;
	}
	else if (b.protocol() == QAbstractSocket::IPv4Protocol && a.protocol() == QAbstractSocket::IPv6Protocol)
	{
		quint32 aIpv4 = a.toIPv4Address();
		return aIpv4 && QHostAddress(aIpv4) == b;
	}
	else
	{
		return a == b;
	}
}

QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress)
{
	if (hostAddress.protocol() == QAbstractSocket::IPv4Protocol)
		return hostAddress;
	if (hostAddress.protocol() == QAbstractSocket::IPv6Protocol)
	{
		quint32 ipv4Address = hostAddress.toIPv4Address();
		if (ipv4Address)
			return QHostAddress(ipv4Address);
	}
	return hostAddress;
}


QString getNatDescription(NatType natType)
{
	switch (natType)
	{
	case UnknownNatType:
		return "UnknownNatType";
	case PublicNetwork:
		return "PublicNetwork";
	case FullOrRestrictedConeNat:
		return "FullOrRestrictedConeNat";
	case PortRestrictedConeNat:
		return "PortRestrictedConeNat";
	case SymmetricNat:
		return "SymmetricNat";
	default:
		return "Error";
	}
}

bool isNatAddress(const QHostAddress & hostAddress)
{
	const quint32 ipv4 = hostAddress.toIPv4Address();
	if (!ipv4)
		return false;

	const quint8 * a = (const quint8 *)&ipv4;
	if (a[3] == 10)
		return true;
	if (a[3] == 100 && a[2] >= 64 && a[2] <= 127)
		return true;
	if (a[3] == 172 && a[2] >= 16 && a[2] <= 31)
		return true;
	if (a[3] == 192 && a[2] == 168)
		return true;
	return false;
}

QString getNetworkInterfaceHardwareAddress(QHostAddress localAddress)
{
	foreach (QNetworkInterface card, QNetworkInterface::allInterfaces())
		foreach (QNetworkAddressEntry entry, card.addressEntries())
			if (entry.ip() == localAddress)
				return card.hardwareAddress();
	return QString();
}

QStringList getGatewayAddress(QString localAddress)
{
#if defined(Q_OS_WIN)
	ULONG neededSize = 0;
	QByteArray buffer;
	if (ERROR_BUFFER_OVERFLOW != GetAdaptersInfo(NULL, &neededSize))
		return QStringList();

	buffer.resize(neededSize);
	if (ERROR_SUCCESS != GetAdaptersInfo((IP_ADAPTER_INFO*)buffer.data(), &neededSize))
		return QStringList();

	localAddress.trimmed();

	for (const IP_ADAPTER_INFO * info = (const IP_ADAPTER_INFO*)buffer.constData();
		info; info = info->Next)
	{
		const IP_ADDR_STRING * ip = NULL;
		for (ip = &info->IpAddressList; ip; ip = ip->Next)
			if (localAddress == ip->IpAddress.String)
				break;
		if (ip)
		{
			QStringList result;
			for (const IP_ADDR_STRING * gateway = &info->GatewayList; gateway; gateway = gateway->Next)
				result << gateway->IpAddress.String;
			return result;
		}
	}
#endif

	return QStringList();
}

QString arpGetHardwareAddress(QString targetAddress, QString localAddress)
{
#if defined(Q_OS_WIN)
	ULONG targetIP = inet_addr(targetAddress.toUtf8().constData());
	if (targetIP == INADDR_NONE)
		return QString();
	UCHAR mac[6] = { 0 };
	ULONG macLen = 6;
	ULONG localIP = inet_addr(localAddress.toUtf8().constData());
	DWORD retValue = SendARP(targetIP, localIP, &mac, &macLen);
	if (retValue != NO_ERROR)
		return QString();
	QString result;
	for (int i = 0; i < (int)macLen; i++)
	{
		if (result.length() > 0)
			result += "-";
		result += QByteArray(1, mac[i]).toHex();
	}
	return result;
#else
	return QString();
#endif
}

QByteArray readFile(const QString fileName)
{
	QFile file(fileName);
	if (file.open(QIODevice::ReadOnly))
		return file.readAll();
	else
		return QByteArray();
}

bool writeFile(const QString & fileName, const QByteArray & content)
{
	QFile file(fileName);
	if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly))
		return false;
	file.write(content);
	return true;
}
