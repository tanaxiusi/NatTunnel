#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include "Peer.h"

class TcpTransfer : public QObject
{
	Q_OBJECT

private:
	enum FrameType
	{
		BeginUnknownFrameType = 0,
		AddTransferType = 1,
		DeleteTransferType = 2,
		NewConnectionType = 3,
		DisconnectConnectionType = 4,
		DataStreamType = 5,
		EndUnknownFrameType = 6,
	};

signals:
	void dataOutput(QByteArray package);

public:
	TcpTransfer(QObject *parent = 0);
	~TcpTransfer();

	void dataInput(QByteArray package);

	bool addTransfer(quint16 localPort, quint16 remoteDestPort, QHostAddress remoteDestAddress);
	bool deleteTransfer(quint16 localPort);

private:
	static bool isValidFrameType(FrameType frameType);

private:
	void dealFrame(FrameType type, const QByteArray & frameData);
	void input_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void input_DeleteTransfer(quint16 localPort);
	void input_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void input_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void input_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);

private:
	void outputFrame(FrameType type, const QByteArray & frameData, const char * extraData = nullptr, int extraDataSize = 0);
	void output_AddTransfer(quint16 localPort, quint16 remoteDestPort, QString remoteDestAddressText);
	void output_DeleteTransfer(quint16 localPort);
	void output_NewConnection(quint16 localPort, qint64 socketDescriptor);
	void output_DisconnectConnection(qint64 socketDescriptor, quint8 direction);
	void output_DataStream(qint64 socketDescriptor, quint8 direction, const char * data, int dataSize);

private slots:
	void onSocketInStateChanged(QAbstractSocket::SocketState state);
	void onSocketOutStateChanged(QAbstractSocket::SocketState state);
	void onTcpNewConnection();
	void onSocketOutReadyRead();
	void onSocketInReadyRead();

private:
	struct SocketInInfo
	{
		qint64 peerSocketDescriptor = 0;		// 对面的socketDescriptor
		QTcpSocket * obj = nullptr;
		QByteArray cachedData;
	};
	QByteArray m_buffer;
	QMap<quint16, QTcpServer*> m_mapTcpServer;		// <localPort,...> 转出隧道监听
	QMap<quint16, Peer> m_mapTransferOut;			// <localPort,...> 转出隧道本地端口-对面地址端口
	QMap<quint16, Peer> m_mapTransferIn;			// <localPort,...> 转入隧道对面端口-本地地址端口
	QMap<qint64, QTcpSocket*> m_mapSocketOut;		// <peerSocketDescriptor,...> 转出隧道的连接
	QMap<qint64, SocketInInfo> m_mapSocketIn;		// <socketDescriptor,...> 转入隧道的连接
};
