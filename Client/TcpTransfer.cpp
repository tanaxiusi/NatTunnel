#include "TcpTransfer.h"
#include <QCryptographicHash>
#include "Other.h"

static const quint8 Direction_In = 1;
static const quint8 Direction_Out = 2;

static const int HeaderSize = 4;

#pragma pack(push)
#pragma pack(1)

struct AddTransferFrame
{
	quint16 localPort;
	quint16 remoteDestPort;
	char remoteDestAddressText[40];
};

struct DeleteTransferFrame
{
	quint16 localPort;
};

struct NewConnectionFrame
{
	quint16 localPort;
	qint64 socketDescriptor;
};

struct DisconnectConnectionFrame
{
	qint64 socketDescriptor;
	quint8 direction;
};

struct DataStreamFrame
{
	qint64 socketDescriptor;
	quint8 direction;
	char data[0];
};

#pragma pack(pop)

TcpTransfer::TcpTransfer(QObject *parent)
	: QObject(parent)
{

}

TcpTransfer::~TcpTransfer()
{
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

bool TcpTransfer::addTransfer(quint16 localPort, quint16 remoteDestPort, QHostAddress remoteDestAddress)
{
	if (m_mapTransferOut.contains(localPort))
		return false;
	QTcpServer * tcpServer = new QTcpServer();
	if (!tcpServer || !tcpServer->listen(QHostAddress::Any, localPort))
	{
		delete tcpServer;
		return false;
	}
	m_mapTcpServer[localPort] = tcpServer;
	m_mapTransferOut[localPort] = Peer(remoteDestAddress, remoteDestPort);
	connect(tcpServer, SIGNAL(newConnection()), this, SLOT(onTcpNewConnection()));
	output_AddTransfer(localPort, remoteDestPort, remoteDestAddress.toString());
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

void TcpTransfer::dealFrame(FrameType type, const QByteArray & frameData)
{
	if (type == AddTransferType)
	{
		if (frameData.size() < sizeof(AddTransferFrame))
			return;
		const AddTransferFrame * frame = (const AddTransferFrame*)frameData.constData();
		input_AddTransfer(frame->localPort, frame->remoteDestPort, frame->remoteDestAddressText);
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
		input_NewConnection(frame->localPort, frame->socketDescriptor);
	}
	else if (type == DisconnectConnectionType)
	{
		if (frameData.size() < sizeof(DisconnectConnectionFrame))
			return;
		const DisconnectConnectionFrame * frame = (const DisconnectConnectionFrame*)frameData.constData();
		input_DisconnectConnection(frame->socketDescriptor, frame->direction);
	}
	else if (type == DataStreamType)
	{
		if (frameData.size() < sizeof(DataStreamFrame))
			return;
		const DataStreamFrame * frame = (const DataStreamFrame*)frameData.constData();
		input_DataStream(frame->socketDescriptor, frame->direction, frame->data, frameData.size() - sizeof(DataStreamFrame));
	}
}

void TcpTransfer::input_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText)
{
	if (remoteDestAddressText.isEmpty())
		return;
	QHostAddress remoteDestAddress(remoteDestAddressText);
	if (localPort && remoteDestPort && !remoteDestAddress.isNull())
		m_mapTransferIn[localPort] = Peer(remoteDestAddress, remoteDestPort);
}

void TcpTransfer::input_DeleteTransfer(quint16 localPort)
{
	if (localPort)
		m_mapTransferIn.remove(localPort);
}

void TcpTransfer::input_NewConnection(quint16 localPort, qint64 socketDescriptor)
{
	if (localPort && socketDescriptor && m_mapTransferIn.contains(localPort))
	{
		const Peer peer = m_mapTransferIn.value(localPort);
		Q_ASSERT(!m_mapSocketIn.contains(socketDescriptor));
		SocketInInfo & socketIn = m_mapSocketIn[socketDescriptor];
		if (socketIn.obj)
			delete socketIn.obj;
		socketIn.cachedData.clear();

		socketIn.obj = new QTcpSocket();
		socketIn.peerSocketDescriptor = socketDescriptor;
		connect(socketIn.obj, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
			this, SLOT(onSocketInStateChanged(QAbstractSocket::SocketState)));
		connect(socketIn.obj, SIGNAL(readyRead()), this, SLOT(onSocketInReadyRead()));
		socketIn.obj->setProperty("peerSocketDescriptor", socketDescriptor);
		socketIn.obj->connectToHost(peer.address, peer.port);
	}
	qDebug() << QString("TcpTransfer::input_NewConnection localPort=%1 socketDescriptor=%2")
		.arg(localPort).arg(socketDescriptor);
}

void TcpTransfer::input_DisconnectConnection(qint64 socketDescriptor, quint8 direction)
{
	// 对面传进来的方向是相反的
	if (direction == Direction_In)
	{
		qDebug() << QString("TcpTransfer::input_DisconnectConnection socketOut disconnected socketDescriptor=%1")
			.arg(socketDescriptor);

		QTcpSocket * socketOut = m_mapSocketOut.value(socketDescriptor);
		if (socketOut)
			delete socketOut;
	}
	else if (direction == Direction_Out)
	{
		qDebug() << QString("TcpTransfer::input_DisconnectConnection socketIn disconnected peerSocketDescriptor=%1")
			.arg(socketDescriptor);

		auto iter = m_mapSocketIn.find(socketDescriptor);
		if (iter != m_mapSocketIn.end())
		{
			SocketInInfo & socketIn = iter.value();
			delete socketIn.obj;
		}
	}
}

void TcpTransfer::input_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize)
{
	// 对面传进来的方向是相反的
	if (direction == Direction_In)
	{
		QTcpSocket * socketOut = m_mapSocketOut.value(socketDescriptor);
		if (socketOut)
			socketOut->write(data, dataSize);
	}
	else if (direction == Direction_Out)
	{
		auto iter = m_mapSocketIn.find(socketDescriptor);
		if (iter != m_mapSocketIn.end())
		{
			SocketInInfo & socketIn = iter.value();
			if (socketIn.obj->state() == QAbstractSocket::ConnectedState)
				socketIn.obj->write(data, dataSize);
			else
				socketIn.cachedData.append(data, dataSize);
		}
	}
}

void TcpTransfer::outputFrame(FrameType type, const QByteArray & frameData, const char * extraData, int extraDataSize)
{
	if (extraDataSize < 0)
		extraDataSize = 0;

	const int totalSize = frameData.size() + extraDataSize;
	if (totalSize > 65535)
		return;

	quint16 header[2] = { 0 };
	header[0] = (quint16)type;
	header[1] = (quint16)totalSize;

	QByteArray package = frameData;
	package.insert(0, QByteArray::fromRawData((const char*)&header, sizeof(header)));

	if (extraDataSize > 0)
		package.append(QByteArray::fromRawData(extraData, extraDataSize));

	emit dataOutput(package);
	//if (extraDataSize > 0)
	//	emit dataOutput(QByteArray::fromRawData(extraData, extraDataSize));
}

void TcpTransfer::output_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText)
{
	AddTransferFrame frame = { 0 };
	frame.localPort = localPort;
	frame.remoteDestPort = remoteDestPort;
	strcpy_(frame.remoteDestAddressText, remoteDestAddressText.toUtf8().constData());
	outputFrame(AddTransferType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DeleteTransfer(quint16 localPort)
{
	DeleteTransferFrame frame;
	frame.localPort = localPort;
	outputFrame(DeleteTransferType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_NewConnection(quint16 localPort, qint64 socketDescriptor)
{
	NewConnectionFrame frame;
	frame.localPort = localPort;
	frame.socketDescriptor = socketDescriptor;
	outputFrame(NewConnectionType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DisconnectConnection(qint64 socketDescriptor, quint8 direction)
{
	DisconnectConnectionFrame frame;
	frame.socketDescriptor = socketDescriptor;
	frame.direction = direction;
	outputFrame(DisconnectConnectionType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)));
}

void TcpTransfer::output_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize)
{
	DataStreamFrame frame;
	frame.socketDescriptor = socketDescriptor;
	frame.direction = direction;
	outputFrame(DataStreamType, QByteArray::fromRawData((const char*)&frame, sizeof(frame)), data, dataSize);
}

void TcpTransfer::onSocketInStateChanged(QAbstractSocket::SocketState state)
{
	QTcpSocket * tcpSocket = (QTcpSocket*)sender();
	if (!tcpSocket)
		return;
	if (state == QAbstractSocket::UnconnectedState)
	{
		// UnconnectedState状态表示连接失败或断开连接
		const qint64 peerSocketDescriptor = tcpSocket->property("peerSocketDescriptor").toLongLong();
		auto iter = m_mapSocketIn.find(peerSocketDescriptor);
		if (iter != m_mapSocketIn.end())
		{
			SocketInInfo & socketIn = iter.value();
			socketIn.obj->deleteLater();
			socketIn.obj = nullptr;
			m_mapSocketIn.erase(iter);
			output_DisconnectConnection(peerSocketDescriptor, Direction_In);
		}
		qDebug() << QString("TcpTransfer::onSocketInStateChanged disconnected peerSocketDescriptor=%1").arg(peerSocketDescriptor);
	}
	else if (state == QAbstractSocket::ConnectedState)
	{
		const qint64 peerSocketDescriptor = tcpSocket->property("peerSocketDescriptor").toLongLong();
		auto iter = m_mapSocketIn.find(peerSocketDescriptor);
		if (iter == m_mapSocketIn.end())
			return;
		SocketInInfo & socketIn = iter.value();
		if (socketIn.cachedData.size() > 0)
		{
			socketIn.obj->write(socketIn.cachedData);
			socketIn.cachedData.clear();
		}
		qDebug() << QString("TcpTransfer::onSocketInStateChanged connected peerSocketDescriptor=%1").arg(peerSocketDescriptor);
	}
}

void TcpTransfer::onSocketOutStateChanged(QAbstractSocket::SocketState state)
{
	QTcpSocket * socketOut = (QTcpSocket*)sender();
	if (!socketOut)
		return;
	if (state == QAbstractSocket::UnconnectedState)
	{
		const qint64 socketDescriptor = socketOut->property("socketDescriptor").toLongLong();
		if (m_mapSocketOut.contains(socketDescriptor))
		{
			m_mapSocketOut.remove(socketDescriptor);
			socketOut->deleteLater();
			output_DisconnectConnection(socketDescriptor, Direction_Out);
		}
		qDebug() << QString("TcpTransfer::onSocketOutStateChanged disconnected socketDescriptor=%1").arg(socketDescriptor);
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
		QTcpSocket * socketOut = tcpServer->nextPendingConnection();
		const qint64 socketDescriptor = socketOut->socketDescriptor();
		socketOut->setProperty("localPort", localPort);
		socketOut->setProperty("socketDescriptor", socketDescriptor);
		m_mapSocketOut[socketDescriptor] = socketOut;
		connect(socketOut, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
			this, SLOT(onSocketOutStateChanged(QAbstractSocket::SocketState)));
		connect(socketOut, SIGNAL(readyRead()), this, SLOT(onSocketOutReadyRead()));
		output_NewConnection(localPort, socketDescriptor);

		qDebug() << QString("TcpTransfer::onTcpNewConnection localPort=%1 socketDescriptor=%2")
			.arg(localPort).arg(socketDescriptor);
	}
}

void TcpTransfer::onSocketOutReadyRead()
{
	QTcpSocket * socketOut = (QTcpSocket*)sender();
	if (!socketOut)
		return;
	const qint64 socketDescriptor = socketOut->socketDescriptor();
	QByteArray bytes = socketOut->readAll();
	output_DataStream(socketDescriptor, Direction_Out, bytes.constData(), bytes.size());

	const quint16 localPort = socketOut->property("localPort").toInt();
}

void TcpTransfer::onSocketInReadyRead()
{
	QTcpSocket * socketIn = (QTcpSocket*)sender();
	if (!socketIn)
		return;
	const qint64 peerSocketDescriptor = socketIn->property("peerSocketDescriptor").toLongLong();

	QByteArray bytes = socketIn->readAll();
	output_DataStream(peerSocketDescriptor, Direction_In, bytes.constData(), bytes.size());
}
