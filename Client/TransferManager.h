#pragma once

#include <QObject>
#include "TcpTransfer.h"
#include "Client.h"

struct TransferInfo
{
	quint16 localPort = 0;
	QHostAddress remoteAddress;
	quint16 remotePort = 0;
};

class TransferManager : public QObject
{
	Q_OBJECT

public:
	TransferManager(QObject *parent, Client * client);
	~TransferManager();

public slots:
	bool addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	bool addTransfer(int tunnelId, QList<TransferInfo> transferInfoList, QList<TransferInfo> * outFailedList);

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
