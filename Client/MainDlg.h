#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QStandardItemModel>
#include <QThread>
#include "ui_MainDlg.h"
#include "Client.h"
#include "QUpnpPortMapper.h"
#include "TcpTransfer.h"

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
	void onClientUpnpStatusChanged(Client::UpnpStatus upnpStatus);
	void onClientWarning(QString msg);

	void onEditLocalPasswordChanged();

	void onBtnTunnel();
	void onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName);

	void onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void onTunnelHandShaked(int tunnelId);
	void onTunnelData(int tunnelId, QByteArray package);
	void onTunnelClosed(int tunnelId);

	void onBtnCloseTunneling();
	void onBtnAddTransfer();

	void onTcpTransferOutput(QByteArray package);

private:
	void updateTableRow(int tunnelId, QString peerUsername, QString peerAddress, QString status);
	void deleteTableRow(int tunnelId);



private:
	Ui::MainDlgClass ui;
	QLabel * m_labelStatus = nullptr;
	QLabel * m_labelNatType = nullptr;
	QLabel * m_labelUpnp = nullptr;
	QStandardItemModel * m_tableModel = nullptr;

	QThread m_workingThread;

	Client m_client;
	QMap<int, TcpTransfer*> m_mapTcpTransfer;
};
