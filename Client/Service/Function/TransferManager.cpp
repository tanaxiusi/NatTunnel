#include "TransferManager.h"

TransferManager::TransferManager(QObject *parent)
	: QObject(parent)
{

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

void TransferManager::dataInput(int tunnelId, QByteArray package)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (tcpTransfer)
		tcpTransfer->dataInput(package);
}

void TransferManager::clientDisconnected()
{
	foreach (TcpTransfer * tcpTransfer, m_mapTcpTransfer)
		delete tcpTransfer;
	m_mapTcpTransfer.clear();
}

void TransferManager::tunnelHandShaked(int tunnelId)
{
	TcpTransfer *& tcpTransfer = m_mapTcpTransfer[tunnelId];
	Q_ASSERT(!tcpTransfer);
	tcpTransfer = new TcpTransfer();
	tcpTransfer->setProperty("tunnelId", tunnelId);
	connect(tcpTransfer, SIGNAL(dataOutput(QByteArray)), this, SLOT(onTcpTransferOutput(QByteArray)));
}

void TransferManager::tunnelClosed(int tunnelId)
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
	emit dataOutput(tunnelId, package);
}