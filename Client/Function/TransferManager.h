#pragma once

#include <QObject>
#include "TcpTransfer.h"
#include "Client.h"

class TransferManager : public QObject
{
	Q_OBJECT

public:
	TransferManager(QObject *parent, Client * client);
	~TransferManager();

public slots:
	bool addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	bool deleteTransfer(int tunnelId, quint16 localPort);

	QMap<quint16, Peer> getTransferOutList(int tunnelId);
	QMap<quint16, Peer> getTransferInList(int tunnelId);

private slots:
	void onClientDisconnected();

	void onTunnelHandShaked(int tunnelId);
	void onTunnelData(int tunnelId, QByteArray package);
	void onTunnelClosed(int tunnelId);

	void onTcpTransferOutput(QByteArray package);

private:
	QMap<int, TcpTransfer*> m_mapTcpTransfer;
	Client * m_client;
};
