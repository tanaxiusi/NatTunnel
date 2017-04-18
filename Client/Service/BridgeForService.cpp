#include "BridgeForService.h"
#include <QCryptographicHash>
#include <QCoreApplication>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

void BridgeForService::sendEvent_hello(QLocalSocket * socket)
{
	QByteArrayMap argument;
	argument["servicePid"] = QByteArray::number(QCoreApplication::applicationPid());
	send(socket, "hello", argument);
}

void BridgeForService::sendEvent_connected()
{
	QByteArrayMap argument;
	send(getCurrentSocket(), "connected", argument);
}

void BridgeForService::sendEvent_disconnected()
{
	QByteArrayMap argument;
	send(getCurrentSocket(), "disconnected", argument);
}

void BridgeForService::sendEvent_discarded(QString reason)
{
	QByteArrayMap argument;
	argument["reason"] = reason.toUtf8();
	send(getCurrentSocket(), "discarded", argument);
}

void BridgeForService::sendEvent_binaryError(QByteArray correctBinary)
{
	QByteArrayMap argument;
	argument["correctBinary"] = correctBinary;
	send(getCurrentSocket(), "binaryError", argument);
}

void BridgeForService::sendEvent_logined()
{
	QByteArrayMap argument;
	send(getCurrentSocket(), "logined", argument);
}

void BridgeForService::sendEvent_loginFailed(QString userName, QString msg)
{
	QByteArrayMap argument;
	argument["userName"] = userName.toUtf8();
	argument["msg"] = msg.toUtf8();
	send(getCurrentSocket(), "loginFailed", argument);
}

void BridgeForService::sendEvent_natTypeConfirmed(NatType natType)
{
	QByteArrayMap argument;
	argument["natType"] = QByteArray::number((int)natType);
	send(getCurrentSocket(), "natTypeConfirmed", argument);
}

void BridgeForService::sendEvent_upnpStatusChanged(UpnpStatus upnpStatus)
{
	QByteArrayMap argument;
	argument["upnpStatus"] = QByteArray::number((int)upnpStatus);
	send(getCurrentSocket(), "upnpStatusChanged", argument);
}

void BridgeForService::sendEvent_warning(QString msg)
{
	QByteArrayMap argument;
	argument["msg"] = msg.toUtf8();
	send(getCurrentSocket(), "warning", argument);
}

void BridgeForService::sendEvent_replyQueryOnlineUser(QStringList onlineUserList)
{
	QByteArrayMap argument;
	argument["onlineUserList"] = onlineUserList.join("\n").toUtf8();
	send(getCurrentSocket(), "replyQueryOnlineUser", argument);
}

void BridgeForService::sendEvent_replyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	QByteArrayMap argument;
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["canTunnel"] = boolToQByteArray(canTunnel);
	argument["needUpnp"] = boolToQByteArray(needUpnp);
	argument["failReason"] = failReason.toUtf8();
	send(getCurrentSocket(), "replyTryTunneling", argument);
}

void BridgeForService::sendEvent_replyReadyTunneling(int requestId, int tunnelId, QString peerUserName)
{
	QByteArrayMap argument;
	argument["requestId"] = QByteArray::number(requestId);
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["peerUserName"] = peerUserName.toUtf8();
	send(getCurrentSocket(), "replyReadyTunneling", argument);
}

void BridgeForService::sendEvent_tunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["peerAddress"] = peerAddress.toString().toUtf8();
	send(getCurrentSocket(), "tunnelStarted", argument);
}

void BridgeForService::sendEvent_tunnelHandShaked(int tunnelId)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	send(getCurrentSocket(), "tunnelHandShaked", argument);
}

void BridgeForService::sendEvent_tunnelClosed(int tunnelId, QString peerUserName, QString reason)
{
	QByteArrayMap argument;
	argument["tunnelId"] = QByteArray::number(tunnelId);
	argument["peerUserName"] = peerUserName.toUtf8();
	argument["reason"] = reason.toUtf8();
	send(getCurrentSocket(), "tunnelClosed", argument);
}

void BridgeForService::sendEvent_onStart(QByteArray bridgeMessageId, bool result_ok)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_ok"] = boolToQByteArray(result_ok);
	send(getCurrentSocket(), "onStart", argument);
}

void BridgeForService::sendEvent_onStop(QByteArray bridgeMessageId, bool result_ok)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_ok"] = boolToQByteArray(result_ok);
	send(getCurrentSocket(), "onStop", argument);
}

void BridgeForService::sendEvent_onTryLogin(QByteArray bridgeMessageId, bool result_ok)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_ok"] = boolToQByteArray(result_ok);
	send(getCurrentSocket(), "onTryLogin", argument);
}

void BridgeForService::sendEvent_onReadyTunneling(QByteArray bridgeMessageId, int result_requestId)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_requestId"] = QByteArray::number(result_requestId);
	send(getCurrentSocket(), "onReadyTunneling", argument);
}

void BridgeForService::sendEvent_onAddTransfer(QByteArray bridgeMessageId, bool result_ok)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_ok"] = boolToQByteArray(result_ok);
	send(getCurrentSocket(), "onAddTransfer", argument);
}

void BridgeForService::sendEvent_onDeleteTransfer(QByteArray bridgeMessageId, bool result_ok)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	argument["result_ok"] = boolToQByteArray(result_ok);
	send(getCurrentSocket(), "onDeleteTransfer", argument);
}

void BridgeForService::sendEvent_onGetTransferOutList(QByteArray bridgeMessageId, QMap<quint16, Peer> result_list)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	QDataStream out(&argument["result_list"], QIODevice::Append);
	out.setByteOrder(QDataStream::LittleEndian);
	out << result_list;
	send(getCurrentSocket(), "onGetTransferOutList", argument);
}

void BridgeForService::sendEvent_onGetTransferInList(QByteArray bridgeMessageId, QMap<quint16, Peer> result_list)
{
	QByteArrayMap argument;
	argument["bridgeMessageId"] = bridgeMessageId;
	QDataStream out(&argument["result_list"], QIODevice::Append);
	out.setByteOrder(QDataStream::LittleEndian);
	out << result_list;
	send(getCurrentSocket(), "onGetTransferInList", argument);
}

BridgeForService::BridgeForService(QObject * parent)
	:QObject(parent)
{
	m_binPath = QCoreApplication::applicationFilePath();
	m_messageConverter.setKey((const quint8*)QCryptographicHash::hash(m_binPath.toUtf8(), QCryptographicHash::Md5).constData());
	m_server.setParent(this);
	m_timer.setParent(this);

	connect(&m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(stopAndExit()));

	m_timer.setSingleShot(true);
	m_timer.setInterval(30 * 1000);
	m_timer.start();			// Service启动后30秒内没有Gui来连接，就退出
}

BridgeForService::~BridgeForService()
{
	foreach(QLocalSocket * socket, m_map.keys())
	{
		Client * client = m_map[socket];
		delete socket;
		delete client;
	}
	m_map.clear();
}

bool BridgeForService::start()
{
	const QString serviceName = "NatTunnelClient_" + QCryptographicHash::hash(m_binPath.toUtf8(), QCryptographicHash::Sha1).toHex();
	return m_server.listen(serviceName);
}

bool BridgeForService::stop()
{
	if (m_server.isListening())
	{
		m_server.close();
		return true;
	}
	else
	{
		return false;
	}
}

void BridgeForService::stopAndExit()
{
	stop();
	QCoreApplication::exit();
}

void BridgeForService::onNewConnection()
{
	m_timer.stop();
	while (m_server.hasPendingConnections())
	{
		QLocalSocket * socket = m_server.nextPendingConnection();

		connect(socket, SIGNAL(disconnected()), this, SLOT(onSocketDisconnected()));
		connect(socket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));

		Client * client = new Client(this);
		m_map[socket] = client;

		connect(client, SIGNAL(connected()), this, SLOT(sendEvent_connected()));
		connect(client, SIGNAL(disconnected()), this, SLOT(sendEvent_disconnected()));
		connect(client, SIGNAL(discarded(QString)), this, SLOT(sendEvent_discarded(QString)));
		connect(client, SIGNAL(binaryError(QByteArray)), this, SLOT(sendEvent_binaryError(QByteArray)));
		connect(client, SIGNAL(logined()), this, SLOT(sendEvent_logined()));
		connect(client, SIGNAL(loginFailed(QString, QString)), this, SLOT(sendEvent_loginFailed(QString, QString)));
		connect(client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(sendEvent_natTypeConfirmed(NatType)));
		connect(client, SIGNAL(upnpStatusChanged(UpnpStatus)), this, SLOT(sendEvent_upnpStatusChanged(UpnpStatus)));
		connect(client, SIGNAL(warning(QString)), this, SLOT(sendEvent_warning(QString)));
		connect(client, SIGNAL(replyQueryOnlineUser(QStringList)), this, SLOT(sendEvent_replyQueryOnlineUser(QStringList)));
		connect(client, SIGNAL(replyTryTunneling(QString, bool, bool, QString)), this, SLOT(sendEvent_replyTryTunneling(QString, bool, bool, QString)));
		connect(client, SIGNAL(replyReadyTunneling(int, int, QString)), this, SLOT(sendEvent_replyReadyTunneling(int, int, QString)));
		connect(client, SIGNAL(tunnelStarted(int, QString, QHostAddress)), this, SLOT(sendEvent_tunnelStarted(int, QString, QHostAddress)));
		connect(client, SIGNAL(tunnelHandShaked(int)), this, SLOT(sendEvent_tunnelHandShaked(int)));
		connect(client, SIGNAL(tunnelClosed(int, QString, QString)), this, SLOT(sendEvent_tunnelClosed(int, QString, QString)));

		sendEvent_hello(socket);
	}
}

void BridgeForService::onSocketDisconnected()
{
	QLocalSocket * socket = (QLocalSocket*)sender();
	QHash<QLocalSocket*, Client*>::iterator iter = m_map.find(socket);
	if (iter == m_map.end())
		return;
	Client * client = iter.value();
	delete client;
	m_map.erase(iter);

	if (m_map.isEmpty())
		stopAndExit();		// 最后一个连接断开后也准备退出
}

void BridgeForService::onSocketReadyRead()
{
	QLocalSocket * socket = (QLocalSocket*)sender();
	Client * client = getClientFromSocket(socket);
	if (!socket || !client)
		return;

	while (socket->canReadLine())
	{
		QByteArray line = socket->readLine();
		if (line.endsWith('\n'))
			line.chop(1);	
		dealIn(socket, client, line);
	}
}

Client * BridgeForService::getClientFromSocket(QLocalSocket * socket)
{
	return m_map.value(socket, NULL);
}

QLocalSocket * BridgeForService::getSocketFromClient(Client * client)
{
	for (QHash<QLocalSocket*, Client*>::ConstIterator iter = m_map.constBegin();
		iter != m_map.constEnd(); ++iter)
	{
		if (iter.value() == client)
			return iter.key();
	}
	return NULL;
}

QLocalSocket * BridgeForService::getCurrentSocket()
{
	if (QLocalSocket * socket = qobject_cast<QLocalSocket*>(sender()))
		return socket;
	if (Client * client = qobject_cast<Client*>(sender()))
		return getSocketFromClient(client);
	return NULL;
}

void BridgeForService::send(QLocalSocket * socket, QByteArray type, QByteArrayMap argument)
{
	if (!socket)
		return;
	socket->write(m_messageConverter.serialize(type, argument));
}

void BridgeForService::dealIn(QLocalSocket * socket, Client * client, QByteArray line)
{
	if (!client)
		return;

	QByteArrayMap argument;
	QByteArray type = m_messageConverter.parse(line, &argument);
	if (type.isEmpty())
		return;

	if (type == "setConfig")
		client->setConfig(argument.value("globalKey"), argument.value("randomIdentifierSuffix"),
			QHostAddress((QString)argument.value("serverHostAddress")), argument.value("serverTcpPort").toInt(),
			QByteArrayToBool(argument.value("disableBinaryCheck")));
	else if (type == "setUserName")
	{
		client->setUserName(argument.value("userName"));
	}
	else if (type == "setLocalPassword")
	{
		client->setLocalPassword(argument.value("localPassword"));
	}
	else if (type == "start")
	{
		bool result = client->start();
		sendEvent_onStart(argument.value("bridgeMessageId"), result);
	}
	else if (type == "stop")
	{
		bool result = client->stop();
		sendEvent_onStop(argument.value("bridgeMessageId"), result);
	}
	else if (type == "tryLogin")
	{
		bool result = client->tryLogin();
		sendEvent_onTryLogin(argument.value("bridgeMessageId"), result);
	}
	else if (type == "queryOnlineUser")
	{
		client->queryOnlineUser();
	}
	else if (type == "tryTunneling")
	{
		client->tryTunneling(argument.value("peerUserName"));
	}
	else if (type == "readyTunneling")
	{
		int result_requestId = client->readyTunneling(argument.value("peerUserName"), argument.value("peerLocalPassword"), QByteArrayToBool(argument.value("useUpnp")));
		sendEvent_onReadyTunneling(argument.value("bridgeMessageId"), result_requestId);
	}
	else if (type == "closeTunneling")
	{
		client->closeTunneling(argument.value("tunnelId").toInt());
	}
	else if (type == "addTransfer")
	{
		bool result = client->addTransfer(argument.value("tunnelId").toInt(), argument.value("localPort").toInt(),
			argument.value("remotePort").toInt(), QHostAddress((QString)argument.value("remoteAddress")));
		sendEvent_onAddTransfer(argument.value("bridgeMessageId"), result);
	}
	else if (type == "deleteTransfer")
	{
		bool result = client->deleteTransfer(argument.value("tunnelId").toInt(), argument.value("localPort").toInt());
		sendEvent_onDeleteTransfer(argument.value("bridgeMessageId"), result);
	}
	else if (type == "getTransferOutList")
	{
		QMap<quint16, Peer> result_list = client->getTransferOutList(argument.value("tunnelId").toInt());
		sendEvent_onGetTransferOutList(argument.value("bridgeMessageId"), result_list);
	}
	else if (type == "getTransferInList")
	{
		QMap<quint16, Peer> result_list = client->getTransferInList(argument.value("tunnelId").toInt());
		sendEvent_onGetTransferInList(argument.value("bridgeMessageId"), result_list);
	}
}

