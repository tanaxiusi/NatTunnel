#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QTime>

class Client : public QObject
{
	Q_OBJECT

signals:
	void connected();
	void logined();
	void loginFailed(QString msg);

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

	enum NatType
	{
		UnknownNatType = 0,
		PublicNetwork,
		FullOrRestrictedConeNat,
		PortRestrictedConeNat,
		SymmetricNat
	};

public:
	Client(QObject *parent = 0);
	~Client();

	void setUserInfo(QString userName, QString password);
	void setServerInfo(QHostAddress hostAddress, quint16 tcpPort);

	bool start();
	bool stop();
	
private slots:
	void onTcpConnected();
	void onTcpReadyRead();
	void onUdp1ReadyRead();
	void onUdp2ReadyRead();
	void timerFunction300ms();

private:
	QUdpSocket * getUdpSocket(int index);
	quint16 getServerUdpPort(int index);
	int getServerIndexFromUdpPort(quint16 serverUdpPort);
	void sendUdp(int localIndex, int serverIndex, QByteArray package);
	void onUdpReadyRead(int localIndex);

	void dealTcpIn(QByteArray line);
	void dealUdpIn(int localIndex, int serverIndex, const QByteArray & line);

	void tcpIn_hello();

	void tcpOut_login();
	void tcpIn_login(bool loginOk, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2);

	void udpIn_checkNatStep1(int localIndex, int serverIndex);
	void timeout_checkNatStep1();
	void tcpOut_checkNatStep1(int partlyType, quint16 clientUdp2LocalPort = 0);

	void udpIn_checkNatStep2Type1(int localIndex, int serverIndex);
	void timeout_checkNatStep2Type1();
	void tcpOut_checkNatStep2Type1(NatType natType);

	void tcpIn_checkNatStep2Type2(NatType natType);

private:
	bool m_running = false;

	QTcpSocket m_tcpSocket;
	QUdpSocket m_udpSocket1;
	QUdpSocket m_udpSocket2;
	QTimer m_timer300ms;

	QString m_userName;
	QString m_password;

	QHostAddress m_serverHostAddress;
	quint16 m_serverTcpPort = 0;
	quint16 m_serverUdpPort1 = 0;
	quint16 m_serverUdpPort2 = 0;

	ClientStatus m_status = UnknownClientStatus;
	NatCheckStatus m_natStatus = UnknownNatCheckStatus;
	QTime m_beginWaitTime;
	NatType m_natType = UnknownNatType;
};
