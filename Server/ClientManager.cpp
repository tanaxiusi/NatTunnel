#include "ClientManager.h"
#include <QtDebug>
#include "Other.h"
#include "crc32.h"

ClientManager::ClientManager(QObject *parent)
	: QObject(parent)
{
	connect(&m_tcpServer, SIGNAL(newConnection()), this, SLOT(onTcpNewConnection()));
	connect(&m_udpServer1, SIGNAL(readyRead()), this, SLOT(onUdp1ReadyRead()));
	connect(&m_udpServer2, SIGNAL(readyRead()), this, SLOT(onUdp2ReadyRead()));

	m_timer300ms.setParent(this);
	connect(&m_timer300ms, SIGNAL(timeout()), this, SLOT(timerFunction300ms()));
	m_timer300ms.start(300);

	m_timer15s.setParent(this);
	connect(&m_timer15s, SIGNAL(timeout()), this, SLOT(timerFunction15s()));
	m_timer15s.start(15 * 1000);

}

ClientManager::~ClientManager()
{

}

void ClientManager::setUserList(QMap<QString, QString> mapUserPassword)
{
	if (m_running)
		return;
	m_mapUserPassword = mapUserPassword;
}

bool ClientManager::start(quint16 tcpPort, quint16 udpPort1, quint16 udpPort2)
{
	if (m_running)
		return false;

	const bool tcpOk = m_tcpServer.listen(QHostAddress::Any, tcpPort);
	const bool udp1Ok = m_udpServer1.bind(udpPort1);
	const bool udp2Ok = m_udpServer2.bind(udpPort2);

	if (!tcpOk || !udp1Ok || !udp2Ok)
	{
		m_tcpServer.close();
		m_udpServer1.close();
		m_udpServer2.close();
		return false;
	}

	m_running = true;
	return true;
}

bool ClientManager::stop()
{
	if (!m_running)
		return false;

	m_tcpServer.close();
	m_udpServer1.close();
	m_udpServer2.close();

	m_running = false;
	return true;
}

void ClientManager::onTcpNewConnection()
{
	while (m_tcpServer.hasPendingConnections())
	{
		QTcpSocket * tcpSocket = m_tcpServer.nextPendingConnection();
		if(!tcpSocket)
			break;

		connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(onTcpReadyRead()));
		connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(onTcpDisconnected()));
		connect(tcpSocket, SIGNAL(disconnected()), tcpSocket, SLOT(deleteLater()));

		tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
		tcpSocket->write("NatTunnelv1\n");

		ClientInfo & clientInfo = m_mapClientInfo[tcpSocket];
		clientInfo.status = ConnectedStatus;
		clientInfo.lastInTime = QTime::currentTime();
		clientInfo.lastOutTime = QTime::currentTime();

		qDebug() << QString("new connection : %1:%2")
			.arg(tryConvertToIpv4(tcpSocket->peerAddress()).toString()).arg(tcpSocket->peerPort());
	}
}


void ClientManager::onTcpDisconnected()
{
	QTcpSocket * tcpSocket = (QTcpSocket*)sender();
	if (!tcpSocket)
		return;
	
	const QString userName = m_mapClientInfo.value(tcpSocket).userName;
	if (userName.length() > 0)
		m_mapUserTcpSocket.remove(userName);
	m_mapClientInfo.remove(tcpSocket);
	m_lstNeedSendUdp.remove(tcpSocket);

	qDebug() << QString("disconnected : %1, logined userName = %2")
		.arg(getSocketPeerDescription(tcpSocket)).arg(userName);
}

void ClientManager::onTcpReadyRead()
{
	QTcpSocket * tcpSocket = (QTcpSocket*)sender();
	if (!tcpSocket)
		return;

	if (!tcpSocket->canReadLine())
		return;

	QMap<QTcpSocket*, ClientInfo>::iterator iter = m_mapClientInfo.find(tcpSocket);
	if (iter == m_mapClientInfo.end())
		return;
	ClientInfo & client = *iter;

	while (tcpSocket->canReadLine())
	{
		QByteArray line = tcpSocket->readLine();
		if (line.endsWith('\n'))
			line.chop(1);

		dealTcpIn(line, *tcpSocket, client);
	}
}

void ClientManager::onUdp1ReadyRead()
{
	onUdpReadyRead(1);
}

void ClientManager::onUdp2ReadyRead()
{
	onUdpReadyRead(2);
}

void ClientManager::timerFunction300ms()
{
	if (!m_running)
		return;
	for (QTcpSocket * tcpSocket : m_lstNeedSendUdp)
	{
		auto iter = m_mapClientInfo.find(tcpSocket);
		if(iter == m_mapClientInfo.end())
			continue;
		ClientInfo & client = *iter;
		QByteArrayMap argument;
		argument["userName"] = client.userName.toUtf8();
		if (client.natStatus == Step1_12SendingToClient1)
		{
			QByteArray line = serializeResponse("checkNatStep1", argument);
			sendUdp(1, line, client.udpHostAddress, client.udp1Port1);
			sendUdp(2, line, client.udpHostAddress, client.udp1Port1);
		}
		else if (client.natStatus == Step2_Type1_1SendingToClient12)
		{
			QByteArray line = serializeResponse("checkNatStep2Type1", argument);
			sendUdp(1, line, client.udpHostAddress, client.udp1Port1);
			sendUdp(1, line, client.udpHostAddress, client.udp2LocalPort);
		}
	}
}

void ClientManager::timerFunction15s()
{
	if (!m_running)
		return;
	QList<QTcpSocket*> timeoutSockets;
	for(auto iter = m_mapClientInfo.begin(); iter != m_mapClientInfo.end(); ++iter)
	{
		QTcpSocket * tcpSocket = iter.key();
		ClientInfo & client = iter.value();
		const int inTimeout = (client.status == LoginedStatus && client.natStatus == NatCheckFinished) ? (120 * 1000) : (20 * 1000);
		if (client.lastInTime.elapsed() > inTimeout)
			timeoutSockets << tcpSocket;
		else if (client.lastOutTime.elapsed() > 70 * 1000)
			tcpOut_heartbeat(*tcpSocket, client);
	}

	for (QTcpSocket * tcpSocket : timeoutSockets)
		disconnectClient(*tcpSocket, "timeout");
}

QUdpSocket * ClientManager::getUdpServer(int index)
{
	if (index == 1)
		return &m_udpServer1;
	else if (index == 2)
		return &m_udpServer2;
	else
		return nullptr;
}

void ClientManager::disconnectClient(QTcpSocket & tcpSocket, QString reason)
{
	tcpSocket.disconnectFromHost();

	qDebug() << QString("force disconnect %1, reason=%2").arg(getSocketPeerDescription(&tcpSocket)).arg(reason);
}

void ClientManager::sendUdp(int index, QByteArray package, QHostAddress hostAddress, quint16 port)
{
	QUdpSocket * udpServer = getUdpServer(index);
	if (!udpServer)
		return;

	uint32_t crc = crc32(package.constData(), package.size());
	package.insert(0, (const char*)&crc, 4);
	udpServer->writeDatagram(package, hostAddress, port);
}

void ClientManager::onUdpReadyRead(int index)
{
	QUdpSocket * udpServer = getUdpServer(index);
	while (udpServer->hasPendingDatagrams())
	{
		char buffer[2000];
		QHostAddress hostAddress;
		quint16 port = 0;
		const int bufferSize = udpServer->readDatagram(buffer, sizeof(buffer), &hostAddress, &port);
		QByteArray package = checksumThenUnpackUdpPackage(QByteArray::fromRawData(buffer, bufferSize));
		if (package.isEmpty())
			continue;
		if (package.endsWith('\n'))
			package.chop(1);
		if (!package.contains('\n'))
			dealUdpIn(index, package, hostAddress, port);
	}
}

void ClientManager::dealTcpIn(QByteArray line, QTcpSocket & tcpSocket, ClientInfo & client)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	if (type == "login")
	{
		tcpIn_login(tcpSocket, client, argument.value("userName"), argument.value("password"));
	}
	else if (type == "checkNatStep1")
	{
		tcpIn_checkNatStep1(tcpSocket, client, argument.value("partlyType").toInt(), argument.value("clientUdp2LocalPort").toInt());
	}
	else if (type == "checkNatStep2Type1")
	{
		tcpIn_checkNatStep2Type1(tcpSocket, client, (NatType)argument.value("natType").toInt());
	}
}

void ClientManager::dealUdpIn(int index, const QByteArray & line, QHostAddress hostAddress, quint16 port)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	const QString userName = argument.value("userName");
	QTcpSocket * tcpSocket = m_mapUserTcpSocket.value(userName);
	if (!tcpSocket)
		return;
	auto iter = m_mapClientInfo.find(tcpSocket);
	if (iter == m_mapClientInfo.end())
		return;
	ClientInfo & client = *iter;

	if (type == "checkNatStep1")
	{
		udpIn_checkNatStep1(index, *tcpSocket, client, hostAddress, port);
	}
	else if (type == "checkNatStep2Type2")
	{
		udpIn_checkNatStep2Type2(index, *tcpSocket, client, port);
	}
}

void ClientManager::tcpOut_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client)
{
	client.lastOutTime = QTime::currentTime();
	tcpSocket.write(serializeResponse("heartbeat", {}));
}


void ClientManager::tcpIn_login(QTcpSocket & tcpSocket, ClientInfo & client, QString userName, QString password)
{
	client.lastInTime = QTime::currentTime();

	QString msg;
	if (login(tcpSocket, client, userName, password, &msg))
	{
		tcpOut_login(tcpSocket, client, true, msg, m_udpServer1.localPort(), m_udpServer2.localPort());
		client.natStatus = Step1_1WaitingForClient1;
		qDebug() << QString("login ok, userName=%1, from %2").arg(userName).arg(getSocketPeerDescription(&tcpSocket));
	}
	else
	{
		tcpOut_login(tcpSocket, client, false, msg);
		qDebug() << QString("login failed, userName=%1, from %2, reason=%3")
			.arg(userName).arg(getSocketPeerDescription(&tcpSocket)).arg(msg);
	}
}

void ClientManager::tcpOut_login(QTcpSocket & tcpSocket, ClientInfo & client, bool loginOk, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2)
{
	client.lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["loginOk"] = loginOk ? "1" : "0";
	argument["msg"] = msg.toUtf8();
	if (loginOk)
	{
		argument["serverUdpPort1"] = QByteArray::number(serverUdpPort1);
		argument["serverUdpPort2"] = QByteArray::number(serverUdpPort2);
	}
	tcpSocket.write(serializeResponse("login", argument));
}

bool ClientManager::login(QTcpSocket & tcpSocket, ClientInfo & client, QString userName, QString password, QString * outMsg)
{
	if (client.status != ConnectedStatus)
	{
		*outMsg = U16("用户已经登录");
		return false;
	}
	if (m_mapUserTcpSocket.contains(userName))
	{
		*outMsg = U16("用户已经登录");
		return false;
	}
	if (m_mapUserPassword.value(userName) == password)
	{
		client.status = LoginedStatus;
		client.userName = userName;
		m_mapUserTcpSocket[userName] = &tcpSocket;
		*outMsg = U16("登录成功");
		return true;
	}
	else
	{
		*outMsg = U16("用户名与密码不匹配");
		return false;
	}
}

void ClientManager::udpIn_checkNatStep1(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1)
{
	if (index != 1)
		return;
	if (client.status != LoginedStatus)
		return;
	if (client.natStatus != Step1_1WaitingForClient1)
		return;

	client.lastInTime = QTime::currentTime();

	client.udpHostAddress = clientUdp1HostAddress;
	client.udp1Port1 = clientUdp1Port1;
	client.natStatus = Step1_12SendingToClient1;

	m_lstNeedSendUdp.insert(&tcpSocket);

	qDebug() << QString("%1 checkNatStep1 udp1Port1=%2, confirming Nat partlyType")
		.arg(client.userName).arg(client.udp1Port1);
}

void ClientManager::tcpIn_checkNatStep1(QTcpSocket & tcpSocket, ClientInfo & client, int partlyType, quint16 clientUdp2LocalPort )
{
	if (client.status != LoginedStatus || client.natStatus != Step1_12SendingToClient1)
	{
		disconnectClient(tcpSocket, "tcpIn_checkNatStep1 status error");
		return;
	}
	if (partlyType != 1 && partlyType != 2)
	{
		disconnectClient(tcpSocket, "tcpIn_checkNatStep1 argument error");
		return;
	}

	client.lastInTime = QTime::currentTime();

	m_lstNeedSendUdp.remove(&tcpSocket);
	if (partlyType == 1)
	{
		client.natStatus = Step2_Type1_1SendingToClient12;
		client.udp2LocalPort = clientUdp2LocalPort;
		m_lstNeedSendUdp.insert(&tcpSocket);
		qDebug() << QString("%1 Nat partlyType=1(PublicNetwork or FullOrRestrictedConeNat)")
			.arg(client.userName);
	}
	else if (partlyType == 2)
	{
		client.natStatus = Step2_Type2_2WaitingForClient1;
		qDebug() << QString("%1 Nat partlyType=2(PortRestrictedConeNat or SymmetricNat)")
			.arg(client.userName);
	}
}

void ClientManager::tcpIn_checkNatStep2Type1(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType)
{
	if (client.status != LoginedStatus || client.natStatus != Step2_Type1_1SendingToClient12)
	{
		disconnectClient(tcpSocket, "tcpIn_checkNatStep2Type1 status error");
		return;
	}
	if (natType != PublicNetwork && natType != FullOrRestrictedConeNat)
	{
		disconnectClient(tcpSocket, "tcpIn_checkNatStep2Type1 argument error");
		return;
	}

	client.lastInTime = QTime::currentTime();

	m_lstNeedSendUdp.remove(&tcpSocket);
	
	client.natType = natType;
	client.natStatus = NatCheckFinished;
	qDebug() << QString("%1 NatType=%2").arg(client.userName)
		.arg(getNatDescription(client.natType));
}

void ClientManager::udpIn_checkNatStep2Type2(int index, QTcpSocket & tcpSocket, ClientInfo & client, quint16 clientUdp1Port2)
{
	if (index != 2)
		return;
	if (client.natStatus != Step2_Type2_2WaitingForClient1)
		return;

	client.lastInTime = QTime::currentTime();

	if (clientUdp1Port2 != client.udp1Port1)
		client.natType = SymmetricNat;
	else
		client.natType = PortRestrictedConeNat;
	client.natStatus = NatCheckFinished;
	tcpOut_checkNatStep2Type2(tcpSocket, client, client.natType);
	qDebug() << QString("%1 NatType=%2").arg(client.userName)
		.arg(getNatDescription(client.natType));
}

void ClientManager::tcpOut_checkNatStep2Type2(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType)
{
	client.lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["natType"] = QByteArray::number((int)natType);
	tcpSocket.write(serializeResponse("checkNatStep2Type2", argument));
}
