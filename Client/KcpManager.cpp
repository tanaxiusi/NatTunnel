#include "KcpManager.h"
#include <QDateTime>
#include "Other.h"

static const QByteArray ShakeHandHeader1 = "Hello";
static const QByteArray ShakeHandHeader2 = "Ok";

static const int ShakeHand_Unknown = 0;
static const int ShakeHand_Partly = 1;
static const int ShakeHand_Finished = 2;

static inline quint32 getCurrentTime()
{
	return (quint32)(QDateTime::currentMSecsSinceEpoch() & 0xfffffffful);
}

KcpManager::KcpManager(QObject *parent)
	: QObject(parent)
{
	m_timer10ms.setParent(this);
	m_timer5s.setParent(this);
	m_cachedCurrentTime = QTime::currentTime();

	connect(&m_timer10ms, SIGNAL(timeout()), this, SLOT(timerFunction10ms()));
	connect(&m_timer5s, SIGNAL(timeout()), this, SLOT(timerFunction5s()));

	m_timer10ms.start(10);
	m_timer5s.start(5 * 1000);
}

KcpManager::~KcpManager()
{
	clear();
}

void KcpManager::setUserName(QString userName)
{
	m_userName = userName.toUtf8();
}

void KcpManager::clear()
{
	for (int tunnelId : m_mapTunnelId.keys())
		deleteKcpConnection(tunnelId);
}

bool KcpManager::haveUnconfirmedKcpConnection()
{
	for (QMap<ikcpcb*, KcpConnection>::iterator iter = m_map.begin();
		iter != m_map.end(); ++iter)
	{
		KcpConnection * connection = &(iter.value());
		if (!connection->peerConfirmed)
			return true;
	}
	return false;
}

bool KcpManager::createKcpConnection(int tunnelId, QHostAddress hostAddress, quint16 port, QString userName, quint32 magicNumber)
{
	const Peer peer(hostAddress, port);
	if (m_mapTunnelId.contains(tunnelId) || getConnectionByPeer(peer) != nullptr)
		return false;

	ikcpcb * kcp = ikcp_create(magicNumber, this);
	ikcp_nodelay(kcp, 1, 10, 2, 1);
	ikcp_wndsize(kcp, 512, 512);
	kcp->output = KcpManager::kcp_write;

	KcpConnection connection;
	connection.tunnelId = tunnelId;
	connection.userName = userName.toUtf8();
	connection.peer = peer;
	connection.kcp = kcp;
	
	m_mapTunnelId[tunnelId] = kcp;
	m_map[kcp] = connection;

	ikcp_send(kcp, ShakeHandHeader1.constData(), ShakeHandHeader1.size());
	updateKcp(m_map[kcp], getCurrentTime());
	return true;
}

bool KcpManager::deleteKcpConnection(int tunnelId)
{
	if (!m_mapTunnelId.contains(tunnelId))
		return false;

	ikcpcb * kcp = m_mapTunnelId.value(tunnelId);
	m_map.remove(kcp);
	m_mapTunnelId.remove(tunnelId);

	ikcp_release(kcp);

	emit disconnected(tunnelId);
	return true;
}

void KcpManager::lowLevelInput(QHostAddress hostAddress, quint16 port, QByteArray package)
{
	if (package.isEmpty())
		return;
	KcpConnection & connection = *getConnectionByPeer(Peer(hostAddress, port));
	if (&connection == nullptr)
		return;
	package = checksumThenUnpackPackage(package, connection.userName);
	if (package.isEmpty())
	{
		qWarning() << QString("KcpManager::lowLevelInput corrupted package from %1 %2 where userName=%3")
			.arg(hostAddress.toString()).arg(port).arg((QString)connection.userName);
		return;
	}
	if (!connection.peerConfirmed)
	{
		connection.peer.address = hostAddress;
		connection.peer.port = port;
		connection.peerConfirmed = true;
	}
	connection.lastInTime = m_cachedCurrentTime;

	ikcpcb * kcp = connection.kcp;
	ikcp_input(kcp, package.constData(), package.size());
	updateKcp(connection, getCurrentTime());

	QByteArray outputBuffer;
	bool received = false;
	while (1)
	{
		char buffer[2000];
		const int bufferSize = ikcp_recv(kcp, buffer, sizeof(buffer));
		if (bufferSize <= 0)
			break;

		if (connection.shakeHandStep != ShakeHand_Finished)
		{
			connection.buffer.append(QByteArray::fromRawData(buffer, bufferSize));
			if (connection.shakeHandStep == ShakeHand_Unknown && connection.buffer.size() >= ShakeHandHeader1.size())
			{
				if (connection.buffer.startsWith(ShakeHandHeader1))
				{
					connection.shakeHandStep = ShakeHand_Partly;
					connection.buffer = connection.buffer.mid(ShakeHandHeader1.size());
					ikcp_send(kcp, ShakeHandHeader2.constData(), ShakeHandHeader2.size());
				}
				else
				{
					deleteKcpConnection(connection.tunnelId);
					return;
				}
			}
			if (connection.shakeHandStep == ShakeHand_Partly && connection.buffer.size() >= ShakeHandHeader2.size())
			{
				if (connection.buffer.startsWith(ShakeHandHeader2))
				{
					connection.shakeHandStep = ShakeHand_Finished;
					connection.buffer = connection.buffer.mid(ShakeHandHeader2.size());
					if (connection.buffer.size() > 0)
					{
						outputBuffer.append(connection.buffer);
						received = true;
					}
					connection.buffer.clear();
					emit handShaked(connection.tunnelId);
				}
				else
				{
					deleteKcpConnection(connection.tunnelId);
					return;
				}
			}
		}else
		{
			outputBuffer.append(QByteArray::fromRawData(buffer, bufferSize));
			received = true;
		}
	}
	if (received)
	{
		emit highLevelOutput(connection.tunnelId, outputBuffer);
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
		const int waitsnd = ikcp_waitsnd(kcp);
		if (waitsnd > 2000)
			qWarning() << "ikcp_waitsnd" << waitsnd;
		if (ikcp_send(kcp, ptr, thisSize) < 0)
			break;

		writtenSize += thisSize;
		ptr += thisSize;
		remainingSize -= thisSize;
	}

	if (writtenSize > 0)
		ikcp_flush(kcp);

	return writtenSize;
}

void KcpManager::timerFunction10ms()
{
	m_cachedCurrentTime = QTime::currentTime();
	const quint32 currentTime = getCurrentTime();
	for (auto iter = m_map.begin(); iter != m_map.end(); ++iter)
	{
		ikcpcb * kcp = iter.key();
		KcpConnection & connection = iter.value();
		quint32 diff = currentTime - connection.nextUpdateTime;
		if(diff < 0x3fffffffu)		// currentTime - nextUpdateTime < 1/4 32位无符号整数，即判定为大于
			updateKcp(connection, currentTime);
	}
}

void KcpManager::timerFunction5s()
{
	QList<int> timeoutConnections;
	for (auto iter = m_map.begin(); iter != m_map.end(); ++iter)
	{
		ikcpcb * kcp = iter.key();
		KcpConnection & connection = iter.value();
		if (connection.lastInTime.elapsed() > 15 * 1000)
			timeoutConnections << connection.tunnelId;
	}
	if (timeoutConnections.isEmpty())
		return;
	for (int tunnelId : timeoutConnections)
		deleteKcpConnection(tunnelId);
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
	const QByteArray package = addChecksumInfo(QByteArray::fromRawData(buf, len), m_userName);
	emit wannaLowLevelOutput(connection.tunnelId, connection.peer.address, connection.peer.port, package);
	return 0;
}

KcpManager::KcpConnection * KcpManager::getConnectionByPeer(const Peer & peer)
{
	for (QMap<ikcpcb*, KcpConnection>::iterator iter = m_map.begin();
		iter != m_map.end(); ++iter)
	{
		KcpConnection * connection = &(iter.value());
		if (!connection->peerConfirmed)
			return connection;
		else if(connection->peer.address == peer.address && connection->peer.port == peer.port)
			return connection;
	}
	return nullptr;
}

void KcpManager::updateKcp(KcpConnection & connection, quint32 currentTime)
{
	ikcp_update(connection.kcp, currentTime);
	connection.nextUpdateTime = ikcp_check(connection.kcp, currentTime);
}
