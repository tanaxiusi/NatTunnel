#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QTime>
#include "Peer.h"

class TcpTransfer : public QObject
{
	Q_OBJECT

private:
	enum FrameType
	{
		BeginUnknownFrameType = 0,
		HeartBeatType,
		AddTransferType,
		DeleteTransferType,
		NewConnectionType,
		DisconnectConnectionType,
		DataStreamType,
		AckType,
		EndUnknownFrameType,
	};

	struct SocketInInfo
	{
		qint64 peerSocketDescriptor = 0;		// 对面的socketDescriptor
		QTcpSocket * obj = nullptr;
		QByteArray cachedData;
		int peerWaitingSize = 0;				// 等待对面写入的字节数
	};
	struct SocketOutInfo
	{
		qint64 socketDescriptor = 0;
		QTcpSocket * obj = nullptr;
		int peerWaitingSize = 0;
	};

	struct SocketIdentifier
	{
		union
		{
			qint64 socketDescriptor;
			qint64 peerSocketDescriptor;
		};
		quint8 direction;
		SocketIdentifier(qint64 socketDescriptor, quint8 direction)
		{
			this->socketDescriptor = socketDescriptor;
			this->direction = direction;
		}
		bool operator == (const SocketIdentifier & other)
		{
			return (this->socketDescriptor == other.socketDescriptor) && (this->direction == other.direction);
		}
	};

signals:
	void dataOutput(QByteArray package);
	bool isTunnelBusy();

public:
	TcpTransfer(QObject *parent = 0);
	~TcpTransfer();

	void dataInput(QByteArray package);

	bool addTransfer(quint16 localPort, quint16 remoteDestPort, QHostAddress remoteDestAddress);
	bool deleteTransfer(quint16 localPort);

private:
	static bool isValidFrameType(FrameType frameType);

private:
	SocketInInfo * findSocketIn(const qint64 & peerSocketDescriptor);
	SocketOutInfo * findSocketOut(const qint64 & socketDescriptor);

private:
	void dealFrame(FrameType type, const QByteArray & frameData);
	void input_heartBeat();
	void input_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void input_DeleteTransfer(quint16 localPort);
	void input_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void input_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void input_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);
	void input_Ack(qint64 socketDescriptor, quint8 direction, int writtenSize);

private:
	void outputFrame(FrameType type, const QByteArray & frameData, const char * extraData = nullptr, int extraDataSize = 0);
	void output_heartBeat();
	void output_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void output_DeleteTransfer(quint16 localPort);
	void output_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void output_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void output_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);
	void output_Ack(qint64 socketDescriptor, quint8 direction, int writtenSize);

private:
	int readAndSendSocketOut(SocketOutInfo & socketOut);
	int readAndSendSocketIn(SocketInInfo & socketIn);

private slots:
	void timerFunction15s();
	void onSocketInStateChanged(QAbstractSocket::SocketState state);
	void onSocketOutStateChanged(QAbstractSocket::SocketState state);
	void onTcpNewConnection();
	void onSocketOutReadyRead();
	void onSocketInReadyRead();
	void onSocketOutBytesWritten(qint64 size);
	void onSocketInBytesWritten(qint64 size);

private:
	QByteArray m_buffer;
	QMap<quint16, QTcpServer*> m_mapTcpServer;				// <localPort,...> 转出隧道监听
	QMap<quint16, Peer> m_mapTransferOut;					// <localPort,...> 转出隧道本地端口-对面地址端口
	QMap<quint16, Peer> m_mapTransferIn;					// <localPort,...> 转入隧道对面端口-本地地址端口
	QMap<qint64, SocketOutInfo> m_mapSocketOut;				// <peerSocketDescriptor,...> 转出隧道的连接
	QMap<qint64, SocketInInfo> m_mapSocketIn;				// <socketDescriptor,...> 转入隧道的连接
	QList<SocketIdentifier> m_lstGlobalWaitingSocket;		// 由于全局流控被迫等待的Socket
	int m_globalWaitingSize = 0;							// 全局流控等待字节数，DataStreamType实际字节数，不包含Frame
	QTime m_lastOutTime;
	QTimer m_timer15s;
};
