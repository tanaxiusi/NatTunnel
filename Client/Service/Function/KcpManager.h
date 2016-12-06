#pragma once

#include <QObject>
#include <QPair>
#include <QHostAddress>
#include <QTimer>
#include <QTime>
#include "kcp/ikcp.h"
#include "Util/Other.h"
#include "Util/Peer.h"

class KcpManager : public QObject
{
	Q_OBJECT

signals :
	void highLevelOutput(int tunnelId, QByteArray package);
	void wannaLowLevelOutput(int tunnelId, QHostAddress hostAddress, quint16 port, QByteArray package);
	void handShaked(int tunnelId);
	void disconnected(int tunnelId, QString reason);

private:
	struct KcpConnection
	{
		int tunnelId;
		QByteArray userName;
		Peer peer;
		ikcpcb * kcp;
		bool peerConfirmed;
		int shakeHandStep;
		QByteArray buffer;	// 仅握手时用到
		quint32 nextUpdateTime;
		QTime lastInTime;
		KcpConnection()
			:kcp(NULL), peerConfirmed(false), shakeHandStep(0)
		{}
	};

public:
	KcpManager(QObject *parent = 0);
	~KcpManager();

	void setUserName(QString userName);

	void clear();
	bool haveUnconfirmedKcpConnection();

	bool createKcpConnection(int tunnelId, QHostAddress hostAddress, quint16 port, QString userName, quint32 magicNumber);
	bool deleteKcpConnection(int tunnelId, QString reason);

	void lowLevelInput(QHostAddress hostAddress, quint16 port, QByteArray package);
	int highLevelInput(int tunnelId, QByteArray package);

private slots:
	void timerFunction10ms();
	void timerFunction5s();

private:
	static int kcp_write(const char *buf, int len, ikcpcb *kcp, void *user);
	int kcp_write(const char *buf, int len, ikcpcb *kcp);
	KcpConnection * getConnectionByPeer(const Peer & peer);
	void updateKcp(KcpConnection & connection, quint32 currentTime);

private:
	QByteArray m_userName;
	QMap<int, ikcpcb*> m_mapTunnelId;
	QMap<ikcpcb*, KcpConnection> m_map;
	QTimer m_timer10ms;
	QTimer m_timer5s;
	QTime m_cachedCurrentTime;		// 随着timer刷新的当前时间，为了减少lowLevelInput中的currentTime系统调用
};
