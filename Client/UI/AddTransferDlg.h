#pragma once

#include <QDialog>
#include <QHostAddress>
#include "ui_AddTransferDlg.h"

class AddTransferDlg : public QDialog
{
	Q_OBJECT

public:
	AddTransferDlg(QWidget *parent = 0);
	~AddTransferDlg();

	quint16 localPort();
	quint16 remotePort();
	QHostAddress remoteAddress();

private slots:
	void onBtnOk();
	void onCheckBoxPeerLocal();

private:
	Ui::AddTransferDlg ui;
};
