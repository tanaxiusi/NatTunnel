#include "TransferManager.h"

TransferManager::TransferManager(QObject *parent, Client * client)
	: QObject(parent)
{
	m_client = client;

	connect(client, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));
	connect(client, SIGNAL(tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)));
	connect(client, SIGNAL(tunnelData(int, QByteArray)), this, SLOT(onTunnelData(int, QByteArray)));
	connect(client, SIGNAL(tunnelClosed(int,QString,QString)), this, SLOT(onTunnelClosed(int)));
}

TransferManager::~TransferManager()
{
	foreach (TcpTransfer * tcpTransfer, m_mapTcpTransfer)
		delete tcpTransfer;
	m_mapTcpTransfer.clear();
}

bool TransferManager::addTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (!tcpTransfer)
		return false;
	return tcpTransfer->addTransfer(localPort, remotePort, remoteAddress);
}

bool TransferManager::deleteTransfer(int tunnelId, quint16 localPort)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (!tcpTransfer)
		return false;
	return tcpTransfer->deleteTransfer(localPort);
}

QMap<quint16, Peer> TransferManager::getTransferOutList(int tunnelId)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (!tcpTransfer)
		return QMap<quint16, Peer>();
	return tcpTransfer->getTransferOutList();
}

QMap<quint16, Peer> TransferManager::getTransferInList(int tunnelId)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (!tcpTransfer)
		return QMap<quint16, Peer>();
	return tcpTransfer->getTransferInList();
}

void TransferManager::onClientDisconnected()
{
	foreach (TcpTransfer * tcpTransfer, m_mapTcpTransfer)
		delete tcpTransfer;
	m_mapTcpTransfer.clear();
}

void TransferManager::onTunnelHandShaked(int tunnelId)
{
	TcpTransfer *& tcpTransfer = m_mapTcpTransfer[tunnelId];
	Q_ASSERT(!tcpTransfer);
	tcpTransfer = new TcpTransfer();
	tcpTransfer->setProperty("tunnelId", tunnelId);
	connect(tcpTransfer, SIGNAL(dataOutput(QByteArray)), this, SLOT(onTcpTransferOutput(QByteArray)));
}

void TransferManager::onTunnelData(int tunnelId, QByteArray package)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (tcpTransfer)
		tcpTransfer->dataInput(package);
}

void TransferManager::onTunnelClosed(int tunnelId)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (tcpTransfer)
	{
		delete tcpTransfer;
		m_mapTcpTransfer.remove(tunnelId);
	}
}

void TransferManager::onTcpTransferOutput(QByteArray package)
{
	TcpTransfer * tcpTransfer = (TcpTransfer*)sender();
	if (!tcpTransfer)
		return;
	const int tunnelId = tcpTransfer->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	m_client->tunnelWrite(tunnelId, package);
}