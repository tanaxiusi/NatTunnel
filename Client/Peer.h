#pragma once
#include <QHostAddress>

struct Peer
{
	QHostAddress address;
	quint16 port;
	Peer(QHostAddress address = QHostAddress(), quint16 port = 0);
};
