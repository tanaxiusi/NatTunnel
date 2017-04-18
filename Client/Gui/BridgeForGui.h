#pragma once
#include <QOBject>
#include <QLocalSocket>
#include <QHostAddress>
#include <QTime>
#include <QTimer>
#include "MessageConverter.h"
#include "Other.h"
#include "Peer.h"

class BridgeForGui : public QObject
{
	Q_OBJECT

signals:
	void lostConnection();

	void event_connected();
	void event_disconnected();
	void event_discarded(QString reason);
	void event_binaryError(QByteArray correctBinary);
	void event_logined();
	void event_loginFailed(QString userName, QString msg);
	void event_natTypeConfirmed(NatType natType);
	void event_upnpStatusChanged(UpnpStatus upnpStatus);
	void event_warning(QString msg);
	void event_replyQueryOnlineUser(QStringList onlineUserList);
	void event_replyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void event_replyReadyTunneling(int requestId, int tunnelId, QString peerUserName);
	void event_tunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void event_tunnelHandShaked(int tunnelId);
	void event_tunnelClosed(int tunnelId, QString peerUserName, QString reason);

	void event_onStart(int bridgeMessageId, bool result_ok);
	void event_onStop(int bridgeMessageId, bool result_ok);
	void event_onTryLogin(int bridgeMessageId, bool result_ok);
	void event_onReadyTunneling(int bridgeMessageId, int result_requestId);
	void event_onAddTransfer(int bridgeMessageId, bool result_ok);
	void event_onDeleteTransfer(int bridgeMessageId, bool result_ok);
	void event_onGetTransferOutList(int bridgeMessageId, QMap<quint16, Peer> result_list);
	void event_onGetTransferInList(int bridgeMessageId, QMap<quint16, Peer> result_list);

public slots:
	void slot_setConfig(QByteArray globalKey, QString randomIdentifierSuffix, QHostAddress serverHostAddress, quint16 serverTcpPort, bool disableBinaryCheck);
	void slot_setUserName(QString userName);
	void slot_setLocalPassword(QString localPassword);
	int slot_start();
	int slot_stop();
	int slot_tryLogin();
	void slot_queryOnlineUser();
	void slot_tryTunneling(QString peerUserName);
	int slot_readyTunneling(QString peerUserName, QString peerLocalPassword, bool useUpnp);
	void slot_closeTunneling(int tunnelId);

	int slot_addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	int slot_deleteTransfer(int tunnelId, quint16 localPort);
	int slot_getTransferOutList(int tunnelId);
	int slot_getTransferInList(int tunnelId);

public:
	BridgeForGui(QObject * parent = 0);
	~BridgeForGui();

public:
	bool start();
	void stop();
	qint64 servicePid();

private slots:
	void tryConnect();
	void onTimerTryConnect();
	void onSocketConnected();
	void onSocketDisconnected();
	void onSocketReadyRead();

private:
	bool tryInstallAndStartWindowsService();
	int getNextMessageId();
	void send(QByteArray type, QByteArrayMap argument);
	void dealIn(QByteArray line);

private:
	bool m_running;
	QString m_binPath;
	MessageConverter m_messageConverter;
	QLocalSocket m_socket;
	QTime m_startTime;
	QTimer m_timerTryConnect;
	qint64 m_servicePid;
	QByteArray m_buffer;
	QAtomicInt m_messageId;
};