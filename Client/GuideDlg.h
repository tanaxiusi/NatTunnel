#pragma once

#include <QDialog>
#include <QHostAddress>
#include "ui_GuideDlg.h"

class GuideDlg : public QDialog
{
	Q_OBJECT

public:
	GuideDlg(QWidget *parent = 0);
	~GuideDlg();

	void setServerAddress(QString serverAddress);
	void setServerPort(quint16 serverPort);
	void setServerKey(QString serverKey);

	QString serverAddress();
	quint16 serverPort();
	QString serverKey();

private:
	Ui::GuideDlg ui;
};
