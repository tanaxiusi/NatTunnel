#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QStandardItemModel>
#include <QThread>
#include "ui_MainDlg.h"
#include "Function/Client.h"
#include "Function/TransferManager.h"

class MainDlg : public QMainWindow
{
	Q_OBJECT

public:
	MainDlg(QWidget *parent = 0);
	~MainDlg();

	void start();
	void stop();

	void leadWindowsFirewallPopup();

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void onConnected();
	void onDisconnected();
	void onDiscarded(QString reason);
	void onBinaryError(QByteArray correctBinary);
	void onLogined();
	void onLoginFailed(QString userName, QString msg);
	void onNatTypeConfirmed(NatType natType);
	void onClientUpnpStatusChanged(UpnpStatus upnpStatus);
	void onClientWarning(QString msg);

	void onEditLocalPasswordChanged();

	void onBtnRefreshOnlineUser();
	void onReplyRefreshOnlineUser(QStringList onlineUserList);

	void onBtnTunnel();
	void onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason);
	void onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName);

	void onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress);
	void onTunnelHandShaked(int tunnelId);
	void onTunnelClosed(int tunnelId, QString peerUserName, QString reason);

	void onBtnCloseTunneling();
	void onBtnManageTransfer();

private:
	void updateTableRow(int tunnelId, QString peerUserName, QString peerAddress, QString status);
	void deleteTableRow(int tunnelId);

	void insertTopUserName(QString userName);

private:
	Ui::MainDlgClass ui;
	QLabel * m_labelStatus;
	QLabel * m_labelNatType;
	QLabel * m_labelUpnp;
	QStandardItemModel * m_tableModel;

	QThread m_workingThread;
	Client * m_client;
	TransferManager * m_transferManager;
};
