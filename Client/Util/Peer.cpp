#include "Peer.h"

Peer::Peer(QHostAddress address, quint16 port)
{
	this->address = address;
	this->port = port;
}
