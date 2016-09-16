#pragma once

#include <QObject>
#include <QPair>
#include <QHostAddress>
#include <QTimer>
#include "ikcp.h"
#include "Other.h"

class KcpManager : public QObject
{
	Q_OBJECT

signals :
	void highLevelOutput(int tunnelId, QByteArray package);
	void wannaLowLevelOutput(int tunnelId, QHostAddress hostAddress, quint16 port, QByteArray package);

private:
	struct Peer
	{
		QHostAddress address;
		quint16 port;
		Peer(QHostAddress address = QHostAddress(), quint16 port = 0)
		{
			this->address = address;
			this->port = port;
		}
	};
	struct KcpConnection
	{
		Peer peer;
		ikcpcb * kcp = nullptr;
		int tunnelId;
		bool handShaked = false;
	};



public:
	KcpManager(QObject *parent = 0);
	~KcpManager();

	void clear();

	void createKcpConnection(int tunnelId, QHostAddress hostAddress, quint16 port, quint32 magicNumber);
	void deleteKcpConnection(int tunnelId);

	void lowLevelInput(QHostAddress hostAddress, quint16 port, QByteArray package);
	int highLevelInput(int tunnelId, QByteArray package);

private slots:
	void updateAllKcp();

private:
	static int kcp_write(const char *buf, int len, ikcpcb *kcp, void *user);
	int kcp_write(const char *buf, int len, ikcpcb *kcp);
	KcpConnection * getConnectionByPeer(const Peer & peer, bool allowUncertain);
	void updateKcp(ikcpcb * kcp);

private:
	QMap<int, ikcpcb*> m_mapTunnelId;
	QMap<ikcpcb*, KcpConnection> m_map;
	QTimer m_timerUpdate;
};
