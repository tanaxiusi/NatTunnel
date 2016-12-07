#pragma once
#include <QHostAddress>
#include <QDataStream>

struct Peer
{
	QHostAddress address;
	quint16 port;
	Peer(QHostAddress address = QHostAddress(), quint16 port = 0);
};


QDataStream & operator >> (QDataStream &in, Peer & peer);
QDataStream & operator<< (QDataStream &out, const Peer & peer);