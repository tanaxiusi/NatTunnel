﻿#include "QUpnpPortMapper.h"
#include <QtDebug>
#include <QNetworkInterface>
#include <QDomDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QRegExp>
#include <QStringList>

static const unsigned char raw_discover[] = {
	0x4d, 0x2d, 0x53, 0x45, 0x41, 0x52, 0x43, 0x48,	0x20, 0x2a, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f,	0x31, 0x2e, 0x31, 0x0d, 0x0a, 0x48, 0x4f, 0x53,
	0x54, 0x3a, 0x20, 0x32, 0x33, 0x39, 0x2e, 0x32,	0x35, 0x35, 0x2e, 0x32, 0x35, 0x35, 0x2e, 0x32,	0x35, 0x30, 0x3a, 0x31, 0x39, 0x30, 0x30, 0x0d,
	0x0a, 0x53, 0x54, 0x3a, 0x20, 0x75, 0x72, 0x6e,	0x3a, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x73,	0x2d, 0x75, 0x70, 0x6e, 0x70, 0x2d, 0x6f, 0x72,
	0x67, 0x3a, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65,	0x3a, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65,	0x74, 0x47, 0x61, 0x74, 0x65, 0x77, 0x61, 0x79,
	0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x3a, 0x31,	0x0d, 0x0a, 0x4d, 0x41, 0x4e, 0x3a, 0x20, 0x22,	0x73, 0x73, 0x64, 0x70, 0x3a, 0x64, 0x69, 0x73,
	0x63, 0x6f, 0x76, 0x65, 0x72, 0x22, 0x0d, 0x0a,	0x4d, 0x58, 0x3a, 0x20, 0x35, 0x0d, 0x0a, 0x0d,	0x0a
};

static const unsigned char raw_queryExternalAddress[] = {
	0x3c,
	0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x20,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x53,0x74,0x79,0x6c,0x65,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,
	0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x2f,0x22,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x53,0x4f,0x41,0x50,0x2d,
	0x45,0x4e,0x56,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x2f,0x22,0x3e,
	0x3c,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x42,0x6f,0x64,0x79,0x3e,0x3c,0x6d,0x3a,0x47,0x65,0x74,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x49,0x50,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x6d,0x3d,0x22,
	0x75,0x72,0x6e,0x3a,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2d,0x75,0x70,0x6e,0x70,0x2d,0x6f,0x72,0x67,0x3a,0x73,0x65,0x72,0x76,0x69,0x63,0x65,0x3a,0x57,0x41,0x4e,0x49,0x50,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x3a,0x31,0x22,0x2f,
	0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x42,0x6f,0x64,0x79,0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,
	0x6c,0x6f,0x70,0x65,0x3e,
};

static const unsigned char raw_addPortMapping[] = {
	0x3c,
	0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x20,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x53,0x74,0x79,0x6c,0x65,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,
	0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x2f,0x22,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x53,0x4f,0x41,0x50,0x2d,
	0x45,0x4e,0x56,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x2f,0x22,0x3e,
	0x3c,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x42,0x6f,0x64,0x79,0x3e,0x3c,0x6d,0x3a,0x41,0x64,0x64,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x6d,0x3d,0x22,0x75,0x72,0x6e,0x3a,0x73,0x63,
	0x68,0x65,0x6d,0x61,0x73,0x2d,0x75,0x70,0x6e,0x70,0x2d,0x6f,0x72,0x67,0x3a,0x73,0x65,0x72,0x76,0x69,0x63,0x65,0x3a,0x57,0x41,0x4e,0x49,0x50,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x3a,0x31,0x22,0x3e,0x3c,0x4e,0x65,0x77,0x45,0x78,
	0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,0x28,0x25,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,0x3c,0x4e,0x65,
	0x77,0x49,0x6e,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,0x28,0x25,0x49,0x6e,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x49,0x6e,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,
	0x3c,0x4e,0x65,0x77,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x3e,0x28,0x25,0x54,0x79,0x70,0x65,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x3e,0x3c,0x4e,0x65,0x77,0x45,0x6e,0x61,0x62,0x6c,0x65,0x64,0x3e,0x31,
	0x3c,0x2f,0x4e,0x65,0x77,0x45,0x6e,0x61,0x62,0x6c,0x65,0x64,0x3e,0x3c,0x4e,0x65,0x77,0x49,0x6e,0x74,0x65,0x72,0x6e,0x61,0x6c,0x43,0x6c,0x69,0x65,0x6e,0x74,0x3e,0x28,0x25,0x4c,0x6f,0x63,0x61,0x6c,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x25,0x29,
	0x3c,0x2f,0x4e,0x65,0x77,0x49,0x6e,0x74,0x65,0x72,0x6e,0x61,0x6c,0x43,0x6c,0x69,0x65,0x6e,0x74,0x3e,0x3c,0x4e,0x65,0x77,0x4c,0x65,0x61,0x73,0x65,0x44,0x75,0x72,0x61,0x74,0x69,0x6f,0x6e,0x3e,0x30,0x3c,0x2f,0x4e,0x65,0x77,0x4c,0x65,0x61,0x73,
	0x65,0x44,0x75,0x72,0x61,0x74,0x69,0x6f,0x6e,0x3e,0x3c,0x4e,0x65,0x77,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x44,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x69,0x6f,0x6e,0x3e,0x28,0x25,0x44,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x69,
	0x6f,0x6e,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x44,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x69,0x6f,0x6e,0x3e,0x3c,0x4e,0x65,0x77,0x52,0x65,0x6d,0x6f,0x74,0x65,0x48,0x6f,0x73,0x74,0x3e,0x3c,
	0x2f,0x4e,0x65,0x77,0x52,0x65,0x6d,0x6f,0x74,0x65,0x48,0x6f,0x73,0x74,0x3e,0x3c,0x2f,0x6d,0x3a,0x41,0x64,0x64,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x42,0x6f,0x64,
	0x79,0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x3e,
};

static const unsigned char raw_deletePortMapping[] = {
	0x3c,
	0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x20,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x53,0x74,0x79,0x6c,0x65,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,
	0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x63,0x6f,0x64,0x69,0x6e,0x67,0x2f,0x22,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x53,0x4f,0x41,0x50,0x2d,
	0x45,0x4e,0x56,0x3d,0x22,0x68,0x74,0x74,0x70,0x3a,0x2f,0x2f,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2e,0x78,0x6d,0x6c,0x73,0x6f,0x61,0x70,0x2e,0x6f,0x72,0x67,0x2f,0x73,0x6f,0x61,0x70,0x2f,0x65,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x2f,0x22,0x3e,
	0x3c,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x42,0x6f,0x64,0x79,0x3e,0x3c,0x6d,0x3a,0x44,0x65,0x6c,0x65,0x74,0x65,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x20,0x78,0x6d,0x6c,0x6e,0x73,0x3a,0x6d,0x3d,0x22,0x75,0x72,0x6e,
	0x3a,0x73,0x63,0x68,0x65,0x6d,0x61,0x73,0x2d,0x75,0x70,0x6e,0x70,0x2d,0x6f,0x72,0x67,0x3a,0x73,0x65,0x72,0x76,0x69,0x63,0x65,0x3a,0x57,0x41,0x4e,0x49,0x50,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x3a,0x31,0x22,0x3e,0x3c,0x4e,0x65,
	0x77,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,0x28,0x25,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x45,0x78,0x74,0x65,0x72,0x6e,0x61,0x6c,0x50,0x6f,0x72,0x74,0x3e,
	0x3c,0x4e,0x65,0x77,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x3e,0x28,0x25,0x54,0x79,0x70,0x65,0x25,0x29,0x3c,0x2f,0x4e,0x65,0x77,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x3e,0x3c,0x4e,0x65,0x77,0x52,0x65,0x6d,0x6f,0x74,0x65,0x48,0x6f,0x73,
	0x74,0x3e,0x3c,0x2f,0x4e,0x65,0x77,0x52,0x65,0x6d,0x6f,0x74,0x65,0x48,0x6f,0x73,0x74,0x3e,0x3c,0x2f,0x6d,0x3a,0x44,0x65,0x6c,0x65,0x74,0x65,0x50,0x6f,0x72,0x74,0x4d,0x61,0x70,0x70,0x69,0x6e,0x67,0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,
	0x4e,0x56,0x3a,0x42,0x6f,0x64,0x79,0x3e,0x3c,0x2f,0x53,0x4f,0x41,0x50,0x2d,0x45,0x4e,0x56,0x3a,0x45,0x6e,0x76,0x65,0x6c,0x6f,0x70,0x65,0x3e,
};


#define DECLARE_DATA(name)\
	static const QByteArray data_##name = QByteArray::fromRawData((const char*)raw_##name, sizeof(raw_##name));

DECLARE_DATA(discover)
DECLARE_DATA(queryExternalAddress)
DECLARE_DATA(addPortMapping)
DECLARE_DATA(deletePortMapping)

QUpnpPortMapper::QUpnpPortMapper(QObject *parent)
	: QObject(parent)
{
	m_udpSocket = NULL;
	m_timer = NULL;
	m_networkAccessManager = NULL;
	m_discoverOk = false;
	m_lastAddPortMappingOk = false;
	m_lastDeletePortMappingOk = false;
}

QUpnpPortMapper::~QUpnpPortMapper()
{
	close();
}

QHostAddress QUpnpPortMapper::localAddress()
{
	return m_localAddress;
}

bool QUpnpPortMapper::isDiscoverOk()
{
	return m_discoverOk;
}

bool QUpnpPortMapper::open(const QHostAddress & localAddress)
{
	if (!m_localAddress.isNull())
	{
		qWarning() << QString("QUpnpPortMapper has already open");
		return false;
	}
	if (localAddress.isNull())
		return false;
	if (localAddress.protocol() == QAbstractSocket::IPv6Protocol)
	{
		qWarning() << QString("QUpnpPortMapper does not support IPv6");
		return false;
	}

	m_localAddress = localAddress;
	m_controlUrl = QUrl();
	m_networkAccessManager = new QNetworkAccessManager(this);
	m_discoverOk = false;
	m_lastExternalAddress = QHostAddress();
	m_lastAddPortMappingOk = false;
	m_lastDeletePortMappingOk = false;

	connect(m_networkAccessManager, SIGNAL(finished(QNetworkReply*)),
		this, SLOT(onReplyFinished(QNetworkReply*)));
	return true;
}

bool QUpnpPortMapper::open(const QNetworkInterface & networkInterface)
{
	foreach(QNetworkAddressEntry entry, networkInterface.addressEntries())
		if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
			return open(entry.ip());
	return false;
}

void QUpnpPortMapper::close()
{
	if (m_localAddress.isNull())
		return;

	if (m_udpSocket)
	{
		delete m_udpSocket;
		m_udpSocket = NULL;
	}

	if (m_timer)
	{
		delete m_timer;
		m_timer = NULL;
	}

	if (m_networkAccessManager)
	{
		delete m_networkAccessManager;
		m_networkAccessManager = NULL;
	}
	
	m_localAddress = QHostAddress();
	m_controlUrl = QString();
	m_lastExternalAddress = QHostAddress();
	m_lastAddPortMappingOk = false;
	m_lastDeletePortMappingOk = false;
}

bool QUpnpPortMapper::discover(int timeout, bool wait)
{
	if (m_discoverOk)
		return false;
	if (!m_udpSocket)
	{
		m_udpSocket = new QUdpSocket(this);
		m_udpSocket->bind(m_localAddress, 0);
		connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(onUdpSocketReadyRead()));
	}
	if (!m_timer)
	{
		m_timer = new QTimer(this);
		connect(m_timer, SIGNAL(timeout()), this, SLOT(onDiscoverTimeout()));
	}
	m_udpSocket->writeDatagram(data_discover, QHostAddress("239.255.255.250"), 1900);
	m_timer->setSingleShot(true);
	m_timer->start(timeout);

	if (wait)
	{
		QEventLoop eventLoop;
		connect(this, SIGNAL(discoverFinished(bool)), &eventLoop, SLOT(quit()));
		eventLoop.exec();
		return m_discoverOk;
	}

	return true;
}

QHostAddress QUpnpPortMapper::queryExternalAddress(bool wait)
{
	if (!m_discoverOk)
		return QHostAddress();
	QNetworkRequest request(m_controlUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
	request.setRawHeader("SOAPAction", "\"urn:schemas-upnp-org:service:WANIPConnection:1#GetExternalIPAddress\"");
	QNetworkReply * reply = m_networkAccessManager->post(request, data_queryExternalAddress);
	reply->setProperty("reply-type", "queryExternalAddress");

	if (wait)
	{
		QEventLoop eventLoop;
		connect(this, SIGNAL(queryExternalAddressFinished(QHostAddress,bool,QString)), &eventLoop, SLOT(quit()));
		eventLoop.exec();
		return m_lastExternalAddress;
	}
	return QHostAddress();
}

bool QUpnpPortMapper::addPortMapping(QAbstractSocket::SocketType type, QHostAddress internalAddress, quint16 internalPort, quint16 externalPort, QString description, bool wait)
{
	if (!m_discoverOk)
		return false;
	QByteArray typeText;
	if (type == QAbstractSocket::TcpSocket)
		typeText = "TCP";
	else if (type == QAbstractSocket::UdpSocket)
		typeText = "UDP";
	else
		return false;
	QByteArray content = data_addPortMapping;
	content = content.replace("(%ExternalPort%)", QByteArray::number(externalPort));
	content = content.replace("(%InternalPort%)", QByteArray::number(internalPort));
	content = content.replace("(%Type%)", typeText);
	content = content.replace("(%LocalAddress%)", internalAddress.toString().toUtf8());
	content = content.replace("(%Description%)", description.toUtf8());

	QNetworkRequest request(m_controlUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
	request.setRawHeader("SOAPAction", "\"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"");
	QNetworkReply * reply = m_networkAccessManager->post(request, content);
	reply->setProperty("reply-type", "addPortMapping");

	if (wait)
	{
		QEventLoop eventLoop;
		connect(this, SIGNAL(addPortMappingFinished(bool, QString)), &eventLoop, SLOT(quit()));
		eventLoop.exec();
		return m_lastAddPortMappingOk;
	}
	return true;
}

bool QUpnpPortMapper::deletePortMapping(QAbstractSocket::SocketType type, quint16 externalPort, bool wait)
{
	if (!m_discoverOk)
		return false;
	QByteArray typeText;
	if (type == QAbstractSocket::TcpSocket)
		typeText = "TCP";
	else if (type == QAbstractSocket::UdpSocket)
		typeText = "UDP";
	else
		return false;
	QByteArray content = data_deletePortMapping;
	content = content.replace("(%ExternalPort%)", QByteArray::number(externalPort));
	content = content.replace("(%Type%)", typeText);

	QNetworkRequest request(m_controlUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
	request.setRawHeader("SOAPAction", "\"urn:schemas-upnp-org:service:WANIPConnection:1#DeletePortMapping\"");
	QNetworkReply * reply = m_networkAccessManager->post(request, content);
	reply->setProperty("reply-type", "deletePortMapping");

	if (wait)
	{
		QEventLoop eventLoop;
		connect(this, SIGNAL(deletePortMappingFinished(bool, QString)), &eventLoop, SLOT(quit()));
		eventLoop.exec();
		return m_lastDeletePortMappingOk;
	}
	return true;
}

void QUpnpPortMapper::onDiscoverTimeout()
{
	if (m_timer)
	{
		m_timer->stop();
		m_timer->deleteLater();
		m_timer = NULL;
	}

	if (m_udpSocket)
	{
		delete m_udpSocket;
		m_udpSocket = NULL;
	}
	
	emit discoverFinished(false);
}

void QUpnpPortMapper::onUdpSocketReadyRead()
{
	if (!m_udpSocket)
		return;

	while (m_udpSocket->hasPendingDatagrams())
	{
		char buffer[3000];
		QHostAddress hostAddress;
		quint16 port = 0;
		const int bufferSize = m_udpSocket->readDatagram(buffer, sizeof(buffer), &hostAddress, &port);

		QString str = hostAddress.toString();

		const QString package = QString::fromUtf8(QByteArray::fromRawData(buffer, bufferSize));
		const QString tab = "Location:";
		const int tabPos = package.indexOf(tab, 0, Qt::CaseInsensitive);
		if(tabPos < 0)
			continue;

		int lineEndPos = package.indexOf('\n', tabPos);
		if (lineEndPos < 0)
			lineEndPos = package.size();

		QString location = package.mid(tabPos + tab.length(), lineEndPos - tabPos - tab.length());
		location = location.trimmed();

		QNetworkReply * reply = m_networkAccessManager->get(QNetworkRequest(QUrl(location)));
		reply->setProperty("reply-type", "discover");

		m_udpSocket->deleteLater();
		m_udpSocket = NULL;

		break;
	}
}

void QUpnpPortMapper::onReplyFinished(QNetworkReply * reply)
{
	reply->deleteLater();
	const QByteArray type = reply->property("reply-type").toByteArray();
	
	if (type == "discover")
		onReplyDiscover(reply);
	else if (type == "queryExternalAddress")
		onReplyQueryExternalAddress(reply);
	else if (type == "addPortMapping")
		onReplyAddPortMapping(reply);
	else if (type == "deletePortMapping")
		onReplyDeletePortMapping(reply);
}

void QUpnpPortMapper::onReplyDiscover(QNetworkReply * reply)
{
	QDomDocument xml;
	QString errorMsg;
	int errorLine = 0;
	int errorColumn = 0;

	if (m_timer)
	{
		m_timer->stop();
		m_timer->deleteLater();
		m_timer = NULL;
	}

	if (xml.setContent(reply, &errorMsg, &errorLine, &errorColumn))
	{
		QString relativeControlUrl = recursiveFindControlUrl(xml.documentElement());
		if (relativeControlUrl.length() > 0)
			m_controlUrl = reply->url().resolved(QUrl(relativeControlUrl));
	}
	if (m_controlUrl.isValid())
		m_discoverOk = true;
	emit discoverFinished(m_controlUrl.isValid());
}

void QUpnpPortMapper::onReplyQueryExternalAddress(QNetworkReply * reply)
{
	QString content = QString::fromUtf8(reply->readAll());
	QString addressText = getXmlTabContent(content, "NewExternalIPAddress", Qt::CaseInsensitive);
	if (!addressText.isNull())
	{
		m_lastExternalAddress = QHostAddress(addressText);
		emit queryExternalAddressFinished(m_lastExternalAddress, true, QString());
	}
	else
	{
		QString errorString = getXmlTabContent(content, "errorDescription", Qt::CaseInsensitive);
		m_lastExternalAddress = QHostAddress();
		emit queryExternalAddressFinished(QHostAddress(), false, errorString);
	}
}

void QUpnpPortMapper::onReplyAddPortMapping(QNetworkReply * reply)
{
	const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString content = QString::fromUtf8(reply->readAll());
	if (statusCode == 200)
	{
		m_lastAddPortMappingOk = true;
		emit addPortMappingFinished(true, QString());
	}
	else
	{
		m_lastAddPortMappingOk = false;
		emit addPortMappingFinished(false, getXmlTabContent(content, "errorDescription", Qt::CaseInsensitive));
	}
}

void QUpnpPortMapper::onReplyDeletePortMapping(QNetworkReply * reply)
{
	const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	QString content = QString::fromUtf8(reply->readAll());
	if (statusCode == 200)
	{
		m_lastDeletePortMappingOk = true;
		emit deletePortMappingFinished(true, QString());
	}
	else
	{
		m_lastDeletePortMappingOk = false;
		emit deletePortMappingFinished(false, getXmlTabContent(content, "errorDescription", Qt::CaseInsensitive));
	}
}

QString QUpnpPortMapper::recursiveFindControlUrl(const QDomElement & parent)
{
	for (QDomElement device = parent.firstChildElement("device");
		!device.isNull(); device = device.nextSiblingElement("device"))
	{
		QDomElement serviceList = device.firstChildElement("serviceList");
		for (QDomElement service = serviceList.firstChildElement("service");
			!service.isNull(); service = service.nextSiblingElement("service"))

		{
			const QString serviceType = service.firstChildElement("serviceType").text();
			if (serviceType == "urn:schemas-upnp-org:service:WANIPConnection:1")
				return service.firstChildElement("controlURL").text();
		}

		QString result = recursiveFindControlUrl(device.firstChildElement("deviceList"));
		if(result.length() > 0)
			return result;
	}
	return QString();
}

QString QUpnpPortMapper::getXmlTabContent(QString content, QString tab, Qt::CaseSensitivity cs)
{
	QRegExp rx(QString("<%1>([^\\r\\n]*)</%1>").arg(tab));
	rx.setCaseSensitivity(cs);
	if (content.indexOf(rx) >= 0)
		return rx.capturedTexts().at(1);
	else
		return QString();
}

