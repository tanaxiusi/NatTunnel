#pragma once
#include <QHostAddress>
#include <QDataStream>

struct Peer
{
	QHostAddress address;
	quint16 port;
	Peer(QHostAddress address = QHostAddress(), quint16 port = 0);

	QString toString() const;
	static Peer fromString(const QString & str);
};


QDataStream & operator >> (QDataStream &in, Peer & peer);
QDataStream & operator<< (QDataStream &out, const Peer & peer);