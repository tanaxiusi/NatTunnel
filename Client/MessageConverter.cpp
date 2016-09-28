#include "MessageConverter.h"
#include "aes.h"
#include "aes.c"

void MessageConverter::setKey(const quint8 * key)
{
	memcpy(m_key, key, sizeof(m_key));
}

QByteArray MessageConverter::parse(QByteArray message, QByteArrayMap * outArgument)
{
	if (message.endsWith('\n'))
		message.chop(1);
	QByteArray encryptedLine = QByteArray::fromBase64(message);
	if (encryptedLine.isEmpty() || encryptedLine.size() % 16 != 0)
		return QByteArray();
	QByteArray line(encryptedLine.size(), 0);
	for (int i = 0; i < encryptedLine.size(); i += 16)
		AES128_ECB_decrypt((quint8*)encryptedLine.constData() + i, m_key, (quint8*)(line.data() + i));
	if (!line.startsWith("M"))
		return QByteArray();
	line = line.mid(1);

	while (line.endsWith('\0'))
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

QByteArray MessageConverter::serialize(QByteArray type, QByteArrayMap argument)
{
	if (type.contains(" "))
		return QByteArray();
	QByteArray line = "M" + type;

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
	if (line.size() % 16 != 0)
		line.append(QByteArray(16 - line.size() % 16, 0));
	QByteArray encryptedLine(line.size(), 0);
	for (int i = 0; i < line.size(); i += 16)
		AES128_ECB_encrypt((quint8*)(line.constData() + i), m_key, (quint8*)(encryptedLine.data() + i));
	return encryptedLine.toBase64() + "\n";
}

QString MessageConverter::argumentToString(const QByteArrayMap & argument)
{
	QString result = "{";

	for (auto iter = argument.constBegin(); iter != argument.constEnd(); ++iter)
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
