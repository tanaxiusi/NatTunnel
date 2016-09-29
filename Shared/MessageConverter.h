#pragma once
#include <QByteArray>
#include <QMap>
#include <QString>

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

class MessageConverter
{
public:
	MessageConverter();

	void setKey(const quint8 * key);

	QByteArray parse(QByteArray message, QByteArrayMap * outArgument);
	QByteArray serialize(QByteArray type, QByteArrayMap argument);

	static QString argumentToString(const QByteArrayMap & argument);

private:
	quint8 m_key[16];
};

