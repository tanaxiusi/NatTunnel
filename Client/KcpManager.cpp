#include "KcpManager.h"
#include <QDateTime>
#include "Other.h"

KcpManager::KcpManager(QObject *parent)
	: QObject(parent)
{
	m_timerUpdate.setParent(this);
	connect(&m_timerUpdate, SIGNAL(timeout()), this, SLOT(updateAllKcp()));
	m_timerUpdate.start(25);
}

KcpManager::~KcpManager()
{
	clear();
}

void KcpManager::clear()
{
	for (int tunnelId : m_mapTunnelId.keys())
		deleteKcpConnection(tunnelId);
}

void KcpManager::createKcpConnection(int tunnelId, QHostAddress hostAddress, quint16 port, quint32 magicNumber)
{
	const Peer peer(hostAddress, port);
	if (m_mapTunnelId.contains(tunnelId) || getConnectionByPeer(peer, true) != nullptr)
		return;

	ikcpcb * kcp = ikcp_create(magicNumber, this);
	ikcp_nodelay(kcp, 1, 10, 2, 1);
	ikcp_wndsize(kcp, 512, 512);
	kcp->output = KcpManager::kcp_write;
	ikcp_send(kcp, "H", 1);
	updateKcp(kcp);

	KcpConnection connection;
	connection.tunnelId = tunnelId;
	connection.peer = peer;
	connection.kcp = kcp;
	
	m_mapTunnelId[tunnelId] = kcp;
	m_map[kcp] = connection;
}

void KcpManager::deleteKcpConnection(int tunnelId)
{
	if (!m_mapTunnelId.contains(tunnelId))
		return;

	ikcpcb * kcp = m_mapTunnelId.value(tunnelId);
	m_map.remove(kcp);
	m_mapTunnelId.remove(tunnelId);

	ikcp_release(kcp);
}

void KcpManager::lowLevelInput(QHostAddress hostAddress, quint16 port, QByteArray package)
{
	package = checksumThenUnpackPackage(package);
	if (package.isEmpty())
		return;
	KcpConnection * connection = getConnectionByPeer(Peer(hostAddress, port), true);
	if (!connection)
		return;
	if (connection->peer.port == 0)
		connection->peer.port = port;
	ikcpcb * kcp = connection->kcp;
	ikcp_input(kcp, package.constData(), package.size());

	QByteArray outputBuffer;
	bool received = false;
	while (1)
	{
		char buffer[2000];
		const int bufferSize = ikcp_recv(kcp, buffer, sizeof(buffer));
		if (bufferSize <= 0)
			break;

		if (!connection->handShaked)
		{
			// 用发来的第一个字节作为握手标记
			if (buffer[0] != 'H')
			{
				deleteKcpConnection(connection->tunnelId);
				return;
			}
			connection->handShaked = true;
			outputBuffer.append(QByteArray::fromRawData(buffer + 1, bufferSize - 1));
		}else
		{
			outputBuffer.append(QByteArray::fromRawData(buffer, bufferSize));
		}
		received = true;
	}
	if (received)
	{
		emit highLevelOutput(connection->tunnelId, outputBuffer);
	}
}

int KcpManager::highLevelInput(int tunnelId, QByteArray package)
{
	QMap<int, ikcpcb*>::iterator iter = m_mapTunnelId.find(tunnelId);
	if (iter == m_mapTunnelId.end())
		return -1;

	ikcpcb * kcp = iter.value();

	int remainingSize = package.size();
	const char * ptr = package.constData();

	int writtenSize = 0;
	while (remainingSize > 0)
	{
		const int thisSize = qMin(remainingSize, (int)kcp->mss);
		if (ikcp_waitsnd(kcp) >= 2000)
			break;
		if (ikcp_send(kcp, ptr, thisSize) < 0)
			break;

		writtenSize += thisSize;
		ptr += thisSize;
		remainingSize -= thisSize;
	}

	return writtenSize;
}

void KcpManager::updateAllKcp()
{
	for (auto iter = m_map.begin(); iter != m_map.end(); ++iter)
	{
		ikcpcb * kcp = iter.key();
		updateKcp(kcp);
	}
}

int KcpManager::kcp_write(const char *buf, int len, ikcpcb *kcp, void *user)
{
	if (!user)
		return -1;
	return ((KcpManager*)user)->kcp_write(buf, len, kcp);
}

int KcpManager::kcp_write(const char *buf, int len, ikcpcb *kcp)
{
	QMap<ikcpcb*, KcpConnection>::iterator iter = m_map.find(kcp);
	if (iter == m_map.end())
		return -1;
	KcpConnection & connection = *iter;
	emit wannaLowLevelOutput(connection.tunnelId, connection.peer.address, connection.peer.port, addChecksumInfo(QByteArray::fromRawData(buf, len)));
	return 0;
}

KcpManager::KcpConnection * KcpManager::getConnectionByPeer(const Peer & peer, bool allowUncertain)
{
	if (!allowUncertain && peer.port == 0)
		return nullptr;
	for (QMap<ikcpcb*, KcpConnection>::iterator iter = m_map.begin();
		iter != m_map.end(); ++iter)
	{
		KcpConnection * connection = &(iter.value());
		if (peer.port != 0)
		{
			if (connection->peer.port == peer.port && connection->peer.address == peer.address)
				return connection;
		}
		else
		{
			if (connection->peer.address == peer.address)
				return connection;
		}
	}
	return nullptr;
}

void KcpManager::updateKcp(ikcpcb * kcp)
{
	ikcp_update(kcp, (IUINT32)(QDateTime::currentMSecsSinceEpoch() & 0xfffffffful));
}
