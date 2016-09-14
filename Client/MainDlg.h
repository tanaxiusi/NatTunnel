#pragma once

#include <QWidget>
#include "ui_MainDlg.h"
#include "Client.h"
#include "QUpnpPortMapper.h"

class MainDlg : public QWidget
{
	Q_OBJECT

public:
	MainDlg(QWidget *parent = 0);
	~MainDlg();

	void leadWindowsFirewallPopup();

private slots:
	void connected();
	void disconnected();
	void logined();
	void loginFailed(QString msg);
	void natTypeConfirmed(NatType natType);
	void firewallWarning();

	void onUpnpDiscoverFinished(bool ok);
	void onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString);

private:
	Ui::MainDlgClass ui;
	Client m_client;
	QUpnpPortMapper m_upnpPortMapper;
};
