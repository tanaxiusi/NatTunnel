#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include "ui_TransferManageDlg.h"
#include "Util/Peer.h"

class TransferManageDlg : public QDialog
{
	Q_OBJECT

signals:
	int wannaGetTransferInList(int tunnelId);
	int wannaGetTransferOutList(int tunnelId);
	int wannaAddTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	int wannaDeleteTransfer(int tunnelId, quint16 localPort);

public:
	TransferManageDlg(QWidget *parent, int tunnelId, QString peerUserName);
	~TransferManageDlg();

	void init();

private slots:
	void onBtnRefreshIn();
	void onBtnAddOut();
	void onBtnSavePreset();
	void onBtnDeleteTransfer();

	void onGetTransferInList(int bridgeMessageId, QMap<quint16, Peer> transferList);
	void onGetTransferOutList(int bridgeMessageId, QMap<quint16, Peer> transferList);
	
private:
	void refreshIn();
	void refreshOut();


private:
	Ui::TransferManageDlg ui;

	QString m_peerUserName;

	QStandardItemModel * m_modelIn;
	QStandardItemModel * m_modelOut;

	QMap<quint16, Peer> m_lastTransferInList;
	QMap<quint16, Peer> m_lastTransferOutList;

	int m_tunnelId;

	QSet<int> m_lstMyMessageId;
};
