#include "Client.h"
#include <QTcpServer>
#include <QFile>
#include <QCoreApplication>
#include <QCryptographicHash>
#include "Util/Other.h"
#include "crc32/crc32.h"

static bool inline isValidIndex(int index)
{
	return index == 1 || index == 2;
}

Client::Client(QObject *parent)
	: QObject(parent)
{
	m_running = false;
	m_discarded = false;
	m_serverTcpPort = 0;
	m_serverUdpPort1 = 0;
	m_serverUdpPort2 = 0;
	m_status = UnknownClientStatus;
	m_natStatus = UnknownNatCheckStatus;
	m_natType = UnknownNatType;
	m_upnpStatus = UpnpUnknownStatus;
	m_upnpAvailability = false;
	m_isPublicNetwork = false;
	m_udp2UpnpPort = 0;

	m_tcpSocket.setParent(this);
	m_udpSocket1.setParent(this);
	m_udpSocket2.setParent(this);
	m_timer300ms.setParent(this);
	m_timer10s.setParent(this);
	m_kcpManager.setParent(this);
	m_upnpPortMapper.setParent(this);

	connect(&m_tcpSocket, SIGNAL(connected()), this, SLOT(onTcpConnected()));
	connect(&m_tcpSocket, SIGNAL(disconnected()), this, SLOT(onTcpDisconnected()));
	connect(&m_tcpSocket, SIGNAL(readyRead()), this, SLOT(onTcpReadyRead()));
	connect(&m_udpSocket1, SIGNAL(readyRead()), this, SLOT(onUdp1ReadyRead()));
	connect(&m_udpSocket2, SIGNAL(readyRead()), this, SLOT(onUdp2ReadyRead()));
	connect(&m_kcpManager, SIGNAL(wannaLowLevelOutput(int, QHostAddress, quint16, QByteArray)),
		this, SLOT(onKcpLowLevelOutput(int, QHostAddress, quint16, QByteArray)));
	connect(&m_kcpManager, SIGNAL(highLevelOutput(int, QByteArray)), this,
		SLOT(onKcpHighLevelOutput(int, QByteArray)));
	connect(&m_kcpManager, SIGNAL(handShaked(int)), this, SLOT(onKcpConnectionHandShaked(int)));
	connect(&m_kcpManager, SIGNAL(disconnected(int, QString)), this, SLOT(onKcpConnectionDisconnected(int, QString)));
	connect(&m_upnpPortMapper, SIGNAL(discoverFinished(bool)),
		this, SLOT(onUpnpDiscoverFinished(bool)));
	connect(&m_upnpPortMapper, SIGNAL(queryExternalAddressFinished(QHostAddress, bool, QString)),
		this, SLOT(onUpnpQueryExternalAddressFinished(QHostAddress, bool, QString)));

	m_timer300ms.setParent(this);
	connect(&m_timer300ms, SIGNAL(timeout()), this, SLOT(timerFunction300ms()));
	m_timer300ms.start(300);

	m_timer10s.setParent(this);
	connect(&m_timer10s, SIGNAL(timeout()), this, SLOT(timerFunction10s()));
	m_timer10s.start(10 * 1000);
}

Client::~Client()
{
	stop();
}

void Client::setConfig(QByteArray globalKey, QString randomIdentifierSuffix, QHostAddress serverHostAddress, quint16 serverTcpPort)
{
	if (m_running)
		return;
	if (globalKey.size() < 16)
		globalKey.append(QByteArray(16 - globalKey.size(), 0));
	m_messageConverter.setKey((const quint8*)globalKey.constData());
	m_randomIdentifierSuffix = randomIdentifierSuffix;
	m_serverHostAddress = tryConvertToIpv4(serverHostAddress);
	m_serverTcpPort = serverTcpPort;
}

void Client::setUserName(QString userName)
{
	if (m_running && (m_status == LoginingStatus || m_status == LoginedStatus))
		return;
	m_userName = userName;
	m_kcpManager.setUserName(userName);
}

void Client::setLocalPassword(QString localPassword)
{
	m_localPassword = localPassword;
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

bool Client::isDiscarded()
{
	if (!m_running)
		return false;
	return m_discarded;
}

void Client::reconnect()
{
	if (!m_running)
		return;
	if (m_discarded)
		m_discarded = false;
	startConnect();
}


bool Client::tryLogin()
{
	if (!m_running)
		return false;
	if (m_status != BinaryCheckedStatus && m_status != LoginFailedStatus)
		return false;
	if(m_userName.isEmpty())
		emit loginFailed(m_userName, U16("用户名为空"));
	else
		tcpOut_login(m_identifier, m_userName);
	return true;
}

QHostAddress Client::getLocalAddress()
{
	if (!m_running)
		return QHostAddress();
	return tryConvertToIpv4(m_tcpSocket.localAddress());
}

QHostAddress Client::getLocalPublicAddress()
{
	if (!m_running)
		return QHostAddress();
	return m_localPublicAddress;
}

void Client::setUpnpAvailable(bool upnpAvailability)
{
	if (!m_running)
		return;
	m_upnpAvailability = upnpAvailability;
	tcpOut_upnpAvailability(upnpAvailability);
}

void Client::queryOnlineUser()
{
	if (!m_running)
		return;
	tcpOut_queryOnlineUser();
}

void Client::tryTunneling(QString peerUserName)
{
	if (!m_running || !checkStatus(LoginedStatus, NatCheckFinished))
		return;
	if (peerUserName == m_userName)
	{
		emit replyTryTunneling(peerUserName, false, false, U16("不能连接自己"));
		return;
	}
	if (m_kcpManager.haveUnconfirmedKcpConnection())
	{
		emit replyTryTunneling(peerUserName, false, false, U16("与其他用户尝试连接中"));
		return;
	}
	udpOut_updateAddress();
	tcpOut_tryTunneling(peerUserName);
}

int Client::readyTunneling(QString peerUserName, QString peerLocalPassword, bool useUpnp)
{
	if (!m_running || !checkStatus(LoginedStatus, NatCheckFinished))
		return 0;
	int requestId = qrand() * 2;
	// 使用upnp时，requestId为奇数，在返回的时候可以作区分
	if (useUpnp)
		requestId++;
	if(useUpnp)
		addUpnpPortMapping();
	udpOut_updateAddress();
	tcpOut_readyTunneling(peerUserName, peerLocalPassword, useUpnp ? m_udp2UpnpPort : 0, requestId);
	m_waitingRequestId.insert(requestId);
	return requestId;
}

void Client::closeTunneling(int tunnelId)
{
	tcpOut_closeTunneling(tunnelId, U16("主动关闭"));
}

int Client::tunnelWrite(int tunnelId, QByteArray package)
{
	if (!m_running)
		return -2;
	return m_kcpManager.highLevelInput(tunnelId, package);
}

void Client::onTcpConnected()
{
	m_status = ConnectedStatus;
	m_lastInTime = QTime::currentTime();

	const QHostAddress localAddress = getLocalAddress();
	if (isNatAddress(localAddress))
	{
		m_upnpPortMapper.close();
		m_upnpPortMapper.open(localAddress);
		m_upnpPortMapper.discover();
		m_upnpStatus = UpnpDiscovering;

		const QString localAddressText = localAddress.toString();
		foreach (QString gatewayAddress, getGatewayAddress(localAddressText))
		{
			const QString gatewayHardwareAddress = arpGetHardwareAddress(gatewayAddress, localAddressText);
			m_lstGatewayInfo.append(gatewayAddress + " " + gatewayHardwareAddress);
		}
		m_lstGatewayInfo.sort();
	}
	else
	{
		m_upnpStatus = UpnpUnneeded;
	}

	emit connected();
	emit upnpStatusChanged(m_upnpStatus);
}

void Client::onTcpDisconnected()
{
	if (!m_running)
		return;
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
		argument["identifier"] = m_identifier.toUtf8();
		sendUdp(1, 1, "checkNatStep1", argument);
	}else if(m_natStatus == Step2_Type2_1SendingToServer2)
	{
		QByteArrayMap argument;
		argument["userName"] = m_userName.toUtf8();
		argument["identifier"] = m_identifier.toUtf8();
		sendUdp(1, 2, "checkNatStep2Type2", argument);
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

void Client::timerFunction10s()
{
	if (!m_running)
		return;
	if (m_discarded)
		return;

	if (m_tcpSocket.state() == QAbstractSocket::UnconnectedState)
		startConnect();

	if (m_status == LoginedStatus && m_natStatus == NatCheckFinished)
	{
		udpOut_updateAddress();
	}

	const int inTimeout = (m_status == LoginedStatus && m_natStatus == NatCheckFinished) ? (120 * 1000) : (20 * 1000);
	if (m_lastInTime.elapsed() > inTimeout)
		m_tcpSocket.disconnectFromHost();
	else if (m_lastOutTime.elapsed() > 70 * 1000)
		tcpOut_heartbeat();
}

void Client::onKcpLowLevelOutput(int tunnelId, QHostAddress hostAddress, quint16 port, QByteArray package)
{
	QMap<int, TunnelInfo>::iterator iter = m_mapTunnelInfo.find(tunnelId);
	if (iter == m_mapTunnelInfo.end())
		return;
	TunnelInfo & tunnel = iter.value();
	QUdpSocket * udpSocket = getUdpSocket(tunnel.localIndex);
	if (!udpSocket)
		return;

	udpSocket->writeDatagram(package, hostAddress, port);
}

void Client::onKcpHighLevelOutput(int tunnelId, QByteArray package)
{
	emit tunnelData(tunnelId, package);
}

void Client::onKcpConnectionHandShaked(int tunnelId)
{
	tcpOut_tunnelHandeShaked(tunnelId);
	emit tunnelHandShaked(tunnelId);
}

void Client::onKcpConnectionDisconnected(int tunnelId, QString reason)
{
	QMap<int, TunnelInfo>::iterator iter = m_mapTunnelInfo.find(tunnelId);
	if (iter == m_mapTunnelInfo.end())
		return;

	m_mapTunnelInfo.erase(iter);

	tcpOut_closeTunneling(tunnelId, U16("等待超时"));
}

void Client::onUpnpDiscoverFinished(bool ok)
{
	if (ok)
	{
		m_upnpPortMapper.queryExternalAddress();
		m_upnpStatus = UpnpQueryingExternalAddress;
	}
	else
	{
		m_upnpStatus = UpnpFailed;
	}
	emit upnpStatusChanged(m_upnpStatus);
}

void Client::onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString)
{
	if (ok)
	{
		if (isNatAddress(address))
		{
			m_upnpStatus = UpnpFailed;
			emit warning(U16("Upnp返回的地址 %2 仍然是内网地址").arg(address.toString()));
		}
		else
		{
			m_upnpStatus = UpnpOk;
			setUpnpAvailable(true);
			const QHostAddress localPublicAddress = getLocalPublicAddress();
			if (!localPublicAddress.isNull() && !isSameHostAddress(address, localPublicAddress))
				emit warning(U16("服务器端返回的IP地址 %1 和upnp返回的地址 %2 不同").arg(getLocalPublicAddress().toString()).arg(address.toString()));
		}
	}
	else
	{
		m_upnpStatus = UpnpFailed;
	}
	emit upnpStatusChanged(m_upnpStatus);
}

QUdpSocket * Client::getUdpSocket(int index)
{
	if (index == 1)
		return &m_udpSocket1;
	else if (index == 2)
		return &m_udpSocket2;
	else
		return NULL;
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
	m_kcpManager.clear();

	m_tcpSocket.close();
	m_udpSocket1.close();
	m_udpSocket2.close();

	deleteUpnpPortMapping();

	m_serverUdpPort1 = 0;
	m_serverUdpPort2 = 0;

	m_status = UnknownClientStatus;
	m_natStatus = UnknownNatCheckStatus;
	m_beginWaitTime = QTime();
	m_natType = UnknownNatType;

	m_upnpStatus = UpnpUnknownStatus;

	m_lastInTime = QTime();
	m_lastOutTime = QTime();

	m_upnpAvailability = false;
	m_isPublicNetwork = false;
	m_localPublicAddress = QHostAddress();
	m_lstGatewayInfo.clear();
	m_udp2UpnpPort = 0;

	m_waitingRequestId.clear();
	m_mapTunnelInfo.clear();
	m_mapHostTunnelId.clear();
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

bool Client::refreshIdentifier()
{
	if (m_status == UnknownClientStatus || m_status == ConnectingStatus)
		return false;
	QString hardwareAddress = getNetworkInterfaceHardwareAddress(getLocalAddress()).toLower();
	hardwareAddress.remove('-');
	if (hardwareAddress.isEmpty())
		hardwareAddress = "000000000000";
	m_identifier = hardwareAddress + "-" + m_randomIdentifierSuffix;
	return true;
}

bool Client::checkStatus(ClientStatus correctStatus, NatCheckStatus correctNatStatus)
{
	return m_status == correctStatus && m_natStatus == correctNatStatus;
}

bool Client::checkStatus(ClientStatus correctStatus)
{
	return m_status == correctStatus;
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

bool Client::checkStatusAndDisconnect(QString functionName, ClientStatus correctStatus)
{
	if (checkStatus(correctStatus))
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

void Client::sendTcp(QByteArray type, QByteArrayMap argument)
{
	qDebug() << QString("tcpOut %1 %2").arg((QString)type).arg(m_messageConverter.argumentToString(argument));
	m_lastOutTime = QTime::currentTime();
	m_tcpSocket.write(m_messageConverter.serialize(type, argument));
}

void Client::sendUdp(int localIndex, int serverIndex, QByteArray package)
{
	if (!isValidIndex(localIndex) || !isValidIndex(serverIndex))
		return;
	QUdpSocket * udpSocket = getUdpSocket(localIndex);
	quint16 port = getServerUdpPort(serverIndex);

	udpSocket->writeDatagram(addChecksumInfo(package), m_serverHostAddress, port);
}

void Client::sendUdp(int localIndex, int serverIndex, QByteArray type, QByteArrayMap argument)
{
	sendUdp(localIndex, serverIndex, m_messageConverter.serialize(type, argument));
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
		hostAddress = tryConvertToIpv4(hostAddress);
		const bool isServerAddress = (hostAddress == m_serverHostAddress);
		const int serverIndex = isServerAddress ? getServerIndexFromUdpPort(port) : 0;
		if (serverIndex != 0)
		{
			QByteArray package = checksumThenUnpackPackage(QByteArray::fromRawData(buffer, bufferSize));
			if (package.isEmpty())
				continue;
			if (package.endsWith('\n'))
				package.chop(1);
			if (!package.contains('\n'))
				dealUdpIn_server(localIndex, serverIndex, package);
		}
		else
		{
			dealUdpIn_p2p(localIndex, hostAddress, port, QByteArray::fromRawData(buffer, bufferSize));
		}
	}
}

void Client::dealTcpIn(QByteArray line)
{
	QByteArrayMap argument;
	QByteArray type = m_messageConverter.parse(line, &argument);
	if (type.isEmpty())
	{
		disconnectServer("dealTcpIn invalid data format");
		return;
	}

	qDebug() << QString("tcpIn %1 %2").arg((QString)type).arg(m_messageConverter.argumentToString(argument));

	if (type == "heartbeat")
		tcpIn_heartbeat();
	else if (type == "discard")
		tcpIn_discard(argument.value("reason"));
	else if (type == "checkBinary")
		tcpIn_checkBinary(argument.value("correct").toInt() == 1, argument.value("compressedBinary"));
	else if (type == "hello")
		tcpIn_hello(argument.value("serverName"), QHostAddress((QString)argument.value("clientAddress")));
	else if (type == "login")
		tcpIn_login(argument.value("loginOk").toInt() == 1, argument.value("userName"), argument.value("msg"),
			argument.value("serverUdpPort1").toInt(), argument.value("serverUdpPort2").toInt());
	else if (type == "checkNatStep2Type2")
		tcpIn_checkNatStep2Type2((NatType)argument.value("natType").toInt());
	else if (type == "queryOnlineUser")
		tcpIn_queryOnlineUser(argument.value("onlineUser"));
	else if (type == "tryTunneling")
		tcpIn_tryTunneling(argument.value("peerUserName"), argument.value("canTunnel").toInt() == 1,
			argument.value("needUpnp").toInt() == 1, argument.value("failReason"));
	else if (type == "readyTunneling")
		tcpIn_readyTunneling(argument.value("peerUserName"), argument.value("requestId").toInt(),
			argument.value("tunnelId").toInt());
	else if (type == "startTunneling")
		tcpIn_startTunneling(argument.value("tunnelId").toInt(), argument.value("localPassword"),
			argument.value("peerUserName"), QHostAddress((QString)argument.value("peerAddress")),
			argument.value("peerPort").toInt(), argument.value("needUpnp").toInt() == 1);
	else if (type == "tunneling")
		tcpIn_tunneling(argument.value("tunnelId").toInt(), QHostAddress((QString)argument.value("peerAddress")),
			argument.value("peerPort").toInt());
	else if (type == "closeTunneling")
		tcpIn_closeTunneling(argument.value("tunnelId").toInt(), argument.value("reason"));
	else
		return;

	m_lastInTime = QTime::currentTime();
}

void Client::dealUdpIn_server(int localIndex, int serverIndex, const QByteArray & line)
{
	QByteArrayMap argument;
	QByteArray type = m_messageConverter.parse(line, &argument);
	if (type.isEmpty())
		return;

	const QString userName = argument.value("userName");
	if (userName != m_userName)
		return;

	if (type == "checkNatStep1")
		udpIn_checkNatStep1(localIndex, serverIndex);
	else if (type == "checkNatStep2Type1")
		udpIn_checkNatStep2Type1(localIndex, serverIndex);
}

void Client::dealUdpIn_p2p(int localIndex, QHostAddress peerAddress, quint16 peerPort, const QByteArray & package)
{
	m_kcpManager.lowLevelInput(peerAddress, peerPort, package);
}

void Client::checkFirewall()
{
	// 如果IP检测发现是公网，但是流程走下来得出的结论不是，说明其中的udp包被防火墙拦截了
	if (m_isPublicNetwork && m_natType != PublicNetwork)
		emit warning(U16("当前处于公网环境，但是防火墙可能拦截了本程序"));
}

void Client::addUpnpPortMapping()
{
	if (m_udp2UpnpPort == 0)
	{
		const quint16 internalPort = m_udpSocket2.localPort();
		const quint16 externalPort = (rand() & 0x7FFF) + 25000;
		m_upnpPortMapper.addPortMapping(QAbstractSocket::UdpSocket, m_upnpPortMapper.localAddress(), internalPort, externalPort, "NatTunnelClient");
		m_udp2UpnpPort = externalPort;
	}
}

void Client::deleteUpnpPortMapping()
{
	if (m_udp2UpnpPort != 0)
	{
		const quint16 externalPort = m_udp2UpnpPort;
		m_upnpPortMapper.deletePortMapping(QAbstractSocket::UdpSocket, externalPort);
	}
}

quint32 Client::getKcpMagicNumber(QString peerUserName)
{
	// 确保不同用户间连接的kcp特征码不同
	QStringList lst = QStringList() << m_userName << peerUserName;
	if (lst[0] > lst[1])
		qSwap(lst[0], lst[1]);
	QByteArray buffer = lst.join("\n").toUtf8();
	return crc32(buffer.constData(), buffer.size());
}

void Client::createKcpConnection(int tunnelId, TunnelInfo & tunnel)
{
	if (tunnel.peerPort == 0 && m_natType == FullOrRestrictedConeNat)
	{
		// peerPort==0说明对方是SymmetricNat，如果自己是RestrictedConeNat，需要先发个包给对方IP随机端口
		QUdpSocket * udpSocket = getUdpSocket(tunnel.localIndex);
		if (udpSocket)
			udpSocket->writeDatagram("~", tunnel.peerAddress, 25000 + rand());
	}
	m_kcpManager.createKcpConnection(tunnelId, tunnel.peerAddress, tunnel.peerPort,
		tunnel.peerUserName, getKcpMagicNumber(tunnel.peerUserName));
}

void Client::tcpOut_heartbeat()
{
	sendTcp("heartbeat", QByteArrayMap());
}

void Client::tcpIn_heartbeat()
{
}

void Client::tcpIn_discard(QString reason)
{
	m_discarded = true;
	emit discarded(reason);
	disconnectServer("have discarded");
}

void Client::tcpIn_hello(QString serverName, QHostAddress clientAddress)
{
	if (!checkStatusAndDisconnect("tcpIn_hello", ConnectedStatus, UnknownNatCheckStatus))
		return;
	if (isSameHostAddress(clientAddress, m_tcpSocket.localAddress()))
		m_isPublicNetwork = true;
	m_localPublicAddress = clientAddress;
	refreshIdentifier();

#if defined(Q_OS_WIN)
	const QString platform = "windows";
#else defined(Q_OS_LINUX)
	const QString platform = "linux";
#endif


	const QByteArray binaryChecksum = QCryptographicHash::hash(readFile(QCoreApplication::applicationFilePath()), QCryptographicHash::Sha1);

	tcpOut_checkBinary(platform, binaryChecksum);
}

void Client::tcpOut_checkBinary(QString platform, QByteArray binaryChecksum)
{
	QByteArrayMap argument;
	argument["platform"] = platform.toUtf8();
	argument["binaryChecksum"] = binaryChecksum;
	sendTcp("checkBinary", argument);

	m_status = BinaryCheckingStatus;
}

void Client::tcpIn_checkBinary(bool correct, QByteArray compressedBinary)
{
	if (!checkStatusAndDisconnect("tcpIn_checkBinary", BinaryCheckingStatus))
		return;
	m_status = BinaryCheckedStatus;
	if (correct)
		tryLogin();
	else
		emit binaryError(qUncompress(compressedBinary));
}

void Client::tcpOut_login(QString identifier, QString userName)
{
	QByteArrayMap argument;
	argument["identifier"] = identifier.toUtf8();
	argument["userName"] = userName.toUtf8();
	sendTcp("login", argument);

	m_status = LoginingStatus;
}

void Client::tcpIn_login(bool loginOk, QString userName, QString msg, quint16 serverUdpPort1, quint16 serverUdpPort2)
{
	if (!checkStatusAndDisconnect("tcpIn_login", LoginingStatus, UnknownNatCheckStatus))
		return;
	if (loginOk)
	{
		m_status = LoginedStatus;
		m_serverUdpPort1 = serverUdpPort1;
		m_serverUdpPort2 = serverUdpPort2;
		m_natStatus = Step1_1SendingToServer1;

		tcpOut_localNetwork(getLocalAddress(), m_udpSocket1.localPort(), m_lstGatewayInfo.join(" "));;

		emit logined();
	}
	else
	{
		m_status = LoginFailedStatus;
		emit loginFailed(userName, msg);
	}
}

void Client::tcpOut_localNetwork(QHostAddress localAddress, quint16 clientUdp1LocalPort, QString gatewayInfo)
{
	QByteArrayMap argument;
	argument["localAddress"] = localAddress.toString().toUtf8();
	argument["clientUdp1LocalPort"] = QByteArray::number(clientUdp1LocalPort);
	argument["gatewayInfo"] = gatewayInfo.toUtf8();
	sendTcp("localNetwork", argument);
}

void Client::udpIn_checkNatStep1(int localIndex, int serverIndex)
{
	if (localIndex != 1)
		return;
	if (m_status != LoginedStatus)
		return;
	if (m_natStatus != Step1_1SendingToServer1 && m_natStatus != Step1_1WaitingForServer2)
		return;

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
	QByteArrayMap argument;
	argument["partlyType"] = QByteArray::number(partlyType);
	argument["clientUdp2LocalPort"] = QByteArray::number(clientUdp2LocalPort);
	sendTcp("checkNatStep1", argument);
}

void Client::udpIn_checkNatStep2Type1(int localIndex, int serverIndex)
{
	if (serverIndex != 1)
		return;
	if (m_status != LoginedStatus)
		return;
	if (m_natStatus != Step2_Type1_12WaitingForServer1 && m_natStatus != Step2_Type1_2WaitingForServer1)
		return;

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
		checkFirewall();
	}
}

void Client::timeout_checkNatStep2Type1()
{
	m_natType = FullOrRestrictedConeNat;
	m_natStatus = NatCheckFinished;
	tcpOut_checkNatStep2Type1(m_natType);

	emit natTypeConfirmed(m_natType);
	checkFirewall();
}

void Client::tcpOut_checkNatStep2Type1(NatType natType)
{
	QByteArrayMap argument;
	argument["natType"] = QByteArray::number((int)natType);
	sendTcp("checkNatStep2Type1", argument);
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

	m_natType = natType;
	m_natStatus = NatCheckFinished;

	emit natTypeConfirmed(m_natType);
	checkFirewall();
}

void Client::tcpOut_upnpAvailability(bool on)
{
	QByteArrayMap argument;
	argument["on"] = on ? "1" : "0";
	sendTcp("upnpAvailability", argument);
}

void Client::udpOut_updateAddress()
{
	QByteArrayMap argument;
	argument["userName"] = m_userName.toUtf8();
	argument["identifier"] = m_identifier.toUtf8();
	sendUdp(1, 1, "updateAddress", argument);
}

void Client::tcpOut_queryOnlineUser()
{
	QByteArrayMap argument;
	sendTcp("queryOnlineUser", argument);
}

void Client::tcpIn_queryOnlineUser(QString onlineUser)
{
	if (!checkStatusAndDisconnect("tcpIn_queryOnlineUser", LoginedStatus))
		return;
	QStringList onlineUserList = onlineUser.split(",");
	onlineUserList.removeAll(m_userName);
	emit replyQueryOnlineUser(onlineUserList);
}

void Client::tcpOut_tryTunneling(QString peerUserName)
{
	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	sendTcp("tryTunneling", argument);
}

void Client::tcpIn_tryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	if (!checkStatusAndDisconnect("tcpIn_tryTunneling", LoginedStatus, NatCheckFinished))
		return;

	if (needUpnp && !m_upnpAvailability)
		canTunnel = false;

	emit replyTryTunneling(peerUserName, canTunnel, needUpnp, failReason);
}

void Client::tcpOut_readyTunneling(QString peerUserName, QString peerLocalPassword, quint16 udp2UpnpPort, int requestId)
{
	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["peerLocalPassword"] = peerLocalPassword.toUtf8();
	argument["udp2UpnpPort"] = QByteArray::number(udp2UpnpPort);
	argument["requestId"] = QByteArray::number(requestId);
	sendTcp("readyTunneling", argument);
}

void Client::tcpIn_readyTunneling(QString peerUserName, int requestId, int tunnelId)
{
	if (!checkStatusAndDisconnect("tcpIn_readyTunneling", LoginedStatus, NatCheckFinished))
		return;
	if (!m_waitingRequestId.contains(requestId))
	{
		disconnectServer(QString("tcpIn_readyTunneling unknown requestId %1").arg(requestId));
		return;
	}
	m_waitingRequestId.remove(requestId);

	if (tunnelId != 0)
	{
		if (m_mapTunnelInfo.contains(tunnelId))
		{
			disconnectServer(QString("tcpIn_readyTunneling duplicated tunnelId %1").arg(tunnelId));
			return;
		}
		TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];
		tunnel.peerUserName = peerUserName;
		tunnel.status = ReadyTunnelingStatus;
		const bool useUpnp = (requestId % 2) == 1;
		tunnel.localIndex = useUpnp ? 2 : 1;

		emit replyReadyTunneling(requestId, tunnelId, tunnel.peerUserName);
	}
	else
	{
		emit replyReadyTunneling(requestId, 0, QString());
	}
}

void Client::tcpIn_startTunneling(int tunnelId, QString localPassword, QString peerUserName, QHostAddress peerAddress, quint16 peerPort, bool needUpnp)
{
	if (!checkStatusAndDisconnect("tcpIn_startTunneling", LoginedStatus, NatCheckFinished))
		return;

	const bool localPasswordError = (localPassword != m_localPassword);
	const bool upnpError = needUpnp && !m_upnpAvailability;
	const bool haveUnconfirmedKcpConnection = m_kcpManager.haveUnconfirmedKcpConnection();

	if (!localPasswordError && !upnpError && !haveUnconfirmedKcpConnection)
	{
		if (tunnelId == 0 || m_mapTunnelInfo.contains(tunnelId))
		{
			disconnectServer(QString("tcpIn_startTunneling error or duplicated tunnelId %1").arg(tunnelId));
			return;
		}
		TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];
		tunnel.status = TunnelingStatus;
		tunnel.peerUserName = peerUserName;
		tunnel.peerAddress = peerAddress;
		tunnel.peerPort = peerPort;
		tunnel.localIndex = needUpnp ? 2 : 1;

		createKcpConnection(tunnelId, tunnel);

		if(needUpnp)
			addUpnpPortMapping();

		udpOut_updateAddress();
		tcpOut_startTunneling(tunnelId, true, needUpnp ? m_udp2UpnpPort : 0, QString());

		emit tunnelStarted(tunnelId, tunnel.peerUserName, tunnel.peerAddress);
	}else
	{
		QString errorString;
		if (localPasswordError)
			errorString = U16("本地密码错误");
		else if (upnpError)
			errorString = U16("对方不支持upnp");
		else if (haveUnconfirmedKcpConnection)
			errorString = U16("对方与其他用户尝试连接中");
		
		tcpOut_startTunneling(tunnelId, false, 0, errorString);
	}
}

void Client::tcpOut_startTunneling(int tunnelId, bool canTunnel, quint16 udp2UpnpPort, QString errorString)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["canTunnel"] = canTunnel ? "1" : "0";
	argument["udp2UpnpPort"] = QByteArray::number(udp2UpnpPort);
	argument["errorString"] = errorString.toUtf8();
	sendTcp("startTunneling", argument);
}

void Client::tcpIn_tunneling(int tunnelId, QHostAddress peerAddress, quint16 peerPort)
{
	if (tunnelId == 0)
	{
		disconnectServer(QString("tcpIn_tunneling error tunnelId %1").arg(tunnelId));
		return;
	}

	TunnelInfo & tunnel = m_mapTunnelInfo[tunnelId];

	if (tunnel.status == TunnelingStatus)
	{
		disconnectServer(QString("tcpIn_tunneling duplicated tunnelId %1").arg(tunnelId));
		return;
	}

	tunnel.status = TunnelingStatus;
	tunnel.peerAddress = peerAddress;
	tunnel.peerPort = peerPort;

	createKcpConnection(tunnelId, tunnel);

	emit tunnelStarted(tunnelId, tunnel.peerUserName, tunnel.peerAddress);
}

void Client::tcpOut_tunnelHandeShaked(int tunnelId)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	sendTcp("tunnelHandeShaked", argument);
}

void Client::tcpIn_closeTunneling(int tunnelId, QString reason)
{
	if (tunnelId == 0)
	{
		disconnectServer(QString("tcpIn_closeTunneling error tunnelId %1").arg(tunnelId));
		return;
	}

	const QString peerUserName = m_mapTunnelInfo.value(tunnelId).peerUserName;

	m_mapTunnelInfo.remove(tunnelId);
	m_kcpManager.deleteKcpConnection(tunnelId, reason);

	emit tunnelClosed(tunnelId, peerUserName, reason);
}

void Client::tcpOut_closeTunneling(int tunnelId, QString reason)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["reason"] = reason.toUtf8();
	sendTcp("closeTunneling", argument);
}
