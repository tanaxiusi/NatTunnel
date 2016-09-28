#pragma once

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QDomElement>
#include <QUrl>

class QUpnpPortMapper : public QObject
{
	Q_OBJECT

signals:
	void discoverFinished(bool ok);
	void queryExternalAddressFinished(QHostAddress address, bool ok, QString errorString);
	void addPortMappingFinished(bool ok, QString errorString);
	void deletePortMappingFinished(bool ok, QString errorString);

public:
	QUpnpPortMapper(QObject *parent = 0);
	~QUpnpPortMapper();

	QHostAddress localAddress();
	bool isDiscoverOk();

public slots:
	bool open(const QHostAddress & localAddress);
	bool open(const QNetworkInterface & networkInterface);
	void close();
	bool discover(int timeout = 5000, bool wait = false);
	QHostAddress queryExternalAddress(bool wait = false);
	bool addPortMapping(QAbstractSocket::SocketType type, QHostAddress internalAddress, quint16 internalPort, quint16 externalPort, QString description = QString(), bool wait = false);
	bool deletePortMapping(QAbstractSocket::SocketType type, quint16 externalPort, bool wait = false);

private slots:
	void onDiscoverTimeout();
	void onUdpSocketReadyRead();
	void onReplyFinished(QNetworkReply * reply);
	void onReplyDiscover(QNetworkReply * reply);
	void onReplyQueryExternalAddress(QNetworkReply * reply);
	void onReplyAddPortMapping(QNetworkReply * reply);
	void onReplyDeletePortMapping(QNetworkReply * reply);

private:
	QString recursiveFindControlUrl(const QDomElement & parent);
	QString getXmlTabContent(QString content, QString tab, Qt::CaseSensitivity cs);

private:
	QHostAddress m_localAddress;
	QUdpSocket * m_udpSocket;
	QTimer * m_timer;
	QNetworkAccessManager * m_networkAccessManager;
	QUrl m_controlUrl;
	bool m_discoverOk;
	QHostAddress m_lastExternalAddress;
	bool m_lastAddPortMappingOk;
	bool m_lastDeletePortMappingOk;
};
