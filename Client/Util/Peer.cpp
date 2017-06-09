#include "Peer.h"
#include "Other.h"

Peer::Peer(QHostAddress address, quint16 port)
{
	this->address = address;
	this->port = port;
}

QString Peer::toString() const
{
	return tryConvertToIpv4(address).toString() + "/" + QString::number(port);
}

Peer Peer::fromString(const QString & str)
{
	const int separatorPos = str.indexOf('/');
	if (separatorPos >= 0)
	{
		const QString addressText = str.left(separatorPos);
		const QString portText = str.right(str.length() - separatorPos - 1);
		return Peer(QHostAddress(addressText), portText.toInt());
	}
	else
	{
		return Peer();
	}
}

QDataStream & operator >> (QDataStream &in, Peer & peer)
{
	in >> peer.address >> peer.port;
	return in;
}

QDataStream & operator<<(QDataStream & out, const Peer & peer)
{
	out << peer.address << peer.port;
	return out;
}
