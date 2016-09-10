#include "Other.h"
#include "crc32.h"

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

bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b)
{
	if (a.protocol() == b.protocol())
		return a == b;

	if (a.protocol() == QAbstractSocket::IPv4Protocol && b.protocol() == QAbstractSocket::IPv6Protocol)
	{
		bool convertOk = false;
		return QHostAddress(b.toIPv4Address(&convertOk)) == a && convertOk;
	}
	else if (b.protocol() == QAbstractSocket::IPv4Protocol && a.protocol() == QAbstractSocket::IPv6Protocol)
	{
		bool convertOk = false;
		return QHostAddress(a.toIPv4Address(&convertOk)) == b && convertOk;
	}
	else
	{
		return a == b;
	}
}
