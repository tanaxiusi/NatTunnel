#include "TransferManageDlg.h"
#include "Util/Other.h"
#include "AddTransferDlg.h"

TransferManageDlg::TransferManageDlg(QWidget *parent, int tunnelId)
	: QDialog(parent)
{
	ui.setupUi(this);

	m_tunnelId = tunnelId;

	connect(ui.btnRefreshIn, SIGNAL(clicked()), this, SLOT(onBtnRefreshIn()));
	connect(ui.btnAddOut, SIGNAL(clicked()), this, SLOT(onBtnAddOut()));

	m_modelIn = new QStandardItemModel(ui.tableViewIn);
	m_modelOut = new QStandardItemModel(ui.tableViewOut);

	ui.tableViewIn->setModel(m_modelIn);
	ui.tableViewOut->setModel(m_modelOut);

	m_modelIn->setHorizontalHeaderLabels(QStringList() << U16("端口") << U16("转发到"));
	m_modelOut->setHorizontalHeaderLabels(QStringList() << U16("端口") << U16("转发到") << U16("操作"));
}

TransferManageDlg::~TransferManageDlg()
{

}

void TransferManageDlg::init()
{
	refreshIn();
	refreshOut();
}

void TransferManageDlg::onBtnRefreshIn()
{
	refreshIn();
}

void TransferManageDlg::onBtnAddOut()
{
	AddTransferDlg dlg(this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	wannaAddTransfer(m_tunnelId, dlg.localPort(), dlg.remotePort(), dlg.remoteAddress());
	refreshOut();
}

void TransferManageDlg::refreshIn()
{
	const QMap<quint16, Peer> transferList = wannaGetTransferInList(m_tunnelId);
	m_modelIn->removeRows(0, m_modelIn->rowCount());
	foreach (quint16 localPort, transferList.keys())
	{
		const Peer peer = transferList.value(localPort);
		QList<QStandardItem*> lstItem;
		lstItem << new QStandardItem(QString::number(localPort));
		lstItem << new QStandardItem(QString("%1:%2").arg(peer.address.toString()).arg(peer.port));
		m_modelIn->appendRow(lstItem);
	}
}

void TransferManageDlg::refreshOut()
{
	const QMap<quint16, Peer> transferList = wannaGetTransferOutList(m_tunnelId);
	m_modelOut->removeRows(0, m_modelOut->rowCount());
	foreach (quint16 localPort, transferList.keys())
	{
		const Peer peer = transferList.value(localPort);
		QList<QStandardItem*> lstItem;
		lstItem << new QStandardItem(QString::number(localPort));
		lstItem << new QStandardItem(QString("%1:%2").arg(peer.address.toString()).arg(peer.port));
		lstItem << new QStandardItem();
		m_modelOut->appendRow(lstItem);

		QPushButton * btnDeleteTransfer = new QPushButton(U16("删除"));
		btnDeleteTransfer->setProperty("localPort", localPort);
		connect(btnDeleteTransfer, SIGNAL(clicked()), this, SLOT(onBtnDeleteTransfer()));

		ui.tableViewOut->setIndexWidget(m_modelOut->index(m_modelOut->rowCount() - 1, m_modelOut->columnCount() - 1), btnDeleteTransfer);
	}
}

void TransferManageDlg::onBtnDeleteTransfer()
{
	QPushButton * btnDeleteTransfer = qobject_cast<QPushButton*>(sender());
	if (!btnDeleteTransfer)
		return;
	const quint16 localPort = (quint16)btnDeleteTransfer->property("localPort").toInt();
	emit wannaDeleteTransfer(m_tunnelId, localPort);
	refreshOut();
}
