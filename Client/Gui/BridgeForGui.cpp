#include "BridgeForGui.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QDataStream>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

void BridgeForGui::slot_setConfig(QByteArray globalKey, QString randomIdentifierSuffix, QHostAddress serverHostAddress, quint16 serverTcpPort, bool disableBinaryCheck, bool disableUpnpPublicNetworkCheck)
{
	QByteArrayMap argument;
	argument["globalKey"] = globalKey;
	argument["randomIdentifierSuffix"] = randomIdentifierSuffix.toUtf8();
	argument["serverHostAddress"] = serverHostAddress.toString().toUtf8();
	argument["serverTcpPort"] = QByteArray::number(serverTcpPort);
	argument["disableBinaryCheck"] = boolToQByteArray(disableBinaryCheck);
	argument["disableUpnpPublicNetworkCheck"] = boolToQByteArray(disableUpnpPublicNetworkCheck);
	send("setConfig", argument);
}

void BridgeForGui::slot_setUserName(QString userName)
{
	QByteArrayMap argument;
	argument["userName"] = userName.toUtf8();
	send("setUserName", argument);
}

void BridgeForGui::slot_setLocalPassword(QString localPassword)
{
	QByteArrayMap argument;
	argument["localPassword"] = localPassword.toUtf8();
	send("setLocalPassword", argument);
}

int BridgeForGui::slot_start()
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	send("start", argument);
	return messageId;
}

int BridgeForGui::slot_stop()
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	send("stop", argument);
	return messageId;
}

int BridgeForGui::slot_tryLogin()
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	send("tryLogin", argument);
	return messageId;
}

void BridgeForGui::slot_queryOnlineUser()
{
	QByteArrayMap argument;
	send("queryOnlineUser", argument);
}

void BridgeForGui::slot_tryTunneling(QString peerUserName)
{
	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	send("tryTunneling", argument);
}

int BridgeForGui::slot_readyTunneling(QString peerUserName, QString peerLocalPassword, bool useUpnp)
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["peerLocalPassword"] = peerLocalPassword.toUtf8();
	argument["useUpnp"] = useUpnp ? "1" : "0";
	send("readyTunneling", argument);
	return messageId;
}

void BridgeForGui::slot_closeTunneling(int tunnelId)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	send("closeTunneling", argument);
}

int BridgeForGui::slot_addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress)
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["localPort"] = QByteArray::number(localPort);
	argument["remotePort"] = QByteArray::number(remotePort);
	argument["remoteAddress"] = remoteAddress.toString().toUtf8();
	send("addTransfer", argument);
	return messageId;
}

int BridgeForGui::slot_deleteTransfer(int tunnelId, quint16 localPort)
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["localPort"] = QByteArray::number(localPort);
	send("deleteTransfer", argument);
	return messageId;
}

int BridgeForGui::slot_getTransferOutList(int tunnelId)
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	send("getTransferOutList", argument);
	return messageId;
}

int BridgeForGui::slot_getTransferInList(int tunnelId)
{
	const int messageId = getNextMessageId();
	QByteArrayMap argument;
	argument["bridgeMessageId"] = QByteArray::number(messageId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	send("getTransferInList", argument);
	return messageId;
}




BridgeForGui::BridgeForGui(QObject * parent)
	:QObject(parent)
{
	m_running = false;
	m_binPath = QCoreApplication::applicationFilePath();
	m_messageConverter.setKey((const quint8*)QCryptographicHash::hash(m_binPath.toUtf8(), QCryptographicHash::Md5).constData());
	m_socket.setParent(this);
	m_timerTryConnect.setParent(this);
	m_timerTryConnect.setInterval(25);
	m_servicePid = 0;

	connect(&m_timerTryConnect, SIGNAL(timeout()), this, SLOT(onTimerTryConnect()));
	connect(&m_socket, SIGNAL(connected()), this, SLOT(onSocketConnected()));
	connect(&m_socket, SIGNAL(disconnected()), this, SLOT(onSocketDisconnected()));
	connect(&m_socket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));
}

BridgeForGui::~BridgeForGui()
{
	stop();
}

bool BridgeForGui::start()
{
	if (m_running)
		return false;
	const bool windowsServiceOk = tryInstallAndStartWindowsService();
	const bool commandServiceOk = windowsServiceOk ? false : QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList() << "CommandService");

	if (!windowsServiceOk && !commandServiceOk)
		return false;

	m_running = true;
	m_servicePid = 0;
	m_startTime = QTime::currentTime();
	m_timerTryConnect.start();
	tryConnect();
	return true;
}

void BridgeForGui::stop()
{
	if (!m_running)
		return;
	m_running = false;
	m_servicePid = 0;
	m_timerTryConnect.stop();
	m_socket.disconnectFromServer();
}

qint64 BridgeForGui::servicePid()
{
	return m_servicePid;
}

void BridgeForGui::tryConnect()
{
	const QString serviceName = "NatTunnelClient_" + QCryptographicHash::hash(m_binPath.toUtf8(), QCryptographicHash::Sha1).toHex();
	m_socket.connectToServer(serviceName);
}

void BridgeForGui::onTimerTryConnect()
{
	if (!m_running)
		return;
	if (m_socket.state() == QLocalSocket::UnconnectedState)
	{
		if(m_startTime.elapsed() < 20 * 1000)
			tryConnect();
		else
			emit lostConnection();
	}
	else if (m_socket.state() == QLocalSocket::ConnectedState)
	{
		m_timerTryConnect.stop();
	}
}

void BridgeForGui::onSocketConnected()
{
	if (!m_running)
		return;
	m_timerTryConnect.stop();
	m_socket.write(m_buffer);
	m_buffer.clear();
}

void BridgeForGui::onSocketDisconnected()
{
	if (!m_running)
		return;
	if(m_timerTryConnect.isActive())		// timer正在运行说明还在尝试连接阶段，不能退
		return;
	emit lostConnection();
}

void BridgeForGui::onSocketReadyRead()
{
	if (!m_running)
		return;
	while (m_socket.canReadLine())
	{
		QByteArray line = m_socket.readLine();
		if (line.endsWith('\n'))
			line.chop(1);

		dealIn(line);
	}
}

bool BridgeForGui::tryInstallAndStartWindowsService()
{
#if defined(Q_OS_WIN)
	const QString serviceStartName = "LocalSystem";
	const QString serviceBinPath = "\"" + m_binPath + "\" WindowsService";
	const QString serviceName = "NatTunnelClient_" + QCryptographicHash::hash(m_binPath.toUtf8(), QCryptographicHash::Sha1).toHex();

	SC_HANDLE hSCM = OpenSCManager(0, 0, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	if (!hSCM)
		return false;
	SC_HANDLE hMyService = OpenServiceW(hSCM, (LPCWSTR)serviceName.constData(), SERVICE_ALL_ACCESS);

	struct MyDeleter
	{
		SC_HANDLE * ptr_hSCM;
		SC_HANDLE * ptr_hMyService;
		~MyDeleter()
		{
			if (*ptr_hMyService)
			{
				CloseServiceHandle(*ptr_hMyService);
				*ptr_hMyService = 0;
			}
			if (*ptr_hSCM)
			{
				CloseServiceHandle(*ptr_hSCM);
				*ptr_hSCM = 0;
			}
		}
	};

	MyDeleter myDeleter;
	myDeleter.ptr_hSCM = &hSCM;
	myDeleter.ptr_hMyService = &hMyService;

	if (hMyService)
	{
		// 服务已存在的情况下，检查参数，如果参数不正确，删除服务重新创建，否则启动服务
		DWORD needSize = 0;
		QueryServiceConfigW(hMyService, NULL, 0, &needSize);
		if (needSize == 0)
			return false;

		QUERY_SERVICE_CONFIGW * serviceConfig = (QUERY_SERVICE_CONFIGW*)malloc(needSize);

		bool correct = false;
		if (QueryServiceConfigW(hMyService, serviceConfig, needSize, &needSize))
		{
			correct = true;
			correct &= serviceConfig->dwServiceType == SERVICE_WIN32_OWN_PROCESS;
			correct &= serviceConfig->dwStartType != SERVICE_DISABLED;
			correct &= QString::fromUtf16((const ushort *)(serviceConfig->lpBinaryPathName)) == serviceBinPath;
			correct &= QString::fromUtf16((const ushort *)(serviceConfig->lpServiceStartName)) == serviceStartName;
		}

		free(serviceConfig);
		serviceConfig = NULL;

		if (correct)
		{
			SERVICE_STATUS serviceStatus = { 0 };
			QueryServiceStatus(hMyService, &serviceStatus);
			if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
				return true;
		}
		else
		{
			SERVICE_STATUS serviceStatus = { 0 };
			ControlService(hMyService, SERVICE_CONTROL_STOP, &serviceStatus);
			DeleteService(hMyService);
			CloseServiceHandle(hMyService);
			hMyService = 0;
		}
	}

	if (hMyService == 0)
	{
		hMyService = CreateServiceW(hSCM, (LPCWSTR)serviceName.constData(), (LPCWSTR)serviceName.constData(), SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, (LPCWSTR)serviceBinPath.constData(),
			NULL, NULL, NULL, (LPCWSTR)serviceStartName.constData(), NULL);
	}
	if (hMyService == 0)
		return false;
	if (StartServiceW(hMyService, 0, NULL))
	{
		return true;
	}
	else
	{
		return false;
	}
#else
	return false;
#endif
}

int BridgeForGui::getNextMessageId()
{
	return m_messageId.fetchAndAddRelaxed(1);
}

void BridgeForGui::send(QByteArray type, QByteArrayMap argument)
{
	if (!m_running)
		return;
	if (m_socket.state() == QLocalSocket::ConnectedState)
		m_socket.write(m_messageConverter.serialize(type, argument));
	else
		m_buffer.append(m_messageConverter.serialize(type, argument));
}

void BridgeForGui::dealIn(QByteArray line)
{
	if (!m_running)
		return;
	QByteArrayMap argument;
	QByteArray type = m_messageConverter.parse(line, &argument);
	if (type.isEmpty())
		return;

	if (type == "hello")
		m_servicePid = argument.value("servicePid").toLongLong();
	if (type == "connected")
		emit event_connected();
	else if (type == "disconnected")
		emit event_disconnected();
	else if (type == "discarded")
		emit event_discarded(argument.value("reason"));
	else if (type == "binaryError")
		emit event_binaryError(argument.value("correctBinary"));
	else if (type == "logined")
		emit event_logined();
	else if (type == "loginFailed")
		emit event_loginFailed(argument.value("userName"), argument.value("msg"));
	else if (type == "natTypeConfirmed")
		emit event_natTypeConfirmed((NatType)argument.value("natType").toInt());
	else if (type == "upnpStatusChanged")
		emit event_upnpStatusChanged((UpnpStatus)argument.value("upnpStatus").toInt());
	else if (type == "warning")
		emit event_warning(argument.value("msg"));
	else if (type == "replyQueryOnlineUser")
		emit event_replyQueryOnlineUser(QString::fromUtf8(argument.value("onlineUserList")).split("\n", QString::SkipEmptyParts));
	else if (type == "replyTryTunneling")
		emit event_replyTryTunneling(argument.value("peerUserName"), QByteArrayToBool(argument.value("canTunnel")),
			QByteArrayToBool(argument.value("needUpnp")), argument.value("failReason"));
	else if (type == "replyReadyTunneling")
		emit event_replyReadyTunneling(argument.value("requestId").toInt(), argument.value("tunnelId").toInt(), argument.value("peerUserName"));
	else if (type == "tunnelStarted")
		emit event_tunnelStarted(argument.value("tunnelId").toInt(), argument.value("peerUserName"), QHostAddress((QString)argument.value("peerAddress")));
	else if (type == "tunnelHandShaked")
		emit event_tunnelHandShaked(argument.value("tunnelId").toInt());
	else if (type == "tunnelClosed")
		emit event_tunnelClosed(argument.value("tunnelId").toInt(), argument.value("peerUserName"), argument.value("reason"));
	else if (type == "onStart")
		emit event_onStart(argument.value("bridgeMessageId").toInt(), QByteArrayToBool(argument.value("result_ok")));
	else if (type == "onStop")
		emit event_onStop(argument.value("bridgeMessageId").toInt(), QByteArrayToBool(argument.value("result_ok")));
	else if (type == "onTryLogin")
		emit event_onTryLogin(argument.value("bridgeMessageId").toInt(), QByteArrayToBool(argument.value("result_ok")));
	else if (type == "onReadyTunneling")
		emit event_onReadyTunneling(argument.value("bridgeMessageId").toInt(), argument.value("result_requestId").toInt());
	else if (type == "onAddTransfer")
		emit event_onAddTransfer(argument.value("bridgeMessageId").toInt(), QByteArrayToBool(argument.value("result_ok")));
	else if (type == "onDeleteTransfer")
		emit event_onDeleteTransfer(argument.value("bridgeMessageId").toInt(), QByteArrayToBool(argument.value("result_ok")));
	else if (type == "onGetTransferOutList")
	{
		QMap<quint16, Peer> result_list;
		QDataStream in(argument.value("result_list"));
		in.setByteOrder(QDataStream::LittleEndian);
		in >> result_list;
		emit event_onGetTransferOutList(argument.value("bridgeMessageId").toInt(), result_list);
	}
	else if (type == "onGetTransferInList")
	{
		QMap<quint16, Peer> result_list;
		QDataStream in(argument.value("result_list"));
		in.setByteOrder(QDataStream::LittleEndian);
		in >> result_list;
		emit event_onGetTransferInList(argument.value("bridgeMessageId").toInt(), result_list);
	}
}
