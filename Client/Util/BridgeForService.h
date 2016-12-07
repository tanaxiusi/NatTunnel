#pragma once
#include <QObject>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include "MessageConverter.h"
#include "Service/Function/Client.h"

class BridgeForService : public QObject
{
	Q_OBJECT

public slots:
	void sendEvent_hello(QLocalSocket * socket);

	void sendEvent_connected();
	void sendEvent_disconnected();
	void sendEvent_discarded(QString reason);
	void sendEvent_binaryError(QByteArray correctBinary);
	void sendEvent_logined();
	void sendEvent_loginFailed(QString userName, QString msg);
	void sendEvent_natTypeConfirmed(NatType natType);
	void sendEvent_upnpStatusChanged(UpnpStatus upnpStatus);
	void sendEvent_warning(QString msg);
	void sendEvent_replyQueryOnlineUser(QStringList onlineUserList);
	void sendEvent_replyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void sendEvent_replyReadyTunneling(int requestId, int tunnelId, QString peerUserName);
	void sendEvent_tunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void sendEvent_tunnelHandShaked(int tunnelId);
	void sendEvent_tunnelClosed(int tunnelId, QString peerUserName, QString reason);

	void sendEvent_onStart(QByteArray bridgeMessageId, bool result_ok);
	void sendEvent_onStop(QByteArray bridgeMessageId, bool result_ok);
	void sendEvent_onTryLogin(QByteArray bridgeMessageId, bool result_ok);
	void sendEvent_onReadyTunneling(QByteArray bridgeMessageId, int result_requestId);
	void sendEvent_onAddTransfer(QByteArray bridgeMessageId, bool result_ok);
	void sendEvent_onDeleteTransfer(QByteArray bridgeMessageId, bool result_ok);
	void sendEvent_onGetTransferOutList(QByteArray bridgeMessageId, QMap<quint16, Peer> result_list);
	void sendEvent_onGetTransferInList(QByteArray bridgeMessageId, QMap<quint16, Peer> result_list);

public:
	BridgeForService(QObject * parent = 0);
	~BridgeForService();

public:
	bool start();
	bool stop();

private slots:
	void stopAndExit();
	void onNewConnection();
	void onSocketDisconnected();
	void onSocketReadyRead();

private:
	Client * getClientFromSocket(QLocalSocket * socket);
	QLocalSocket * getSocketFromClient(Client * client);
	QLocalSocket * getCurrentSocket();
	void send(QLocalSocket * socket, QByteArray type, QByteArrayMap argument);
	void dealIn(QLocalSocket * socket, Client * client, QByteArray line);

private:
	QString m_binPath;
	MessageConverter m_messageConverter;
	QLocalServer m_server;
	QTimer m_timer;
	QHash<QLocalSocket*, Client*> m_map;
};