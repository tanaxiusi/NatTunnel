#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>
#include "Util/Other.h"
#include "Util/QStringMap.h"
#include "MessageConverter.h"
#include "UserContainer.h"

class ClientManager : public QObject
{
	Q_OBJECT

	enum ClientStatus
	{
		UnknownClientStatus = 0,
		ConnectedStatus,
		BinaryCheckedStatus,
		LoginedStatus,
		DiscardedStatus			// 废弃状态。由于通讯格式错误或被踢下线，过一段时间会被清理
	};
	enum NatCheckStatus
	{
		UnknownNatCheckStatus = 0,
		Step1_1WaitingForClient1,
		Step1_12SendingToClient1,
		Step2_Type1_1SendingToClient12,
		Step2_Type2_2WaitingForClient1,
		NatCheckFinished
	};

	struct ClientInfo
	{
		ClientStatus status;
		QString identifier;
		QString userName;
		NatType natType;
		NatCheckStatus natStatus;
		QHostAddress udpHostAddress;
		quint16 udp1Port1;
		quint16 udp1LocalPort;
		quint16 udp2LocalPort;
		QHostAddress localAddress;
		QString gatewayInfo;
		bool upnpAvailable;
		QTime lastInTime;
		QTime lastOutTime;
		ClientInfo()
			:natType(UnknownNatType), natStatus(UnknownNatCheckStatus),
			udp1Port1(0), udp1LocalPort(0), udp2LocalPort(0), upnpAvailable(false)
		{}
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
		QString clientAUserName;		// A为发起方，B为接收方，虽然有所区分，但功能上对等
		QString clientBUserName;
		quint16 clientAUdp2UpnpPort;	// 如果开启了upnp，这个端口号非0
		quint16 clientBUdp2UpnpPort;
		TunnelInfo()
			:status(UnknownTunnelStatus), clientAUdp2UpnpPort(0), clientBUdp2UpnpPort(0)
		{}
	};

	struct PlatformBinaryInfo
	{
		QByteArray checksum;
		QByteArray binary;
		QByteArray cachedMessage;
	};

public:
	ClientManager(QObject *parent = 0);
	~ClientManager();

	void setGlobalKey(QByteArray key);
	void setDatabase(QString fileName, QString userName, QString password);
	void setPlatformBinary(QString platform, QByteArray binary);

	bool start(quint16 tcpPort, quint16 udpPort1, quint16 udpPort2);
	bool stop();

private slots:
	void onTcpNewConnection();
	void onTcpDisconnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();
	void timerFunction15s();

private:
	QUdpSocket * getUdpServer(int index);

	bool checkStatus(QTcpSocket & tcpSocket, ClientInfo & client, ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatus(QTcpSocket & tcpSocket, ClientInfo & client, ClientStatus correctStatus);
	bool checkStatusAndDiscard(QTcpSocket & tcpSocket, ClientInfo & client, QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus);
	bool checkStatusAndDiscard(QTcpSocket & tcpSocket, ClientInfo & client, QString functionName, ClientStatus correctStatus);

	void disconnectClient(QTcpSocket & tcpSocket, ClientInfo & client, QString reason);

	void sendTcp(QTcpSocket & tcpSocket, ClientInfo & client, QByteArray type, QByteArrayMap argument);
	void sendTcpRaw(QTcpSocket & tcpSocket, ClientInfo & client, QByteArray line);
	void sendUdp(int index, QByteArray type, QByteArrayMap argument, QHostAddress hostAddress, quint16 port);
	void onUdpReadyRead(int index);

	bool checkCanTunnel(ClientInfo & localClient, QString peerUserName, bool * outLocalNeedUpnp, bool * outPeerNeedUpnp, QString * outFailReason);
	bool isExistTunnel(QString userName1, QString userName2);
	// 根据Client类型和可能可用的upnp端口来确定外部连接端口，
	quint16 getExternalTunnelPort(ClientInfo & client, quint16 upnpPort);
	// 根据和对面的内网/公网类型来获取Client最优的连接端口
	void getBetterTunnelAddressPort(ClientInfo & client, ClientInfo & peerClient, quint16 clientUpnpPort, QHostAddress * outAddress, quint16 * outPort);
	int getNextTunnelId();

	bool getTcpSocketAndClientInfoByUserName(QString userName, QTcpSocket ** outTcpSocket, ClientInfo ** outClientInfo);
	TunnelInfo * getTunnelInfo(int tunnelId);

	void clearUserTunnel(QString userName, QString reason);

	void dealTcpIn(QByteArray line, QTcpSocket & tcpSocket, ClientInfo & client);
	void dealUdpIn(int index, const QByteArray & line, QHostAddress hostAddress, quint16 port);

	void tcpIn_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client);
	void tcpOut_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client);

	void tcpOut_hello(QTcpSocket & tcpSocket, ClientInfo & client);

	void discardClient(QTcpSocket & tcpSocket, ClientInfo & client, QString reason);
	void tcpOut_discard(QTcpSocket & tcpSocket, ClientInfo & client, QString reason);

	void tcpIn_checkBinary(QTcpSocket & tcpSocket, ClientInfo & client, QString platform, QByteArray binaryChecksum);
	void tcpOut_checkBinary(QTcpSocket & tcpSocket, ClientInfo & client, bool correct, QString platform);

	void tcpIn_login(QTcpSocket & tcpSocket, ClientInfo & client, QString identifier, QString userName);
	void tcpOut_login(QTcpSocket & tcpSocket, ClientInfo & client, bool loginOk, QString userName, QString msg, quint16 serverUdpPort1 = 0, quint16 serverUdpPort2 = 0);
	bool login(QTcpSocket & tcpSocket, ClientInfo & client, QString identifier, QString userName, QString * outMsg);

	void tcpIn_localNetwork(QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress localAddress, quint16 clientUdp1LocalPort, QString gatewayInfo);

	void udpIn_checkNatStep1(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1);
	void tcpIn_checkNatStep1(QTcpSocket & tcpSocket, ClientInfo & client, int partlyType, quint16 clientUdp2LocalPort);

	void tcpIn_checkNatStep2Type1(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType);
	void udpIn_checkNatStep2Type2(int index, QTcpSocket & tcpSocket, ClientInfo & client, quint16 clientUdp1Port2);
	void tcpOut_checkNatStep2Type2(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType);

	void tcpIn_upnpAvailability(QTcpSocket & tcpSocket, ClientInfo & client, bool on);

	void udpIn_updateAddress(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1);

	void tcpIn_queryOnlineUser(QTcpSocket & tcpSocket, ClientInfo & client);
	void tcpOut_queryOnlineUser(QTcpSocket & tcpSocket, ClientInfo & client, QString onlineUser);

	void tcpIn_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName);
	void tcpOut_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);

	void tcpIn_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId);
	void tcpOut_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int requestId, int tunnelId, QString peerUserName);

	void tcpOut_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp);
	void tcpIn_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString);

	void tcpOut_tunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QHostAddress peerAddress, quint16 peerPort);

	void tcpIn_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason);
	void tcpOut_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason);

private:
	bool m_running;
	MessageConverter m_messageConverter;
	QTcpServer m_tcpServer;
	QUdpSocket m_udpServer1;
	QUdpSocket m_udpServer2;
	int m_lastTunnelId;
	UserContainer m_userContainer;
	QMap<QString, PlatformBinaryInfo> m_mapPlatformBinaryInfo;
	QMap<QTcpSocket*, ClientInfo> m_mapClientInfo;
	QMap<QString, QTcpSocket*> m_mapUserTcpSocket;
	QSet<QString> m_lstLoginedIdentifier;
	QMap<int, TunnelInfo> m_mapTunnelInfo;
	QTimer m_timer300ms;
	QTimer m_timer15s;
	QSet<QTcpSocket*> m_lstNeedSendUdp;
};
