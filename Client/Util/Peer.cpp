#include "Peer.h"

Peer::Peer(QHostAddress address, quint16 port)
{
	this->address = address;
	this->port = port;
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
