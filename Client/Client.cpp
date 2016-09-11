#include "Client.h"
#include "Other.h"
#include "crc32.h"

static bool inline isValidIndex(int index)
{
	return index == 1 || index == 2;
}

Client::Client(QObject *parent)
	: QObject(parent)
{
	connect(&m_tcpSocket, SIGNAL(newConnection()), this, SLOT(onTcpNewConnection()));
	connect(&m_tcpSocket, SIGNAL(connected()), this, SLOT(onTcpConnected()));
	connect(&m_tcpSocket, SIGNAL(disconnected()), this, SLOT(onTcpDisconnected()));
	connect(&m_tcpSocket, SIGNAL(readyRead()), this, SLOT(onTcpReadyRead()));
	connect(&m_udpSocket1, SIGNAL(readyRead()), this, SLOT(onUdp1ReadyRead()));
	connect(&m_udpSocket2, SIGNAL(readyRead()), this, SLOT(onUdp2ReadyRead()));

	m_timer300ms.setParent(this);
	connect(&m_timer300ms, SIGNAL(timeout()), this, SLOT(timerFunction300ms()));
	m_timer300ms.start(300);

	m_timer15s.setParent(this);
	connect(&m_timer15s, SIGNAL(timeout()), this, SLOT(timerFunction15s()));
	m_timer15s.start(15 * 1000);
}

Client::~Client()
{

}

void Client::setUserInfo(QString userName, QString password)
{
	if (m_running)
		return;
	m_userName = userName;
	m_password = password;
}

void Client::setLocalPassword(QString localPassword)
{
	m_localPassword = localPassword;
}

void Client::setServerInfo(QHostAddress hostAddress, quint16 tcpPort)
{
	if (m_running)
		return;
	m_serverHostAddress = hostAddress;
	m_serverTcpPort = tcpPort;
}

bool Client::start()
{
	if (m_running)
		return false;

	m_running = true;

	startConnect();

	return true;
}

bool Client::stop()
{
	if (!m_running)
		return false;

	clear();

	m_running = false;
	return true;
}

void Client::tryTunneling(QString peerUserName)
{
	if (!m_running || !checkStatus(LoginedStatus, NatCheckFinished))
		return;
	tcpOut_tryTunneling(peerUserName);
}

void Client::readyTunneling(QString peerUserName, QString peerLocalPassword, int requestId)
{
	if (!m_running || !checkStatus(LoginedStatus, NatCheckFinished))
		return;
	tcpOut_readyTunneling(peerUserName, peerLocalPassword, requestId);
}

void Client::onTcpConnected()
{
	m_status = ConnectedStatus;
	m_lastInTime = QTime::currentTime();
	emit connected();
}

void Client::onTcpDisconnected()
{
	clear();
	emit disconnected();
}

void Client::onTcpReadyRead()
{
	QTcpSocket * tcpSocket = &m_tcpSocket;
	while (tcpSocket->canReadLine())
	{
		QByteArray line = tcpSocket->readLine();
		if (line.endsWith('\n'))
			line.chop(1);

		dealTcpIn(line);
	}
}

void Client::onUdp1ReadyRead()
{
	onUdpReadyRead(1);
}

void Client::onUdp2ReadyRead()
{
	onUdpReadyRead(2);
}

void Client::timerFunction300ms()
{
	if (!m_running)
		return;
	if (m_natStatus == Step1_1SendingToServer1)
	{
		QByteArrayMap argument;
		argument["userName"] = m_userName.toUtf8();
		QByteArray line = serializeResponse("checkNatStep1", argument);
		sendUdp(1, 1, line);
	}else if(m_natStatus == Step2_Type2_1SendingToServer2)
	{
		QByteArrayMap argument;
		argument["userName"] = m_userName.toUtf8();
		QByteArray line = serializeResponse("checkNatStep2Type2", argument);
		sendUdp(1, 2, line);
	}
	else if (m_natStatus == Step1_1WaitingForServer2)
	{
		if (m_beginWaitTime.isValid() && m_beginWaitTime.elapsed() > 3000)
			timeout_checkNatStep1();
	}
	else if (m_natStatus == Step2_Type1_2WaitingForServer1)
	{
		if (m_beginWaitTime.isValid() && m_beginWaitTime.elapsed() > 3000)
			timeout_checkNatStep2Type1();
	}
}

void Client::timerFunction15s()
{
	if (!m_running)
		return;

	if (m_tcpSocket.state() == QAbstractSocket::UnconnectedState)
		startConnect();

	const int inTimeout = (m_status == LoginedStatus && m_natStatus == NatCheckFinished) ? (120 * 1000) : (20 * 1000);
	if (m_lastInTime.elapsed() > inTimeout)
		m_tcpSocket.disconnectFromHost();
	else if (m_lastOutTime.elapsed() > 70 * 1000)
		tcpOut_heartbeat();
}

QUdpSocket * Client::getUdpSocket(int index)
{
	if (index == 1)
		return &m_udpSocket1;
	else if (index == 2)
		return &m_udpSocket2;
	else
		return nullptr;
}

quint16 Client::getServerUdpPort(int index)
{
	if (index == 1)
		return m_serverUdpPort1;
	else if (index == 2)
		return m_serverUdpPort2;
	else
		return 0;
}

int Client::getServerIndexFromUdpPort(quint16 serverUdpPort)
{
	if (serverUdpPort == m_serverUdpPort1)
		return 1;
	else if (serverUdpPort == m_serverUdpPort2)
		return 2;
	else
		return 0;
}

void Client::clear()
{
	m_tcpSocket.close();
	m_udpSocket1.close();
	m_udpSocket2.close();

	m_serverUdpPort1 = 0;
	m_serverUdpPort2 = 0;

	m_status = UnknownClientStatus;
	m_natStatus = UnknownNatCheckStatus;
	m_beginWaitTime = QTime();
	m_natType = UnknownNatType;

	m_lastInTime = QTime();
	m_lastOutTime = QTime();

	m_mapTunnelInfo.clear();
}

void Client::startConnect()
{
	clear();

	m_tcpSocket.connectToHost(m_serverHostAddress, m_serverTcpPort);
	m_udpSocket1.bind(0);
	m_udpSocket2.bind(0);
	m_status = ConnectingStatus;

	m_lastInTime = QTime::currentTime();
	m_lastOutTime = QTime::currentTime();
}

bool Client::checkStatus(ClientStatus correctStatus, NatCheckStatus correctNatStatus)
{
	return m_status == correctStatus && m_natStatus == correctNatStatus;
}

bool Client::checkStatusAndDisconnect(QString functionName, ClientStatus correctStatus, NatCheckStatus correctNatStatus)
{
	if (checkStatus(correctStatus, correctNatStatus))
	{
		return true;
	}
	else
	{
		disconnectServer(functionName + " status error");
		return false;
	}
}

void Client::disconnectServer(QString reason)
{
	m_tcpSocket.disconnectFromHost();

	qDebug() << QString("force disconnect, reason=%1").arg(reason);
}

void Client::sendUdp(int localIndex, int serverIndex, QByteArray package)
{
	if (!isValidIndex(localIndex) || !isValidIndex(serverIndex))
		return;
	QUdpSocket * udpSocket = getUdpSocket(localIndex);
	quint16 port = getServerUdpPort(serverIndex);

	uint32_t crc = crc32(package.constData(), package.size());
	package.insert(0, (const char*)&crc, 4);
	udpSocket->writeDatagram(package, m_serverHostAddress, port);
}

void Client::onUdpReadyRead(int localIndex)
{
	QUdpSocket * udpSocket = getUdpSocket(localIndex);
	while (udpSocket->hasPendingDatagrams())
	{
		char buffer[2000];
		QHostAddress hostAddress;
		quint16 port = 0;
		const int bufferSize = udpSocket->readDatagram(buffer, sizeof(buffer), &hostAddress, &port);
		if (!isSameHostAddress(hostAddress, m_serverHostAddress))
			continue;
		const int serverIndex = getServerIndexFromUdpPort(port);
		if(serverIndex == 0)
			continue;
		QByteArray package = checksumThenUnpackUdpPackage(QByteArray::fromRawData(buffer, bufferSize));
		if (package.isEmpty())
			continue;
		if (package.endsWith('\n'))
			package.chop(1);
		if (!package.contains('\n'))
			dealUdpIn(localIndex, serverIndex, package);
	}
}

void Client::dealTcpIn(QByteArray line)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	if (type == "heartbeat")
	{
		tcpIn_heartbeat();
	}
	else if (type == "NatTunnelv1")
	{
		tcpIn_hello();
	}
	else if(type == "login")
	{
		tcpIn_login(argument.value("loginOk").toInt() == 1, argument.value("msg"),
			argument.value("serverUdpPort1").toInt(), argument.value("serverUdpPort2").toInt());
	}
	else if (type == "tryTunneling")
	{
		tcpIn_tryTunneling(argument.value("peerUserName"), argument.value("canTunnel").toInt() == 1,
			argument.value("failReason"));
	}
	else if (type == "readyTunneling")
	{
		tcpIn_readyTunneling(argument.value("peerUserName"), argument.value("requestId").toInt(),
			argument.value("tunnelId").toInt());
	}
}

void Client::dealUdpIn(int localIndex, int serverIndex, const QByteArray & line)
{
	QByteArrayMap argument;
	QByteArray type = parseRequest(line, &argument);
	if (type.isEmpty())
		return;

	const QString userName = argument.value("userName");
	if (userName != m_userName)
		return;

	if (type == "checkNatStep1")
	{
		udpIn_checkNatStep1(localIndex, serverIndex);
	}
	else if (type == "checkNatStep2Type1")
	{
		udpIn_checkNatStep2Type1(localIndex, serverIndex);
	}
}

void Client::tcpOut_heartbeat()
{
	m_lastOutTime = QTime::currentTime();
	m_tcpSocket.write(serializeResponse("heartbeat", {}));
}

void Client::tcpIn_heartbeat()
{
	m_lastInTime = QTime::currentTime();
}

void Client::tcpIn_hello()
{
	if (!checkStatusAndDisconnect("tcpIn_hello", ConnectedStatus, UnknownNatCheckStatus))
		return;
	m_lastInTime = QTime::currentTime();
	tcpOut_login();
}

void Client::tcpOut_login()
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["userName"] = m_userName.toUtf8();
	argument["password"] = m_password.toUtf8();
	m_tcpSocket.write(serializeResponse("login", argument));

	m_status = LoginingStatus;
}

void Client::tcpIn_login(bool loginOk, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2)
{
	if (!checkStatusAndDisconnect("tcpIn_login", LoginingStatus, UnknownNatCheckStatus))
		return;
	m_lastInTime = QTime::currentTime();
	if (loginOk)
	{
		m_status = LoginedStatus;
		m_serverUdpPort1 = serverUdpPort1;
		m_serverUdpPort2 = serverUdpPort2;
		m_natStatus = Step1_1SendingToServer1;
		emit logined();
	}
	else
	{
		m_status = LoginFailedStatus;
		emit loginFailed(msg);
	}
}

void Client::udpIn_checkNatStep1(int localIndex, int serverIndex)
{
	if (localIndex != 1)
		return;
	if (m_status != LoginedStatus)
		return;
	if (m_natStatus != Step1_1SendingToServer1 && m_natStatus != Step1_1WaitingForServer2)
		return;
	m_lastInTime = QTime::currentTime();

	if (serverIndex == 1)
	{
		if (m_natStatus == Step1_1SendingToServer1)
		{
			m_natStatus = Step1_1WaitingForServer2;
			m_beginWaitTime = QTime::currentTime();
		}
	}
	else if(serverIndex == 2)
	{
		m_beginWaitTime = QTime();
		m_natStatus = Step2_Type1_12WaitingForServer1;
		m_beginWaitTime = QTime::currentTime();
		tcpOut_checkNatStep1(1, m_udpSocket2.localPort());
	}
}

void Client::timeout_checkNatStep1()
{
	m_natStatus = Step2_Type2_1SendingToServer2;
	tcpOut_checkNatStep1(2);
}

void Client::tcpOut_checkNatStep1(int partlyType, quint16 clientUdp2LocalPort)
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["partlyType"] = QByteArray::number(partlyType);
	argument["clientUdp2LocalPort"] = QByteArray::number(clientUdp2LocalPort);
	m_tcpSocket.write(serializeResponse("checkNatStep1", argument));
}

void Client::udpIn_checkNatStep2Type1(int localIndex, int serverIndex)
{
	if (serverIndex != 1)
		return;
	if (m_status != LoginedStatus)
		return;
	if (m_natStatus != Step2_Type1_12WaitingForServer1 && m_natStatus != Step2_Type1_2WaitingForServer1)
		return;
	m_lastInTime = QTime::currentTime();

	if(localIndex == 1)
	{ 
		if (m_natStatus == Step2_Type1_12WaitingForServer1)
		{
			m_natStatus = Step2_Type1_2WaitingForServer1;
			m_beginWaitTime = QTime::currentTime();
		}
	}
	else if (localIndex == 2)
	{
		m_beginWaitTime = QTime();
		m_natType = PublicNetwork;
		m_natStatus = NatCheckFinished;
		tcpOut_checkNatStep2Type1(m_natType);

		emit natTypeConfirmed(m_natType);
	}
}

void Client::timeout_checkNatStep2Type1()
{
	m_natType = FullOrRestrictedConeNat;
	m_natStatus = NatCheckFinished;
	tcpOut_checkNatStep2Type1(m_natType);

	emit natTypeConfirmed(m_natType);
}

void Client::tcpOut_checkNatStep2Type1(NatType natType)
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["natType"] = QByteArray::number((int)natType);
	m_tcpSocket.write(serializeResponse("checkNatStep2Type1", argument));
}

void Client::tcpIn_checkNatStep2Type2(NatType natType)
{
	if (!checkStatusAndDisconnect("tcpIn_checkNatStep2Type2", LoginedStatus, Step2_Type2_1SendingToServer2))
		return;
	if (natType != PortRestrictedConeNat && natType != SymmetricNat)
	{
		disconnectServer("tcpIn_checkNatStep2Type2 argument error");
		return;
	}
	m_lastInTime = QTime::currentTime();

	m_natType = natType;
	m_natStatus = NatCheckFinished;

	emit natTypeConfirmed(m_natType);
}

void Client::tcpOut_tryTunneling(QString peerUserName)
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	m_tcpSocket.write(serializeResponse("tryTunneling", argument));
}

void Client::tcpIn_tryTunneling(QString peerUserName, bool canTunnel, QString failReason)
{
	if (!checkStatusAndDisconnect("tcpIn_tryTunneling", LoginedStatus, NatCheckFinished))
		return;
	m_lastInTime = QTime::currentTime();

	emit onReplyTryTunneling(peerUserName, canTunnel, failReason);
}

void Client::tcpOut_readyTunneling(QString peerUserName, QString peerLocalPassword, int requestId)
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["peerLocalPassword"] = peerLocalPassword.toUtf8();
	argument["requestId"] = QByteArray::number(requestId);
	m_tcpSocket.write(serializeResponse("readyTunneling", argument));
}

void Client::tcpIn_readyTunneling(QString peerUserName, int requestId, int tunnelId)
{
	if (!checkStatusAndDisconnect("tcpIn_readyTunneling", LoginedStatus, NatCheckFinished))
		return;
	m_lastInTime = QTime::currentTime();

	if (tunnelId != 0)
	{
		if (m_mapTunnelInfo.contains(tunnelId))
		{
			disconnectServer(QString("tcpIn_readyTunneling duplicated tunnelId %1").arg(tunnelId));
			return;
		}
		TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];
		tunnel.status = ReadyTunnelingStatus;
	}

	emit onReplyReadTunneling(requestId, tunnelId);
}

void Client::tcpIn_startTunneling(int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerHostAddress, quint16 peerPort)
{
	if (!checkStatusAndDisconnect("tcpIn_startTunneling", LoginedStatus, NatCheckFinished))
		return;
	m_lastInTime = QTime::currentTime();

	if (localPassword == m_localPassword)
	{
		if (tunnelId == 0 || m_mapTunnelInfo.contains(tunnelId))
		{
			disconnectServer(QString("tcpIn_readyTunneling error or duplicated tunnelId %1").arg(tunnelId));
			return;
		}
		TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];
		tunnel.status = TunnelingStatus;
		tunnel.peerUserName = peerUserName;
		tunnel.peerHostAddress = tryConvertToIpv4(peerHostAddress);
		tunnel.peerPort = peerPort;

		if (peerPort)
		{

		}

		tcpOut_startTunneling(tunnelId, true);
	}else
	{
		tcpOut_startTunneling(tunnelId, false);
	}
}

void Client::tcpOut_startTunneling(int tunnelId, bool localPasswordCorrect)
{
	m_lastOutTime = QTime::currentTime();

	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["localPasswordCorrect"] = localPasswordCorrect ? "1" : "0";
	m_tcpSocket.write(serializeResponse("startTunneling", argument));
}
