#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include "Util/Other.h"
#include "KcpManager.h"
#include "QUpnpPortMapper.h"
#include "MessageConverter.h"
#include "TransferManager.h"

class Client : public QObject
{
	Q_OBJECT

signals:
	void connected();
	void disconnected();
	void discarded(QString reason);
	void binaryError(QByteArray correctBinary);
	void logined();
	void loginFailed(QString userName, QString msg);
	void natTypeConfirmed(NatType natType);
	void upnpStatusChanged(UpnpStatus upnpStatus);

	void warning(QString msg);

	void replyQueryOnlineUser(QStringList onlineUserList);

	void replyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void replyReadyTunneling(int requestId, int tunnelId, QString peerUserName);
	void tunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void tunnelHandShaked(int tunnelId);
	void tunnelClosed(int tunnelId, QString peerUserName, QString reason);

private:
	enum ClientStatus
	{
		UnknownClientStatus = 0,
		ConnectingStatus,
		ConnectedStatus,
		BinaryCheckingStatus,
		BinaryCheckedStatus,
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
		TunnelStatus status;
		int localIndex;		// 本地udp端口编号(1或2)
		QString peerUserName;
		QHostAddress peerAddress;
		quint16 peerPort;
		ikcpcb * kcp;
		TunnelInfo()
			:status(UnknownTunnelStatus),localIndex(0),peerPort(0),kcp(NULL)
		{}

	};

public:
	Client(QObject *parent = 0);
	~Client();

public slots:
	void setConfig(QByteArray globalKey, QString randomIdentifierSuffix, QHostAddress serverHostAddress, quint16 serverTcpPort);
	void setUserName(QString userName);
	void setLocalPassword(QString localPassword);
	bool start();
	bool stop();
	bool tryLogin();
	void queryOnlineUser();
	void tryTunneling(QString peerUserName);
	int readyTunneling(QString peerUserName, QString peerLocalPassword, bool useUpnp);
	void closeTunneling(int tunnelId);

	bool addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	bool deleteTransfer(int tunnelId, quint16 localPort);
	QMap<quint16, Peer> getTransferOutList(int tunnelId);
	QMap<quint16, Peer> getTransferInList(int tunnelId);
	
private slots:
	void onTcpConnected();
	void onTcpDisconnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();
	void timerFunction10s();
	void onUpnpDiscoverFinished(bool ok);
	void onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString);
	void onKcpLowLevelOutput(int tunnelId, QHostAddress hostAddress, quint16 port, QByteArray package);
	void onKcpConnectionHandShaked(int tunnelId);
	void onKcpConnectionDisconnected(int tunnelId, QString reason);

private:
	QUdpSocket * getUdpSocket(int index);
	quint16 getServerUdpPort(int index);
	int getServerIndexFromUdpPort(quint16 serverUdpPort);

	void clear();
	void startConnect();
	bool refreshIdentifier();

	bool checkStatus(ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatus(ClientStatus correctStatus);
	bool checkStatusAndDisconnect(QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatusAndDisconnect(QString functionName, ClientStatus correctStatus);

	void disconnectServer(QString reason);
	void sendTcp(QByteArray type, QByteArrayMap argument);
	void sendUdp(int localIndex, int serverIndex, QByteArray package);
	void sendUdp(int localIndex, int serverIndex, QByteArray type, QByteArrayMap argument);
	void onUdpReadyRead(int localIndex);

	void dealTcpIn(QByteArray line);
	void dealUdpIn_server(int localIndex, int serverIndex, const QByteArray & line);
	void dealUdpIn_p2p(int localIndex, QHostAddress peerAddress, quint16 peerPort, const QByteArray & package);

	QHostAddress getLocalAddress();
	QHostAddress getLocalPublicAddress();
	void checkFirewall();
	void addUpnpPortMapping();
	void deleteUpnpPortMapping();
	quint32 getKcpMagicNumber(QString peerUserName);
	void createKcpConnection(int tunnelId, TunnelInfo & tunnel);

private:
	void tcpOut_heartbeat();
	void tcpIn_heartbeat();

	void tcpIn_discard(QString reason);

	void tcpIn_hello(QString serverName, QHostAddress clientAddress);

	void tcpOut_checkBinary(QString platform, QByteArray binaryChecksum);
	void tcpIn_checkBinary(bool correct, QByteArray compressedBinary);

	void tcpOut_login(QString identifier, QString userName);
	void tcpIn_login(bool loginOk, QString userName, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2);

	void tcpOut_localNetwork(QHostAddress localAddress, quint16 clientUdp1LocalPort, QString gatewayInfo);

	void udpIn_checkNatStep1(int localIndex, int serverIndex);
	void timeout_checkNatStep1();
	void tcpOut_checkNatStep1(int partlyType, quint16 clientUdp2LocalPort = 0);

	void udpIn_checkNatStep2Type1(int localIndex, int serverIndex);
	void timeout_checkNatStep2Type1();
	void tcpOut_checkNatStep2Type1(NatType natType);

	void tcpIn_checkNatStep2Type2(NatType natType);

	void tcpOut_upnpAvailability(bool on);

	void udpOut_updateAddress();

	void tcpOut_queryOnlineUser();
	void tcpIn_queryOnlineUser(QString onlineUser);

	void tcpOut_tryTunneling(QString peerUserName);
	void tcpIn_tryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);

	void tcpOut_readyTunneling(QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId);
	void tcpIn_readyTunneling(QString peerUserName, int requestId, int tunnelId);

	void tcpIn_startTunneling(int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp);
	void tcpOut_startTunneling(int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString);

	void tcpIn_tunneling(int tunnelId, QHostAddress peerAddress, quint16 peerPort);
	void tcpOut_tunnelHandeShaked(int tunnelId);

	void tcpIn_closeTunneling(int tunnelId, QString reason);
	void tcpOut_closeTunneling(int tunnelId, QString reason);

private:
	bool m_running;
	bool m_discarded;

	MessageConverter m_messageConverter;
	QTcpSocket m_tcpSocket;
	QUdpSocket m_udpSocket1;
	QUdpSocket m_udpSocket2;
	QTimer m_timer300ms;
	QTimer m_timer10s;
	KcpManager m_kcpManager;
	QUpnpPortMapper m_upnpPortMapper;
	TransferManager m_transferManager;

	QString m_randomIdentifierSuffix;
	QString m_identifier;

	QString m_userName;
	QString m_localPassword;

	QHostAddress m_serverHostAddress;
	quint16 m_serverTcpPort;
	quint16 m_serverUdpPort1;
	quint16 m_serverUdpPort2;

	ClientStatus m_status;
	NatCheckStatus m_natStatus;
	QTime m_beginWaitTime;
	NatType m_natType;

	UpnpStatus m_upnpStatus;

	QTime m_lastInTime;
	QTime m_lastOutTime;

	bool m_upnpAvailability;
	bool m_isPublicNetwork;
	QHostAddress m_localPublicAddress;

	QStringList m_lstGatewayInfo;		// 内网情况下，存储网关信息，List<"网关IP 网关MAC">

	quint16 m_udp2UpnpPort;

	QSet<int> m_waitingRequestId;

	QMap<int, TunnelInfo> m_mapTunnelInfo;
	QMap<QPair<QHostAddress, quint16>, int> m_mapHostTunnelId;
};