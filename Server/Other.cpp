#include "Other.h"
#include "crc32.h"


quint32 rand_u32()
{
	quint32 randValueList[] = { (quint32)rand() & 0x7FFF, (quint32)rand() & 0x7FFF, (quint32)rand() & 3 };
	return randValueList[0] | (randValueList[1] << 15) | (randValueList[2] << 30);
}

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument)
{
	if (line.endsWith('\n'))
		line.chop(1);
	const int spacePos = line.indexOf(' ');
	if (spacePos < 0)
		return line;
	const QByteArray type = line.left(spacePos);
	if (type.isEmpty())
		return QByteArray();
	if (outArgument)
	{
		QList<QByteArray> argumentList = line.mid(type.length() + 1).split(' ');
		for (QByteArray argumentText : argumentList)
		{
			const int equalPos = argumentText.indexOf('=');
			if (equalPos < 0)
				return QByteArray();
			QByteArray argumentName = argumentText.left(equalPos);
			QByteArray argumentValue = QByteArray::fromHex(argumentText.mid(equalPos + 1));
			(*outArgument)[argumentName] = argumentValue;
		}
	}
	return type;
}

QByteArray serializeResponse(QByteArray type, QByteArrayMap argument)
{
	if (type.contains(" "))
		return QByteArray();
	QByteArray line = type;

	for (auto iter = argument.constBegin(); iter != argument.constEnd(); ++iter)
	{
		const QByteArray & argumentName = iter.key();
		const QByteArray & argumentValue = iter.value();

		if (argumentName.contains("=") || argumentName.contains(" "))
			return QByteArray();

		line.append(" ");
		line.append(argumentName);
		line.append("=");
		line.append(argumentValue.toHex());
	}
	line.append('\n');
	return line;
}

QByteArray checksumThenUnpackUdpPackage(QByteArray package)
{
	if (package.size() < 4)
		return QByteArray();
	const QByteArray content = QByteArray::fromRawData(package.constData() + 4, package.size() - 4);
	const uint32_t receivedCrc = *(uint32_t*)package.constData();
	const uint32_t actualCrc = crc32(content.constData(), content.size());
	if (receivedCrc != actualCrc)
		return QByteArray();
	return content;
}

QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress)
{
	if (hostAddress.protocol() == QAbstractSocket::IPv4Protocol)
		return hostAddress;
	if (hostAddress.protocol() == QAbstractSocket::IPv6Protocol)
	{
		bool ok = false;
		auto ipv4Address = hostAddress.toIPv4Address(&ok);
		if (ok)
			return QHostAddress(ipv4Address);
	}
	return hostAddress;
}

QString getSocketPeerDescription(const QAbstractSocket * socket)
{
	return QString("%1:%2").arg(tryConvertToIpv4(socket->peerAddress()).toString()).arg(socket->peerPort());
}

QString serializeQByteArrayMap(const QByteArrayMap & theMap)
{
	QString result = "{";

	for (auto iter = theMap.constBegin(); iter != theMap.constEnd(); ++iter)
	{
		const QByteArray & key = iter.key();
		const QByteArray & value = iter.value();
		result += key;
		result += "='";
		result += value;
		result += "',";
	}
	if (result.at(result.length() - 1) == ',')
		result[result.length() - 1] = '}';
	else
		result += "}";
	return result;
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
