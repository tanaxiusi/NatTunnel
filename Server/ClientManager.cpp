#include "ClientManager.h"
#include <QtDebug>
#include <QCryptographicHash>
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

	m_lastTunnelId = 0;
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
		
		ClientInfo & client = m_mapClientInfo[tcpSocket];
		client.status = ConnectedStatus;
		client.lastInTime = QTime::currentTime();
		client.lastOutTime = QTime::currentTime();

		tcpOut_hello(*tcpSocket, client);

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
	clearUserTunnel(userName, U16("对方下线"));

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
			sendUdp(1, "checkNatStep1", argument, client.udpHostAddress, client.udp1Port1);
			sendUdp(2, "checkNatStep1", argument, client.udpHostAddress, client.udp1Port1);
		}
		else if (client.natStatus == Step2_Type1_1SendingToClient12)
		{
			sendUdp(1, "checkNatStep2Type1", argument, client.udpHostAddress, client.udp1Port1);
			sendUdp(1, "checkNatStep2Type1", argument, client.udpHostAddress, client.udp2LocalPort);
		}
	}
}

void ClientManager::timerFunction15s()
{
	if (!m_running)
		return;
	QList<QPair<QTcpSocket*, ClientInfo*>> timeoutSockets;
	for(auto iter = m_mapClientInfo.begin(); iter != m_mapClientInfo.end(); ++iter)
	{
		QTcpSocket * tcpSocket = iter.key();
		ClientInfo & client = iter.value();
		const int inTimeout = (client.status == LoginedStatus && client.natStatus == NatCheckFinished) ? (120 * 1000) : (30 * 1000);
		if (client.lastInTime.elapsed() > inTimeout)
			timeoutSockets << qMakePair(tcpSocket, &client);
		else if (client.lastOutTime.elapsed() > 70 * 1000)
			tcpOut_heartbeat(*tcpSocket, client);
	}

	for (QPair<QTcpSocket*, ClientInfo*> thePair: timeoutSockets)
		disconnectClient(*thePair.first, *thePair.second, "timeout");
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

bool ClientManager::checkStatus(QTcpSocket & tcpSocket, ClientInfo & client, ClientStatus correctStatus, NatCheckStatus correctNatStatus)
{
	return client.status == correctStatus && client.natStatus == correctNatStatus;
}

bool ClientManager::checkStatusAndDisconnect(QTcpSocket & tcpSocket, ClientInfo & client, QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus)
{
	if (checkStatus(tcpSocket, client, correctStatus, correctNatStatus))
	{
		return true;
	}
	else
	{
		disconnectClient(tcpSocket, client, functionName + " status error");
		return false;
	}
}

void ClientManager::disconnectClient(QTcpSocket & tcpSocket, ClientInfo & client, QString reason)
{
	qDebug() << QString("force disconnect %1, userName=%3, reason=%2")
		.arg(getSocketPeerDescription(&tcpSocket)).arg(client.userName).arg(reason);

	tcpSocket.disconnectFromHost();
}

void ClientManager::sendTcp(QTcpSocket & tcpSocket, ClientInfo & client, QByteArray type, QByteArrayMap argument)
{
	qDebug() << QString("%1 %2 tcpOut %3 %4")
		.arg(getSocketPeerDescription(&tcpSocket)).arg(client.userName)
		.arg((QString)type).arg(argumentToString(argument));
	client.lastOutTime = QTime::currentTime();
	tcpSocket.write(serializeResponse(type, argument));
}

void ClientManager::sendUdp(int index, QByteArray type, QByteArrayMap argument, QHostAddress hostAddress, quint16 port)
{
	QUdpSocket * udpServer = getUdpServer(index);
	if (!udpServer)
		return;

	QByteArray package = serializeResponse(type, argument);
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
		QByteArray package = checksumThenUnpackPackage(QByteArray::fromRawData(buffer, bufferSize));
		if (package.isEmpty())
			continue;
		hostAddress = tryConvertToIpv4(hostAddress);
		if (package.endsWith('\n'))
			package.chop(1);
		if (!package.contains('\n'))
			dealUdpIn(index, package, hostAddress, port);
	}
}

bool ClientManager::checkCanTunnel(ClientInfo & localClient, QString peerUserName, bool * outLocalNeedUpnp, bool * outPeerNeedUpnp, QString * outFailReason)
{
	bool dummy1, dummy2;
	QString dummy3;
	if (!outLocalNeedUpnp)
		outLocalNeedUpnp = &dummy1;
	if (!outPeerNeedUpnp)
		outPeerNeedUpnp = &dummy2;
	if (!outFailReason)
		outFailReason = &dummy3;
	*outLocalNeedUpnp = false;
	*outPeerNeedUpnp = false;
	*outFailReason = QString();

	if (peerUserName == localClient.userName)
	{
		*outFailReason = U16("不能连接自己");
		return false;
	}

	QTcpSocket * ptrPeerTcpSocket = nullptr;
	ClientInfo * ptrPeerClient = nullptr;
	if (!getTcpSocketAndClientInfoByUserName(peerUserName, &ptrPeerTcpSocket, &ptrPeerClient))
	{
		*outFailReason = U16("对方未上线");
		return false;
	}

	QTcpSocket & peerTcpSocket = *ptrPeerTcpSocket;
	ClientInfo & peerClient = *ptrPeerClient;
	const bool peerStatusCorrect = (peerClient.status == LoginedStatus && peerClient.natStatus == NatCheckFinished);
	if (!peerStatusCorrect)
	{
		*outFailReason = U16("对方还在初始化");
		return false;
	}
	
	const bool localNatPartlyTypeIs1 = (localClient.natType == PublicNetwork || localClient.natType == FullOrRestrictedConeNat);
	const bool peerNatPartlyTypeIs1 = (peerClient.natType == PublicNetwork || peerClient.natType == FullOrRestrictedConeNat);
	// 只要一边达到RestrictedConeNat等级，或者两边都是PortRestrictedConeNat，都可以无需upnp
	const bool unneedUpnp = (localNatPartlyTypeIs1 || peerNatPartlyTypeIs1 || (localClient.natType == PortRestrictedConeNat && peerClient.natType == PortRestrictedConeNat));
	const bool needUpnp = !unneedUpnp;

	if (needUpnp)
	{
		if (!localClient.upnpAvailable && !peerClient.upnpAvailable)
		{
			*outFailReason = U16("本地NAT类型=%1 对方NAT类型=%2 都不支持upnp 无法连接")
				.arg(getNatDescription(localClient.natType)).arg(getNatDescription(peerClient.natType));
			return false;
		}
		// 剩下的情况是PortRestrictedConeNat+SymmetricNat或者两边都是SymmetricNat
		if (localClient.upnpAvailable)
		{
			*outLocalNeedUpnp = true;
		}
		else if (peerClient.upnpAvailable)
		{
			// 这种情况下对面开upnp
			*outPeerNeedUpnp = true;
		}
	}

	if(isExistTunnel(localClient.userName, peerClient.userName))
	{
		*outFailReason = U16("已经存在到 %1 的连接").arg(peerUserName);
		return false;
	}
	return true;
}


bool ClientManager::isExistTunnel(QString userName1, QString userName2)
{
	for (auto iter = m_mapTunnelInfo.begin(); iter != m_mapTunnelInfo.end(); ++iter)
	{
		TunnelInfo & tunnel = iter.value();
		if(tunnel.clientAUserName == userName1 && tunnel.clientBUserName == userName2 ||
			tunnel.clientBUserName == userName1 && tunnel.clientAUserName == userName2)
		{
			return true;
		}
	}
	return false;
}


quint16 ClientManager::getTunnelPort(ClientInfo & client, quint16 orginalPort, quint16 upnpPort)
{
	// 如果开启了upnp，就用upnp映射的端口
	if (upnpPort)
		return upnpPort;
	// 如果是SymmetricNat，新端口号不能确定，返回0让对方客户端等待连接
	if (client.natType == SymmetricNat)
		return 0;
	// 其他情况下，用原端口
	return orginalPort;
}

int ClientManager::getNextTunnelId()
{
	return ++m_lastTunnelId;
}

bool ClientManager::getTcpSocketAndClientInfoByUserName(QString userName, QTcpSocket ** outTcpSocket, ClientInfo ** outClientInfo)
{
	*outTcpSocket = nullptr;
	*outClientInfo = nullptr;
	QTcpSocket * tcpSocket = m_mapUserTcpSocket.value(userName);
	if (!tcpSocket)
		return false;
	auto iter = m_mapClientInfo.find(tcpSocket);
	if (iter == m_mapClientInfo.end())
		return false;

	*outTcpSocket = tcpSocket;
	*outClientInfo = &(iter.value());
	return true;
}

ClientManager::TunnelInfo * ClientManager::getTunnelInfo(int tunnelId)
{
	auto iter = m_mapTunnelInfo.find(tunnelId);
	if (iter == m_mapTunnelInfo.end())
		return nullptr;
	return &(iter.value());
}

void ClientManager::clearUserTunnel(QString userName, QString reason)
{
	if (userName.isEmpty())
		return;

	for (auto iter = m_mapTunnelInfo.begin(); iter != m_mapTunnelInfo.end();)
	{
		const int tunnelId = iter.key();
		TunnelInfo & tunnel = iter.value();
		const bool isA = (tunnel.clientAUserName == userName);
		const bool isB = (tunnel.clientBUserName == userName);
		if (!isA && !isB)
		{
			++iter;
			continue;
		}

		const QString peerUserName = (isA ? tunnel.clientBUserName : tunnel.clientAUserName);

		QTcpSocket * ptrPeerTcpSocket = nullptr;
		ClientInfo * ptrPeerClient = nullptr;
		if(getTcpSocketAndClientInfoByUserName(peerUserName, &ptrPeerTcpSocket, &ptrPeerClient))
		{
			QTcpSocket & peerTcpSocket = *ptrPeerTcpSocket;
			ClientInfo & peerClient = *ptrPeerClient;
			tcpOut_closeTunneling(peerTcpSocket, peerClient, tunnelId, reason);
		}
		m_mapTunnelInfo.erase(iter++);
	}
}

void ClientManager::dealTcpIn(QByteArray line, QTcpSocket & tcpSocket, ClientInfo & client)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	qDebug() << QString("%1 %2 tcpIn %3 %4")
		.arg(getSocketPeerDescription(&tcpSocket)).arg(client.userName)
		.arg((QString)type).arg(argumentToString(argument));

	if (type == "heartbeat")
		tcpIn_heartbeat(tcpSocket, client);
	else if (type == "login")
		tcpIn_login(tcpSocket, client, argument.value("userName"), argument.value("password"));
	else if (type == "checkNatStep1")
		tcpIn_checkNatStep1(tcpSocket, client, argument.value("partlyType").toInt(), argument.value("clientUdp2LocalPort").toInt());
	else if (type == "checkNatStep2Type1")
		tcpIn_checkNatStep2Type1(tcpSocket, client, (NatType)argument.value("natType").toInt());
	else if (type == "upnpAvailability")
		tcpIn_upnpAvailability(tcpSocket, client, argument.value("on").toInt() == 1);
	else if (type == "tryTunneling")
		tcpIn_tryTunneling(tcpSocket, client, argument.value("peerUserName"));
	else if (type == "readyTunneling")
		tcpIn_readyTunneling(tcpSocket, client, argument.value("peerUserName"), argument.value("peerLocalPassword"),
			argument.value("udp2UpnpPort").toInt(), argument.value("requestId").toInt());
	else if (type == "startTunneling")
		tcpIn_startTunneling(tcpSocket, client, argument.value("tunnelId").toInt(), argument.value("canTunnel").toInt() == 1,
			argument.value("udp2UpnpPort").toInt(), argument.value("errorString"));
	else if (type == "closeTunneling")
		tcpIn_closeTunneling(tcpSocket, client, argument.value("tunnelId").toInt(), argument.value("reason"));
	else
		return;

	client.lastInTime = QTime::currentTime();
}

void ClientManager::dealUdpIn(int index, const QByteArray & line, QHostAddress hostAddress, quint16 port)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	const QString userName = argument.value("userName");
	const QByteArray passwordHash = argument.value("passwordHash");
	QTcpSocket * tcpSocket = m_mapUserTcpSocket.value(userName);
	if (!tcpSocket)
		return;
	auto iter = m_mapClientInfo.find(tcpSocket);
	if (iter == m_mapClientInfo.end())
		return;

	const QString password = m_mapUserPassword.value(userName);
	if (QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5).toHex() != passwordHash)
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
	else if (type == "updateAddress")
	{
		udpIn_updateAddress(index, *tcpSocket, client, hostAddress, port);
	}
}

void ClientManager::tcpIn_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client)
{
}

void ClientManager::tcpOut_heartbeat(QTcpSocket & tcpSocket, ClientInfo & client)
{
	sendTcp(tcpSocket, client, "heartbeat", {});
}

void ClientManager::tcpOut_hello(QTcpSocket & tcpSocket, ClientInfo & client)
{
	QByteArrayMap argument;
	argument["serverName"] = "NatTunnelv1";
	argument["clientAddress"] = tryConvertToIpv4(tcpSocket.peerAddress()).toString().toUtf8();

	sendTcp(tcpSocket, client, "hello", argument);
}

void ClientManager::tcpIn_login(QTcpSocket & tcpSocket, ClientInfo & client, QString userName, QString password)
{
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
	QByteArrayMap argument;
	argument["loginOk"] = loginOk ? "1" : "0";
	argument["msg"] = msg.toUtf8();
	if (loginOk)
	{
		argument["serverUdpPort1"] = QByteArray::number(serverUdpPort1);
		argument["serverUdpPort2"] = QByteArray::number(serverUdpPort2);
	}
	sendTcp(tcpSocket, client, "login", argument);
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

	client.udpHostAddress = clientUdp1HostAddress;
	client.udp1Port1 = clientUdp1Port1;
	client.natStatus = Step1_12SendingToClient1;

	m_lstNeedSendUdp.insert(&tcpSocket);

	qDebug() << QString("%1 checkNatStep1 udpHostAddress=%2 udp1Port1=%3, confirming Nat partlyType")
		.arg(client.userName).arg(client.udpHostAddress.toString()).arg(client.udp1Port1);
}

void ClientManager::tcpIn_checkNatStep1(QTcpSocket & tcpSocket, ClientInfo & client, int partlyType, quint16 clientUdp2LocalPort )
{
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_checkNatStep1", LoginedStatus, Step1_12SendingToClient1))
		return;
	if (partlyType != 1 && partlyType != 2)
	{
		disconnectClient(tcpSocket, client, "tcpIn_checkNatStep1 argument error");
		return;
	}

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
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_checkNatStep2Type1", LoginedStatus, Step2_Type1_1SendingToClient12))
		return;
	if (natType != PublicNetwork && natType != FullOrRestrictedConeNat)
	{
		disconnectClient(tcpSocket, client, "tcpIn_checkNatStep2Type1 argument error");
		return;
	}

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

	if (clientUdp1Port2 != client.udp1Port1)
		client.natType = SymmetricNat;
	else
		client.natType = PortRestrictedConeNat;
	client.natStatus = NatCheckFinished;
	tcpOut_checkNatStep2Type2(tcpSocket, client, client.natType);
	qDebug() << QString("%1 NatType=%2")
		.arg(client.userName).arg(getNatDescription(client.natType));
}

void ClientManager::tcpOut_checkNatStep2Type2(QTcpSocket & tcpSocket, ClientInfo & client, NatType natType)
{
	QByteArrayMap argument;
	argument["natType"] = QByteArray::number((int)natType);
	sendTcp(tcpSocket, client, "checkNatStep2Type2", argument);
}

void ClientManager::tcpIn_upnpAvailability(QTcpSocket & tcpSocket, ClientInfo & client, bool on)
{
	client.upnpAvailable = on;
}

void ClientManager::udpIn_updateAddress(int index, QTcpSocket & tcpSocket, ClientInfo & client, QHostAddress clientUdp1HostAddress, quint16 clientUdp1Port1)
{
	if (index != 1)
		return;
	if (client.status != LoginedStatus)
		return;
	if (client.natStatus != NatCheckFinished)
		return;

	if(client.udpHostAddress != clientUdp1HostAddress || client.udp1Port1 != clientUdp1Port1)
		qDebug() << QString("%1 udp updateAddress %2 %3")
			.arg(client.userName).arg(client.udpHostAddress.toString()).arg(client.udp1Port1);

	client.udpHostAddress = clientUdp1HostAddress;
	client.udp1Port1 = clientUdp1Port1;
}

void ClientManager::tcpIn_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName)
{
	// local = A, peer = B
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_tryTunneling", LoginedStatus, NatCheckFinished))
		return;

	bool canTunnel = false;
	bool needUpnp = false;
	QString failReason;

	canTunnel = checkCanTunnel(client, peerUserName, &needUpnp, nullptr, &failReason);
	tcpOut_tryTunneling(tcpSocket, client, peerUserName, canTunnel, needUpnp, failReason);
}

void ClientManager::tcpOut_tryTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["canTunnel"] = canTunnel ? "1" : "0";
	argument["needUpnp"] = needUpnp ? "1" : "0";
	argument["failReason"] = failReason.toUtf8();
	sendTcp(tcpSocket, client, "tryTunneling", argument);
}

void ClientManager::tcpIn_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId)
{
	// local = A, peer = B
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_readyTunneling", LoginedStatus, NatCheckFinished))
		return;

	bool peerNeedUpnp = false;

	if (checkCanTunnel(client, peerUserName, nullptr, &peerNeedUpnp, nullptr))
	{
		const int tunnelId = getNextTunnelId();
		TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];
		tunnel.clientAUserName = client.userName;
		tunnel.clientBUserName = peerUserName;
		tunnel.clientAUdp2UpnpPort = udp2UpnpPort;
		tunnel.status = ReadyTunnelingStatus;
		tcpOut_readyTunneling(tcpSocket, client, requestId, tunnelId, peerUserName);

		const quint16 tunnelPort = getTunnelPort(client, client.udp1Port1, udp2UpnpPort);

		QTcpSocket & peerTcpSocket = *m_mapUserTcpSocket.value(peerUserName);
		ClientInfo & peerClient = m_mapClientInfo[&peerTcpSocket];
		tcpOut_startTunneling(peerTcpSocket, peerClient, tunnelId, peerLocalPassword, client.userName, client.udpHostAddress,
			tunnelPort, peerNeedUpnp);
	}
	else
	{
		tcpOut_readyTunneling(tcpSocket, client, requestId, 0, peerUserName);
	}
}

void ClientManager::tcpOut_readyTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int requestId, int tunnelId, QString peerUserName)
{
	QByteArrayMap argument;
	argument["requestId"] = QByteArray::number(requestId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["peerUserName"] = peerUserName.toUtf8();
	sendTcp(tcpSocket, client, "readyTunneling", argument);
}

void ClientManager::tcpOut_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["localPassword"] = localPassword.toUtf8();
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["peerAddress"] = peerAddress.toString().toUtf8();
	argument["peerPort"] = QByteArray::number(peerPort);
	argument["needUpnp"] = needUpnp ? "1" : "0";
	sendTcp(tcpSocket, client, "startTunneling", argument);
}

void ClientManager::tcpIn_startTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString)
{
	// local = B, peer = A
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_startTunneling", LoginedStatus, NatCheckFinished))
		return;
	TunnelInfo & tunnel = *getTunnelInfo(tunnelId);
	if (&tunnel == nullptr)
	{
		// 如果在B响应前，A已经取消了连接，会出现找不到tunnelId的情况
		return;
	}

	QTcpSocket * ptrPeerTcpSocket = nullptr;
	ClientInfo * ptrPeerClient = nullptr;
	if (!getTcpSocketAndClientInfoByUserName(tunnel.clientAUserName, &ptrPeerTcpSocket, &ptrPeerClient))
	{
		// A找不到，可能下线了
		m_mapTunnelInfo.remove(tunnelId);
		tcpOut_closeTunneling(tcpSocket, client, tunnelId, U16("对方已经下线"));
		return;
	}

	QTcpSocket & peerTcpSocket = *ptrPeerTcpSocket;
	ClientInfo & peerClient = *ptrPeerClient;

	if (canTunnel)
	{
		tunnel.clientBUdp2UpnpPort = udp2UpnpPort;
		tunnel.status = TunnelingStatus;

		quint16 tunnelPort = getTunnelPort(client, client.udp1Port1, udp2UpnpPort);
		tcpOut_tunneling(peerTcpSocket, peerClient, tunnelId, client.udpHostAddress, tunnelPort);
	}else
	{
		m_mapTunnelInfo.remove(tunnelId);
		tcpOut_closeTunneling(peerTcpSocket, peerClient, tunnelId, errorString);
	}
}


void ClientManager::tcpOut_tunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QHostAddress peerAddress, quint16 peerPort)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["peerAddress"] = peerAddress.toString().toUtf8();
	argument["peerPort"] = QByteArray::number(peerPort);
	sendTcp(tcpSocket, client, "tunneling", argument);
}

void ClientManager::tcpIn_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason)
{
	if (!checkStatusAndDisconnect(tcpSocket, client, "tcpIn_closeTunneling", LoginedStatus, NatCheckFinished))
		return;

	TunnelInfo & tunnel = *getTunnelInfo(tunnelId);
	if (&tunnel == nullptr)
		return;

	QString peerUserName;
	if (client.userName == tunnel.clientAUserName)
	{
		peerUserName = tunnel.clientBUserName;
	}
	else if (client.userName == tunnel.clientBUserName)
	{
		peerUserName = tunnel.clientAUserName;
	}
	else
	{
		QString text = QString("try to close a tunnel(%2,%3,%4) not belong to it")
			.arg(tunnelId).arg(tunnel.clientAUserName).arg(tunnel.clientBUserName);
		disconnectClient(tcpSocket, client, text);
		return;
	}

	m_mapTunnelInfo.remove(tunnelId);

	tcpOut_closeTunneling(tcpSocket, client, tunnelId, reason);

	QTcpSocket * peerTcpSocket = nullptr;
	ClientInfo * peerClient = nullptr;
	if (!getTcpSocketAndClientInfoByUserName(peerUserName, &peerTcpSocket, &peerClient))
		return;

	tcpOut_closeTunneling(*peerTcpSocket, *peerClient, tunnelId, reason);
}

void ClientManager::tcpOut_closeTunneling(QTcpSocket & tcpSocket, ClientInfo & client, int tunnelId, QString reason)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["reason"] = reason.toUtf8();
	sendTcp(tcpSocket, client, "closeTunneling", argument);
}
