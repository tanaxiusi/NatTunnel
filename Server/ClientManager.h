#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>

class ClientManager : public QObject
{
	Q_OBJECT

	enum ClientStatus
	{
		UnknownClientStatus = 0,
		ConnectedStatus,
		LoginedStatus
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
	enum NatType
	{
		UnknownNatType = 0,
		PublicNetwork,
		FullOrRestrictedConeNat,
		PortRestrictedConeNat,
		SymmetricNat
	};

	struct ClientInfo
	{
		ClientStatus status;
		QString userName;
		NatType natType = UnknownNatType;
		NatCheckStatus natStatus = UnknownNatCheckStatus;
		QHostAddress udpHostAddress;
		quint16 udp1Port1 = 0;
		quint16 udp2LocalPort = 0;
		QTime beginWaitTime;
	};

public:
	ClientManager(QObject *parent = 0);
	~ClientManager();

	void setUserList(QMap<QString, QString> mapUserPassword);

	bool start(quint16 tcpPort, quint16 udpPort1, quint16 udpPort2);
	bool stop();

private slots:
	void onTcpNewConnection();
	void onTcpDisconnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();

private:
	QUdpSocket * getUdpServer(int index);
	void disconnectClient(QTcpSocket & tcpSocket);
	void sendUdp(int index, QByteArray package, QHostAddress hostAddress, quint16 port);
	void onUdpReadyRead(int localIndex);

	void dealTcpIn(QByteArray line, QTcpSocket & tcpSocket, ClientInfo & client);
	void dealUdpIn(int index, const QByteArray & line, QHostAddress hostAddress, quint16 port);

	void tcpIn_login(QTcpSocket & tcpSocket, ClientInfo & client, QString userName, QString password);
	void tcpOut_login(QTcpSocket & tcpSocket, bool loginOk, QString msg, quint16 serverUdpPort1 = 0, quint16 serverUdpPort2 = 0);
	bool login(QTcpSocket & tcpSocket, ClientInfo & client, QString userName, QString password, QString * outMsg);

	void udpIn_checkNatStep1(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1);
	void tcpIn_checkNatStep1(QTcpSocket & tcpSocket, ClientInfo & client, int partlyType, quint16 clientUdp2LocalPort);

	void tcpIn_checkNatStep2Type1(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType);
	void udpIn_checkNatStep2Type2(int index, QTcpSocket & tcpSocket, ClientInfo & client, quint16 clientUdp1Port2);
	void tcpOut_checkNatStep2Type2(QTcpSocket & tcpSocket, NatType natType);


private:
	bool m_running = false;
	QTcpServer m_tcpServer;
	QUdpSocket m_udpServer1;
	QUdpSocket m_udpServer2;
	quint32 m_magicNumber = 0;
	QMap<QString, QString> m_mapUserPassword;
	QMap<QTcpSocket*, ClientInfo> m_mapClientInfo;
	QMap<QString, QTcpSocket*> m_mapUserTcpSocket;
	QTimer m_timer300ms;
	QSet<QTcpSocket*> m_lstNeedSendUdp;
};
