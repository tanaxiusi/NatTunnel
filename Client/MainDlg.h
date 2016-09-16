#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QStandardItemModel>
#include "ui_MainDlg.h"
#include "Client.h"
#include "QUpnpPortMapper.h"

class MainDlg : public QMainWindow
{
	Q_OBJECT

public:
	MainDlg(QWidget *parent = 0);
	~MainDlg();

	void leadWindowsFirewallPopup();

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void connected();
	void disconnected();
	void logined();
	void loginFailed(QString msg);
	void natTypeConfirmed(NatType natType);
	void firewallWarning();

	void onUpnpDiscoverFinished(bool ok);
	void onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString);

	void onEditLocalPasswordChanged();

	void onBtnTunnel();
	void onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName);

	void onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void onTunnelHandShaked(int tunnelId);
	void onTunnelData(int tunnelId, QByteArray package);
	void onTunnelClosed(int tunnelId);

	quint16 addUpnpPortMapping(quint16 internalPort);
	void deleteUpnpPortMapping(quint16 externalPort);

private:
	void updateTableRow(int tunnelId, QString peerUsername, QString peerAddress, QString status);
	void deleteTableRow(int tunnelId);



private:
	Ui::MainDlgClass ui;
	Client m_client;
	QUpnpPortMapper m_upnpPortMapper;
	QLabel * m_labelStatus = nullptr;
	QLabel * m_labelNatType = nullptr;
	QLabel * m_labelUpnp = nullptr;
	QStandardItemModel * m_model = nullptr;
};
