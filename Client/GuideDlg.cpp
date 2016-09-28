#include "GuideDlg.h"
#include <QMessageBox>
#include "../Server/Other.h"

GuideDlg::GuideDlg(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}

GuideDlg::~GuideDlg()
{

}

void GuideDlg::setServerAddress(QString serverAddress)
{
	ui.editServerAddress->setText(serverAddress);
}

void GuideDlg::setServerPort(quint16 serverPort)
{
	ui.spinBoxServerPort->setValue(serverPort);
}

void GuideDlg::setServerKey(QString serverKey)
{
	ui.editServerKey->setText(serverKey);
}

QString GuideDlg::serverAddress()
{
	return ui.editServerAddress->text();
}

quint16 GuideDlg::serverPort()
{
	return ui.spinBoxServerPort->value();
}

QString GuideDlg::serverKey()
{
	return ui.editServerKey->text();
}
