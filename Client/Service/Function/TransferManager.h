#pragma once

#include <QObject>
#include "TcpTransfer.h"

class TransferManager : public QObject
{
	Q_OBJECT

signals:
	void dataOutput(int tunnelId, QByteArray package);

public:
	TransferManager(QObject *parent = 0);
	~TransferManager();

public slots:
	bool addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	bool deleteTransfer(int tunnelId, quint16 localPort);

	QMap<quint16, Peer> getTransferOutList(int tunnelId);
	QMap<quint16, Peer> getTransferInList(int tunnelId);

	void dataInput(int tunnelId, QByteArray package);
	void clientDisconnected();
	void tunnelHandShaked(int tunnelId);
	void tunnelClosed(int tunnelId);

private slots:
	void onTcpTransferOutput(QByteArray package);

private:
	QMap<int, TcpTransfer*> m_mapTcpTransfer;
};
