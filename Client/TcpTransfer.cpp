#include "TcpTransfer.h"
#include <QCryptographicHash>
#include "Other.h"

static const quint8 Direction_In = 1;
static const quint8 Direction_Out = 2;

static const int SocketMaxWaitingSize = 1024 * 1024;
static const int SocketReadBufferSize = 128 * 1024;

static const int GlobalMaxWaitingSize = 2 * 1024 * 1024;

static const int DataStreamMaxDataSize = 32768;

static inline quint8 getOppositeDirection(quint8 direction)
{
	switch (direction)
	{
	case Direction_In:
		return Direction_Out;
	case Direction_Out:
		return Direction_In;
	default:
		return 0;
	}
}

static const int HeaderSize = 4;

#pragma pack(push)
#pragma pack(1)

struct AddTransferFrame
{
	quint16 localPort;
	quint16 remotePort;
	char remoteAddressText[40];
};

struct DeleteTransferFrame
{
	quint16 localPort;
};

struct NewConnectionFrame
{
	quint16 localPort;
	quint32 socketIndex;
};

struct DisconnectConnectionFrame
{
	quint32 socketIndex;
	quint8 direction;
};

struct DataStreamFrame
{
	quint32 socketIndex;
	quint8 direction;
	quint8 isCompressed;
	char data[0];
};

struct AckFrame
{
	quint32 socketIndex;
	quint8 direction;
	int writtenSize;
};

#pragma pack(pop)

TcpTransfer::TcpTransfer(QObject *parent)
	: QObject(parent)
{
	m_timer5s.setParent(this);
	connect(&m_timer5s, SIGNAL(timeout()), this, SLOT(timerFunction5s()));
	m_lastOutTime = QTime::currentTime();
	m_timer5s.start(5 * 1000);
}

TcpTransfer::~TcpTransfer()
{
	m_timer5s.stop();
	for (SocketOutInfo socketOut : m_mapSocketOut.values())
		delete socketOut.obj;
	m_mapSocketOut.clear();
	for (SocketInInfo socketIn : m_mapSocketIn.values())
		delete socketIn.obj;
	m_mapSocketIn.clear();
	for (QTcpServer * tcpServer : m_mapTcpServer)
		delete tcpServer;
	m_mapTcpServer.clear();
}

void TcpTransfer::dataInput(QByteArray package)
{
	m_buffer.append(package);
	while (m_buffer.size() >= HeaderSize)
	{
		const FrameType frameType = (FrameType)*(quint16*)m_buffer.constData();
		const int frameSize = *(quint16*)(m_buffer.constData() + 2);
		if(m_buffer.size() < frameSize + HeaderSize)
			break;
		if (isValidFrameType(frameType))
		{
			const QByteArray frame(m_buffer.constData() + HeaderSize, frameSize);
			dealFrame(frameType, frame);
		}
		m_buffer = m_buffer.mid(frameSize + HeaderSize);
	}
}

bool TcpTransfer::addTransfer(quint16 localPort, quint16 remotePort, QHostAddress remoteAddress)
{
	if (m_mapTransferOut.contains(localPort))
	{
		const Peer remotePeer = m_mapTransferOut[localPort];
		if (remotePeer.address == remoteAddress &&  remotePeer.port == remotePort)
			return true;
		else
			return false;
	}
	QTcpServer * tcpServer = new QTcpServer();
	if (!tcpServer || !tcpServer->listen(QHostAddress::Any, localPort))
	{
		delete tcpServer;
		return false;
	}
	m_mapTcpServer[localPort] = tcpServer;
	m_mapTransferOut[localPort] = Peer(remoteAddress, remotePort);
	connect(tcpServer, SIGNAL(newConnection()), this, SLOT(onTcpNewConnection()));
	output_AddTransfer(localPort, remotePort, remoteAddress.toString());
	return true;
}

bool TcpTransfer::deleteTransfer(quint16 localPort)
{
	if (m_mapTransferOut.remove(localPort) == 0)
		return false;

	QTcpServer * tcpServer = m_mapTcpServer.value(localPort);
	if (tcpServer)
	{
		delete tcpServer;
		m_mapTcpServer.remove(localPort);
	}

	output_DeleteTransfer(localPort);
	return true;
}

bool TcpTransfer::isValidFrameType(FrameType frameType)
{
	return frameType > BeginUnknownFrameType && frameType < EndUnknownFrameType;
}

quint32 TcpTransfer::nextSocketIndex()
{
	const quint32 nextSocketIndex = m_nextSocketIndex;
	if (++m_nextSocketIndex == 0)
		m_nextSocketIndex = 1;
	return nextSocketIndex;
}

TcpTransfer::SocketInInfo * TcpTransfer::findSocketIn(const quint32 & peerSocketIndex)
{
	auto iter = m_mapSocketIn.find(peerSocketIndex);
	if (iter != m_mapSocketIn.end())
		return &(iter.value());
	else
		return nullptr;
}

TcpTransfer::SocketOutInfo * TcpTransfer::findSocketOut(const quint32 & socketIndex)
{
	auto iter = m_mapSocketOut.find(socketIndex);
	if (iter != m_mapSocketOut.end())
		return &(iter.value());
	else
		return nullptr;
}

void TcpTransfer::dealFrame(FrameType type, const QByteArray & frameData)
{
	if (type == HeartBeatType)
	{
		input_heartBeat();
	}else if (type == AddTransferType)
	{
		if (frameData.size() < sizeof(AddTransferFrame))
			return;
		const AddTransferFrame * frame = (const AddTransferFrame*)frameData.constData();
		input_AddTransfer(frame->localPort, frame->remotePort, frame->remoteAddressText);
	}
	else if (type == DeleteTransferType)
	{
		if (frameData.size() < sizeof(DeleteTransferFrame))
			return;
		const DeleteTransferFrame * frame = (const DeleteTransferFrame*)frameData.constData();
		input_DeleteTransfer(frame->localPort);
	}
	else if (type == NewConnectionType)
	{
		if (frameData.size() < sizeof(NewConnectionFrame))
			return;
		const NewConnectionFrame * frame = (const NewConnectionFrame*)frameData.constData();
		input_NewConnection(frame->localPort, frame->socketIndex);
	}
	else if (type == DisconnectConnectionType)
	{
		if (frameData.size() < sizeof(DisconnectConnectionFrame))
			return;
		const DisconnectConnectionFrame * frame = (const DisconnectConnectionFrame*)frameData.constData();
		input_DisconnectConnection(frame->socketIndex, frame->direction);
	}
	else if (type == DataStreamType)
	{
		if (frameData.size() < sizeof(DataStreamFrame))
			return;
		const DataStreamFrame * frame = (const DataStreamFrame*)frameData.constData();
		input_DataStream(frame->socketIndex, frame->direction, frame->data, frameData.size() - sizeof(DataStreamFrame));
	}
	else if (type == AckType)
	{
		if (frameData.size() < sizeof(AckType))
			return;
		const AckFrame * frame = (const AckFrame*)frameData.constData();
		input_Ack(frame->socketIndex, frame->direction, frame->writtenSize);
	}
}

void TcpTransfer::input_heartBeat()
{

}

void TcpTransfer::input_AddTransfer(quint16 localPort, quint16 remotePort, QString remoteAddressText)
{
	if (remoteAddressText.isEmpty())
		return;
	QHostAddress remoteAddress(remoteAddressText);
	if (localPort && remotePort && !remoteAddress.isNull())
		m_mapTransferIn[localPort] = Peer(remoteAddress, remotePort);
}

void TcpTransfer::input_DeleteTransfer(quint16 localPort)
{
	if (localPort)
		m_mapTransferIn.remove(localPort);
}

void TcpTransfer::input_NewConnection(quint16 localPort, quint32 socketIndex)
{
	if (localPort && socketIndex && m_mapTransferIn.contains(localPort))
	{
		const Peer peer = m_mapTransferIn.value(localPort);
		Q_ASSERT(!m_mapSocketIn.contains(socketIndex));
		SocketInInfo & socketIn = m_mapSocketIn[socketIndex];
		if (socketIn.obj)
			delete socketIn.obj;
		socketIn.cachedData.clear();

		socketIn.obj = new QTcpSocket();
		socketIn.obj->setReadBufferSize(SocketReadBufferSize);
		socketIn.peerSocketIndex = socketIndex;
		connect(socketIn.obj, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
			this, SLOT(onSocketInStateChanged(QAbstractSocket::SocketState)));
		connect(socketIn.obj, SIGNAL(readyRead()), this, SLOT(onSocketInReadyRead()));
		connect(socketIn.obj, SIGNAL(bytesWritten(qint64)), this, SLOT(onSocketInBytesWritten(qint64)));
		socketIn.obj->setProperty("peerSocketIndex", socketIndex);
		socketIn.obj->connectToHost(peer.address, peer.port);
	}
	qDebug() << QString("TcpTransfer::input_NewConnection localPort=%1 socketIndex=%2")
		.arg(localPort).arg(socketIndex);
}

void TcpTransfer::input_DisconnectConnection(quint32 socketIndex, quint8 direction)
{
	direction = getOppositeDirection(direction);
	if (direction == Direction_Out)
	{
		qDebug() << QString("TcpTransfer::input_DisconnectConnection socketOut disconnected socketIndex=%1")
			.arg(socketIndex);

		if (SocketOutInfo * socketOut = findSocketOut(socketIndex))
			delete socketOut->obj;
		m_lstGlobalWaitingSocket.removeOne(SocketIdentifier(socketIndex, Direction_Out));
	}
	else if (direction == Direction_In)
	{
		const quint32 & peerSocketIndex = socketIndex;
		qDebug() << QString("TcpTransfer::input_DisconnectConnection socketIn disconnected peerSocketIndex=%1")
			.arg(peerSocketIndex);

		if (SocketInInfo * socketIn = findSocketIn(peerSocketIndex))
			delete socketIn->obj;
		m_lstGlobalWaitingSocket.removeOne(SocketIdentifier(socketIndex, Direction_In));
	}
}

void TcpTransfer::input_DataStream(quint32 socketIndex, quint8 direction, const char * data, int dataSize)
{
	direction = getOppositeDirection(direction);
	if (direction == Direction_Out)
	{
		if (SocketOutInfo * socketOut = findSocketOut(socketIndex))
			socketOut->obj->write(data, dataSize);
	}
	else if (direction == Direction_In)
	{
		const quint32 & peerSocketIndex = socketIndex;
		if (SocketInInfo * socketIn = findSocketIn(peerSocketIndex))
		{
			if (socketIn->obj->state() == QAbstractSocket::ConnectedState)
				socketIn->obj->write(data, dataSize);
			else
				socketIn->cachedData.append(data, dataSize);
		}
	}
	output_Ack(0, 0, dataSize);
}

void TcpTransfer::input_Ack(quint32 socketIndex, quint8 direction, int writtenSize)
{
	if (direction == 0 && socketIndex == 0)
	{
		// 这是全局流控
		m_globalWaitingSize -= writtenSize;
		if (m_globalWaitingSize < 0)
		{
			m_globalWaitingSize = 0;
			qWarning() << QString("TcpTransfer::input_Ack m_globalWaitingSize < 0");
		}
		while (m_globalWaitingSize < GlobalMaxWaitingSize && m_lstGlobalWaitingSocket.size() > 0)
		{
			const SocketIdentifier & socketIdentifier = m_lstGlobalWaitingSocket.first();
			if(socketIdentifier.direction == Direction_In)
			{ 
				if (SocketInInfo * socketIn = findSocketIn(socketIdentifier.peerSocketIndex))
					readAndSendSocketIn(*socketIn);
			}
			else if (socketIdentifier.direction == Direction_Out)
			{
				if (SocketOutInfo * socketOut = findSocketOut(socketIdentifier.socketIndex))
					readAndSendSocketOut(*socketOut);
			}
			m_lstGlobalWaitingSocket.removeFirst();
		}
	}
	else
	{
		direction = getOppositeDirection(direction);
		if (direction == Direction_Out)
		{
			if (SocketOutInfo * socketOut = findSocketOut(socketIndex))
			{
				socketOut->peerWaitingSize -= writtenSize;
				if (socketOut->peerWaitingSize < 0)
				{
					socketOut->peerWaitingSize = 0;
					qWarning() << QString("TcpTransfer::input_Ack out peerWaitingSize < 0, socketIndex=%1")
						.arg(socketIndex);
				}
				readAndSendSocketOut(*socketOut);
			}
		}
		else if (direction == Direction_In)
		{
			const quint32 & peerSocketIndex = socketIndex;
			if (SocketInInfo * socketIn = findSocketIn(peerSocketIndex))
			{
				socketIn->peerWaitingSize -= writtenSize;
				if (socketIn->peerWaitingSize < 0)
				{
					socketIn->peerWaitingSize = 0;
					qWarning() << QString("TcpTransfer::input_Ack in peerWaitingSize < 0, peerSocketIndex=%1")
						.arg(peerSocketIndex);
				}
				readAndSendSocketIn(*socketIn);
			}
		}
	}
}

void TcpTransfer::outputFrame(FrameType type, const QByteArray & frameData, const char * extraData, int extraDataSize)
{
	if (extraDataSize < 0)
		extraDataSize = 0;

	const int totalSize = frameData.size() + extraDataSize;
	if (totalSize > 65535)
	{
		qCritical() << QString("TcpTransfer::outputFrame totalSize %1 too big").arg(totalSize);
		return;
	}

	quint16 header[2] = { 0 };
	header[0] = (quint16)type;
	header[1] = (quint16)totalSize;

	QByteArray package = frameData;
	package.insert(0, QByteArray::fromRawData((const char*)&header, sizeof(header)));

	emit dataOutput(package);
	if (extraDataSize > 0)
		emit dataOutput(QByteArray::fromRawData(extraData, extraDataSize));

	m_lastOutTime = QTime::currentTime();
}

void TcpTransfer::output_heartBeat()
{
	outputFrame(HeartBeatType, QByteArray());
}

void TcpTransfer::output_AddTransfer(quint16 localPort, quint16 remotePort, QString remoteAddressText)
{
	AddTransferFrame frame = { 0 };
	frame.localPort = localPort;
	frame.remotePort = remotePort;
	strcpy_(frame.remoteAddressText, remoteAddressText.toUtf8().constData());
	outputFrame(AddTransferType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DeleteTransfer(quint16 localPort)
{
	DeleteTransferFrame frame;
	frame.localPort = localPort;
	outputFrame(DeleteTransferType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_NewConnection(quint16 localPort, quint32 socketIndex)
{
	NewConnectionFrame frame;
	frame.localPort = localPort;
	frame.socketIndex = socketIndex;
	outputFrame(NewConnectionType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DisconnectConnection(quint32 socketIndex, quint8 direction)
{
	DisconnectConnectionFrame frame;
	frame.socketIndex = socketIndex;
	frame.direction = direction;
	outputFrame(DisconnectConnectionType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DataStream(quint32 socketIndex, quint8 direction, const char * data, int dataSize)
{
	DataStreamFrame frame;
	frame.socketIndex = socketIndex;
	frame.direction = direction;
	frame.isCompressed = 0;
	outputFrame(DataStreamType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)), data, dataSize);
}

void TcpTransfer::output_Ack(quint32 socketIndex, quint8 direction, int writtenSize)
{
	AckFrame frame;
	frame.socketIndex = socketIndex;
	frame.direction = direction;
	frame.writtenSize = writtenSize;
	outputFrame(AckType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

int TcpTransfer::readAndSendSocketOut(SocketOutInfo & socketOut)
{
	int totalSendSize = 0;
	while (1)
	{
		const int availableSize = socketOut.obj->bytesAvailable();
		if (availableSize == 0)
			break;
		const int singleMaxSendSize = qMin(SocketMaxWaitingSize - socketOut.peerWaitingSize, DataStreamMaxDataSize);
		if (singleMaxSendSize <= 0)
			break;
		const int globalMaxSendSize = GlobalMaxWaitingSize - m_globalWaitingSize;
		// 如果全局流控允许发送的数量比自身流控更少，并且availableSize大于全局流控允许的数量，就认为这次的限制发送是全局流控导致的
		if (globalMaxSendSize < singleMaxSendSize && availableSize > globalMaxSendSize)
		{
			SocketIdentifier socketIdentifier(socketOut.socketIndex, Direction_Out);
			if (!m_lstGlobalWaitingSocket.contains(socketIdentifier))
			{
				m_lstGlobalWaitingSocket << socketIdentifier;
				qWarning() << QString("TcpTransfer::readAndSendSocketOut %1 global waiting").arg(socketOut.socketIndex);
			}
		}
		const int maxSendSize = qMin(globalMaxSendSize, singleMaxSendSize);
		if (maxSendSize <= 0)
			break;

		QByteArray bytes = socketOut.obj->read(maxSendSize);
		socketOut.peerWaitingSize += bytes.size();
		m_globalWaitingSize += bytes.size();
		output_DataStream(socketOut.socketIndex, Direction_Out, bytes.constData(), bytes.size());
		totalSendSize += bytes.size();

		// 发送数量小于单次最大，说明要么全部发完，要么受到了流控
		if(bytes.size() < DataStreamMaxDataSize)
			break;
	}
	return totalSendSize;
}

int TcpTransfer::readAndSendSocketIn(SocketInInfo & socketIn)
{
	int totalSendSize = 0;
	while (1)
	{
		const int availableSize = socketIn.obj->bytesAvailable();
		if (availableSize == 0)
			break;
		const int singleMaxSendSize = qMin(SocketMaxWaitingSize - socketIn.peerWaitingSize, DataStreamMaxDataSize);
		if (singleMaxSendSize <= 0)
			break;
		const int globalMaxSendSize = GlobalMaxWaitingSize - m_globalWaitingSize;
		if (globalMaxSendSize < singleMaxSendSize && availableSize > globalMaxSendSize)
		{
			SocketIdentifier socketIdentifier(socketIn.peerSocketIndex, Direction_Out);
			if (!m_lstGlobalWaitingSocket.contains(socketIdentifier))
			{
				m_lstGlobalWaitingSocket << socketIdentifier;
				qWarning() << QString("TcpTransfer::readAndSendSocketIn %1 global waiting").arg(socketIn.peerSocketIndex);
			}
		}
		const int maxSendSize = qMin(globalMaxSendSize, singleMaxSendSize);
		if (maxSendSize <= 0)
			break;

		QByteArray bytes = socketIn.obj->read(maxSendSize);
		socketIn.peerWaitingSize += bytes.size();
		m_globalWaitingSize += bytes.size();
		output_DataStream(socketIn.peerSocketIndex, Direction_In, bytes.constData(), bytes.size());
		totalSendSize += bytes.size();

		if (bytes.size() < DataStreamMaxDataSize)
			break;
	}
	return totalSendSize;
}

void TcpTransfer::timerFunction5s()
{
	if (!m_lastOutTime.isValid() || m_lastOutTime.elapsed() > 10 * 1000)
		output_heartBeat();
}

void TcpTransfer::onSocketInStateChanged(QAbstractSocket::SocketState state)
{
	QTcpSocket * tcpSocket = (QTcpSocket*)sender();
	if (!tcpSocket)
		return;
	if (state == QAbstractSocket::UnconnectedState)
	{
		// UnconnectedState状态表示连接失败或断开连接
		const quint32 peerSocketIndex = tcpSocket->property("peerSocketIndex").toLongLong();
		if (SocketInInfo * socketIn = findSocketIn(peerSocketIndex))
		{
			socketIn->obj->deleteLater();
			socketIn->obj = nullptr;
			m_mapSocketIn.remove(peerSocketIndex);
			output_DisconnectConnection(peerSocketIndex, Direction_In);
		}
		m_lstGlobalWaitingSocket.removeOne(SocketIdentifier(peerSocketIndex, Direction_In));
		qDebug() << QString("TcpTransfer::onSocketInStateChanged disconnected peerSocketIndex=%1").arg(peerSocketIndex);
	}
	else if (state == QAbstractSocket::ConnectedState)
	{
		const quint32 peerSocketIndex = tcpSocket->property("peerSocketIndex").toLongLong();
		SocketInInfo * socketIn = findSocketIn(peerSocketIndex);
		if (!socketIn)
			return;
		if (socketIn->cachedData.size() > 0)
		{
			socketIn->obj->write(socketIn->cachedData);
			socketIn->cachedData.clear();
		}
		qDebug() << QString("TcpTransfer::onSocketInStateChanged connected peerSocketIndex=%1").arg(peerSocketIndex);
	}
}

void TcpTransfer::onSocketOutStateChanged(QAbstractSocket::SocketState state)
{
	QTcpSocket * socketOut = (QTcpSocket*)sender();
	if (!socketOut)
		return;
	if (state == QAbstractSocket::UnconnectedState)
	{
		const quint32 socketIndex = socketOut->property("socketIndex").toLongLong();
		if (m_mapSocketOut.contains(socketIndex))
		{
			m_mapSocketOut.remove(socketIndex);
			socketOut->deleteLater();
			output_DisconnectConnection(socketIndex, Direction_Out);
		}
		m_lstGlobalWaitingSocket.removeOne(SocketIdentifier(socketIndex, Direction_Out));
		qDebug() << QString("TcpTransfer::onSocketOutStateChanged disconnected socketIndex=%1").arg(socketIndex);
	}
}

void TcpTransfer::onTcpNewConnection()
{
	QTcpServer * tcpServer = (QTcpServer*)sender();
	if (!tcpServer)
		return;
	const quint16 localPort = tcpServer->serverPort();
	while (tcpServer->hasPendingConnections())
	{
		QTcpSocket * socketOutObj = tcpServer->nextPendingConnection();
		const quint32 socketIndex = nextSocketIndex();

		Q_ASSERT(!m_mapSocketOut.contains(socketIndex));
		SocketOutInfo & socketOut = m_mapSocketOut[socketIndex];
		if (socketOut.obj)
			delete socketOut.obj;

		socketOut.obj = socketOutObj;
		socketOut.socketIndex = socketIndex;

		socketOut.obj->setReadBufferSize(SocketReadBufferSize);
		socketOut.obj->setProperty("socketIndex", socketIndex);
		connect(socketOut.obj, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
			this, SLOT(onSocketOutStateChanged(QAbstractSocket::SocketState)));
		connect(socketOut.obj, SIGNAL(readyRead()), this, SLOT(onSocketOutReadyRead()));
		connect(socketOut.obj, SIGNAL(bytesWritten(qint64)), this, SLOT(onSocketOutBytesWritten(qint64)));
		output_NewConnection(localPort, socketIndex);

		qDebug() << QString("TcpTransfer::onTcpNewConnection localPort=%1 socketIndex=%2")
			.arg(localPort).arg(socketIndex);
	}
}

void TcpTransfer::onSocketOutReadyRead()
{
	QTcpSocket * socketOutObj = (QTcpSocket*)sender();
	if (!socketOutObj)
		return;
	const quint32 socketIndex = socketOutObj->property("socketIndex").toUInt();
	if (SocketOutInfo * socketOut = findSocketOut(socketIndex))
	{
		readAndSendSocketOut(*socketOut);
	}
	else
	{
		qCritical() << QString("TcpTransfer::onSocketOutReadyRead unknown socketIndex %1").arg(socketIndex);
		socketOutObj->deleteLater();
	}	
}

void TcpTransfer::onSocketInReadyRead()
{
	QTcpSocket * socketInObj = (QTcpSocket*)sender();
	if (!socketInObj)
		return;
	const quint32 peerSocketIndex = socketInObj->property("peerSocketIndex").toLongLong();

	if (SocketInInfo * socketIn = findSocketIn(peerSocketIndex))
	{
		readAndSendSocketIn(*socketIn);
	}
	else
	{
		qCritical() << QString("TcpTransfer::onSocketOutReadyRead unknown peerSocketIndex %1").arg(peerSocketIndex);
		socketInObj->deleteLater();
	}
}

void TcpTransfer::onSocketOutBytesWritten(qint64 size)
{
	QTcpSocket * socketOut = (QTcpSocket*)sender();
	if (!socketOut)
		return;
	const quint32 socketIndex = socketOut->property("socketIndex").toUInt();
	if (socketIndex == 0)
		return;
	output_Ack(socketIndex, Direction_Out, size);
}

void TcpTransfer::onSocketInBytesWritten(qint64 size)
{
	QTcpSocket * socketIn = (QTcpSocket*)sender();
	if (!socketIn)
		return;
	const quint32 peerSocketIndex = socketIn->property("peerSocketIndex").toLongLong();
	if (peerSocketIndex == 0)
		return;
	output_Ack(peerSocketIndex, Direction_In, size);
}
