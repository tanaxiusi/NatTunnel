#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>
#include "Other.h"
#include "KcpManager.h"

class Client : public QObject
{
	Q_OBJECT

signals:
	void connected();
	void disconnected();
	void logined();
	void loginFailed(QString msg);
	void natTypeConfirmed(NatType natType);
	void firewallWarning();
	quint16 wannaAddUpnpPortMapping(quint16 internalPort);
	void wannaDeleteUpnpPortMapping(quint16 externalPort);

	void replyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void replyReadyTunneling(int requestId, int tunnelId, QString peerUserName);
	void tunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void tunnelHandShaked(int tunnelId);
	void tunnelData(int tunnelId, QByteArray package);
	void tunnelClosed(int tunnelId);

private:
	enum ClientStatus
	{
		UnknownClientStatus = 0,
		ConnectingStatus,
		ConnectedStatus,
		LoginingStatus,
		LoginedStatus,
		LoginFailedStatus
	};

	enum NatCheckStatus
	{
		UnknownNatCheckStatus = 0,
		Step1_1SendingToServer1,
		Step1_1WaitingForServer2,
		Step2_Type1_12WaitingForServer1,
		Step2_Type1_2WaitingForServer1,
		Step2_Type2_1SendingToServer2,
		NatCheckFinished
	};

	enum TunnelStatus
	{
		UnknownTunnelStatus = 0,
		ReadyTunnelingStatus,
		TunnelingStatus
	};

	struct TunnelInfo
	{
		TunnelStatus status = UnknownTunnelStatus;
		int localIndex = 0;		// 本地udp端口编号(1或2)
		QString peerUserName;
		QHostAddress peerAddress;
		quint16 peerPort = 0;
		ikcpcb * kcp = nullptr;
	};

public:
	Client(QObject *parent = 0);
	~Client();

	void setUserInfo(QString userName, QString password);
	void setLocalPassword(QString localPassword);
	void setServerInfo(QHostAddress hostAddress, quint16 tcpPort);

	bool start();
	bool stop();

	QHostAddress getLocalAddress();
	QHostAddress getLocalPublicAddress();

	void setUpnpAvailable(bool upnpAvailable);

	void tryTunneling(QString peerUserName);
	int readyTunneling(QString peerUserName, QString peerLocalPassword, bool useUpnp);
	
private slots:
	void onTcpConnected();
	void onTcpDisconnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();
	void timerFunction15s();
	void onKcpLowLevelOutput(int tunnelId, QHostAddress hostAddress, quint16 port, QByteArray package);
	void onKcpHighLevelOutput(int tunnelId, QByteArray package);

private:
	QUdpSocket * getUdpSocket(int index);
	quint16 getServerUdpPort(int index);
	int getServerIndexFromUdpPort(quint16 serverUdpPort);

	void clear();
	void startConnect();

	bool checkStatus(ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatusAndDisconnect(QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus);

	void disconnectServer(QString reason);
	void sendUdp(int localIndex, int serverIndex, QByteArray package);
	void onUdpReadyRead(int localIndex);

	void dealTcpIn(QByteArray line);
	void dealUdpIn_server(int localIndex, int serverIndex, const QByteArray & line);
	void dealUdpIn_p2p(int localIndex, QHostAddress peerAddress, quint16 peerPort, const QByteArray & package);

	void checkFirewall();
	void addUpnpPortMapping();
	void deleteUpnpPortMapping();
	quint32 getKcpMagicNumber(QString peerUserName);

private:
	void tcpOut_heartbeat();
	void tcpIn_heartbeat();

	void tcpIn_hello(QString serverName, QHostAddress clientAddress);

	void tcpOut_login();
	void tcpIn_login(bool loginOk, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2);

	void udpIn_checkNatStep1(int localIndex, int serverIndex);
	void timeout_checkNatStep1();
	void tcpOut_checkNatStep1(int partlyType, quint16 clientUdp2LocalPort = 0);

	void udpIn_checkNatStep2Type1(int localIndex, int serverIndex);
	void timeout_checkNatStep2Type1();
	void tcpOut_checkNatStep2Type1(NatType natType);

	void tcpIn_checkNatStep2Type2(NatType natType);

	void tcpOut_upnpAvailability(bool on);

	void tcpOut_tryTunneling(QString peerUserName);
	void tcpIn_tryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);

	void tcpOut_readyTunneling(QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId);
	void tcpIn_readyTunneling(QString peerUserName, int requestId, int tunnelId);

	void tcpIn_startTunneling(int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp);
	void tcpOut_startTunneling(int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString);

	void tcpIn_tunneling(int tunnelId, QHostAddress peerAddress, quint16 peerPort);
	void tcpIn_closeTunneling(int tunnelId);

private:
	bool m_running = false;

	QTcpSocket m_tcpSocket;
	QUdpSocket m_udpSocket1;
	QUdpSocket m_udpSocket2;
	QTimer m_timer300ms;
	QTimer m_timer15s;
	KcpManager m_kcpManager;

	QString m_userName;
	QString m_password;
	QString m_localPassword;

	QHostAddress m_serverHostAddress;
	quint16 m_serverTcpPort = 0;
	quint16 m_serverUdpPort1 = 0;
	quint16 m_serverUdpPort2 = 0;

	ClientStatus m_status = UnknownClientStatus;
	NatCheckStatus m_natStatus = UnknownNatCheckStatus;
	QTime m_beginWaitTime;
	NatType m_natType = UnknownNatType;

	QTime m_lastInTime;
	QTime m_lastOutTime;

	bool m_upnpAvailability = false;
	bool m_isPublicNetwork = false;
	QHostAddress m_localPublicAddress;

	quint16 m_udp2UpnpPort = 0;

	QMap<int, TunnelInfo> m_mapTunnelInfo;
	QMap<QPair<QHostAddress, quint16>, int> m_mapHostTunnelId;
};
