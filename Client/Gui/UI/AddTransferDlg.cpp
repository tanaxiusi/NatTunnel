#include "AddTransferDlg.h"
#include <QMessageBox>
#include "Util/Other.h"

AddTransferDlg::AddTransferDlg(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(onBtnOk()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	connect(ui.checkBoxPeerLocal, SIGNAL(stateChanged(int)), this, SLOT(onCheckBoxPeerLocal()));

	ui.checkBoxPeerLocal->setChecked(true);
}

AddTransferDlg::~AddTransferDlg()
{

}

quint16 AddTransferDlg::localPort()
{
	return (quint16)ui.spinBoxLocalPort->value();
}

quint16 AddTransferDlg::remotePort()
{
	return (quint16)ui.spinBoxRemotePort->value();
}

QHostAddress AddTransferDlg::remoteAddress()
{
	return QHostAddress(ui.editRemoteAddress->text());
}

void AddTransferDlg::onBtnOk()
{
	if (localPort() != 0 && remotePort() != 0 && !remoteAddress().isNull())
		accept();
	else
		QMessageBox::warning(this, U16("错误"), U16("输入的数据无效"));
}

void AddTransferDlg::onCheckBoxPeerLocal()
{
	if (ui.checkBoxPeerLocal->isChecked())
	{
		ui.editRemoteAddress->setText("127.0.0.1");
		ui.editRemoteAddress->setEnabled(false);
	}
	else
	{
		ui.editRemoteAddress->setEnabled(true);
	}
}
