#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include "ui_TransferManageDlg.h"
#include "Function/Peer.h"

class TransferManageDlg : public QDialog
{
	Q_OBJECT

signals:
	QMap<quint16, Peer> wannaGetTransferOutList(int tunnelId);
	QMap<quint16, Peer> wannaGetTransferInList(int tunnelId);
	bool wannaAddTransfer(int tunnelId, quint16 localPort, quint16 remotePort, QHostAddress remoteAddress);
	bool wannaDeleteTransfer(int tunnelId, quint16 localPort);

public:
	TransferManageDlg(QWidget *parent, int tunnelId);
	~TransferManageDlg();

	void init();

private slots:
	void onBtnRefreshIn();
	void onBtnAddOut();
	void onBtnDeleteTransfer();
	
private:
	void refreshIn();
	void refreshOut();

private:
	Ui::TransferManageDlg ui;
	QStandardItemModel * m_modelIn;
	QStandardItemModel * m_modelOut;

	int m_tunnelId;
};
