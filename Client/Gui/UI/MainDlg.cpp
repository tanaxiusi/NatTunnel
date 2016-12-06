#include "MainDlg.h"
#include <QTcpServer>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QProcess>
#include "Util/Other.h"
#include "GuideDlg.h"
#include "TransferManageDlg.h"

MainDlg::MainDlg(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_labelStatus = NULL;
	m_labelNatType = NULL;
	m_labelUpnp = NULL;
	m_tableModel = NULL;
	m_bridge = NULL;

	m_labelStatus = new QLabel(U16("未连接"));
	m_labelNatType = new QLabel(U16(" "));
	m_labelUpnp = new QLabel(U16(" "));

	m_labelStatus->setMinimumWidth(60);
	m_labelNatType->setMinimumWidth(60);
	m_labelUpnp->setMinimumWidth(60);

	ui.statusBar->addPermanentWidget(m_labelUpnp);
	ui.statusBar->addPermanentWidget(m_labelNatType);
	ui.statusBar->addPermanentWidget(m_labelStatus);

	m_tableModel = new QStandardItemModel(ui.tableView);
	ui.tableView->setModel(m_tableModel);
	m_tableModel->setHorizontalHeaderLabels(U16("tunnelId,对方用户名,对方IP地址,状态,操作").split(",", QString::SkipEmptyParts));
	ui.tableView->setColumnHidden(0, true);
	ui.tableView->setColumnHidden(2, true);
	ui.tableView->setColumnWidth(1, 100);
	ui.tableView->setColumnWidth(2, 100);
	ui.tableView->setColumnWidth(3, 100);
	ui.tableView->setColumnWidth(4, 160);

	ui.btnQueryOnlineUser->setIcon(QIcon(":/MainDlg/refresh.png"));

	connect(ui.editLocalPassword, SIGNAL(textChanged(const QString &)), this, SLOT(onEditLocalPasswordChanged()));
	connect(ui.btnQueryOnlineUser, SIGNAL(clicked()), this, SLOT(onBtnQueryOnlineUser()));
	connect(ui.btnTunnel, SIGNAL(clicked()), this, SLOT(onBtnTunnel()));

	start();
}

MainDlg::~MainDlg()
{
	stop();
}

void MainDlg::start()
{
	if (m_bridge)
		return;

	m_bridge = new BridgeForGui(this);

	connect(m_bridge, SIGNAL(lostConnection()), this, SLOT(close()), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_connected()), this, SLOT(onConnected()), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_disconnected()), this, SLOT(onDisconnected()), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_discarded(QString)), this, SLOT(onDiscarded(QString)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_binaryError(QByteArray)), this, SLOT(onBinaryError(QByteArray)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_logined()), this, SLOT(onLogined()), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_loginFailed(QString, QString)), this, SLOT(onLoginFailed(QString, QString)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_natTypeConfirmed(NatType)), this, SLOT(onNatTypeConfirmed(NatType)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_upnpStatusChanged(UpnpStatus)), this, SLOT(onClientUpnpStatusChanged(UpnpStatus)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_warning(QString)), this, SLOT(onClientWarning(QString)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_replyQueryOnlineUser(QStringList)), this, SLOT(onReplyQueryOnlineUser(QStringList)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_replyTryTunneling(QString, bool, bool, QString)), this, SLOT(onReplyTryTunneling(QString, bool, bool, QString)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_replyReadyTunneling(int, int, QString)), this, SLOT(onReplyReadyTunneling(int, int, QString)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_tunnelStarted(int, QString, QHostAddress)), this, SLOT(onTunnelStarted(int, QString, QHostAddress)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)), Qt::QueuedConnection);
	connect(m_bridge, SIGNAL(event_tunnelClosed(int,QString,QString)), this, SLOT(onTunnelClosed(int,QString,QString)), Qt::QueuedConnection);

	m_bridge->start();

	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);

	const QHostAddress serverAddress = QHostAddress(setting.value("Server/Address").toString());
	const int serverPort = setting.value("Server/Port").toInt();
	const QByteArray serverKey = setting.value("Server/Key").toByteArray();

	const QString userName = setting.value("Client/UserName").toString();
	const QString localPassword = setting.value("Client/LocalPassword", QString::number(rand_u32())).toString();
	QString randomIdentifierSuffix = setting.value("Client/RandomIdentifierSuffix").toString();
	if (randomIdentifierSuffix.isEmpty())
	{
		randomIdentifierSuffix = QString::number(rand_u32());
		setting.setValue("Client/RandomIdentifierSuffix", randomIdentifierSuffix);
	}


	if(setting.value("Other/ShowTunnelId").toInt() == 1)
		ui.tableView->setColumnHidden(0, false);
	if (setting.value("Other/ShowAddress").toInt() == 1)
		ui.tableView->setColumnHidden(2, false);

	ui.editUserName->setText(userName);
	ui.editLocalPassword->setText(localPassword);

	m_bridge->slot_setConfig(serverKey, randomIdentifierSuffix, serverAddress, serverPort);
	m_bridge->slot_setUserName(userName);
	m_bridge->slot_start();
}

void MainDlg::stop()
{
	if (!m_bridge)
		return;

	delete m_bridge;
	m_bridge = NULL;
}

void MainDlg::closeEvent(QCloseEvent *event)
{
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	setting.setValue("Client/LocalPassword", ui.editLocalPassword->text());
}
void MainDlg::onConnected()
{
	m_labelStatus->setText(U16("正在登录"));
}

void MainDlg::onDisconnected()
{	
	m_labelStatus->setText(U16("断开"));
	m_labelNatType->clear();
	m_labelUpnp->clear();
	ui.btnTunnel->setEnabled(false);
	m_tableModel->removeRows(0, m_tableModel->rowCount());
}

void MainDlg::onDiscarded(QString reason)
{
	stop();
	QMessageBox::warning(this, U16("被踢下线"), reason);
	this->close();
}

void MainDlg::onBinaryError(QByteArray correctBinary)
{
	if (QMessageBox::Ok == QMessageBox::warning(this, U16("错误"), U16("二进制文件错误，点击确认更新"), QMessageBox::Ok | QMessageBox::Cancel))
	{
		const QString binaryFileName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
		const QString updateFileName = binaryFileName + ".update";
		const QString scriptFileName = "update.bat";

		const QString currentDir = QCoreApplication::applicationDirPath();

#if defined(Q_OS_WIN)
		QByteArray scriptContent = readFile(":/MainDlg/WindowsUpdate.txt");
#endif
		scriptContent.replace("(GuiPid)", QByteArray::number(QCoreApplication::applicationPid()));
		scriptContent.replace("(ServicePid)", QByteArray::number(m_bridge->servicePid()));
		scriptContent.replace("(BinaryFileName)", binaryFileName.toLocal8Bit());
		scriptContent.replace("(UpdateFileName)", updateFileName.toLocal8Bit());
		scriptContent.replace("(ScriptFileName)", scriptFileName.toLocal8Bit());

		const bool ok1 = writeFile(currentDir + "/" + updateFileName, correctBinary);
		const bool ok2 = writeFile(currentDir + "/" + scriptFileName, scriptContent);
		if (ok1 && ok2)
			QProcess::startDetached(scriptFileName, QStringList(), currentDir);
		else
			QMessageBox::warning(this, U16("错误"), U16("写入更新文件失败"));
	}

	this->close();
}

void MainDlg::onLogined()
{
	onBtnQueryOnlineUser();
	m_labelStatus->setText(U16("登录成功"));
	m_labelNatType->setText(U16("正在检测NAT类型"));
}

void MainDlg::onLoginFailed(QString userName, QString msg)
{
	QString tipText;
	if (userName.isEmpty())
		tipText = U16("填写一个用户名");
	else
		tipText = U16("%1登录失败：%2，填写一个新用户名").arg(userName).arg(msg);
	const QString newUserName = QInputDialog::getText(this, U16("填写用户名"), tipText);
	if (newUserName.isNull())
	{
		this->close();
		return;
	}

	ui.editUserName->setText(newUserName);
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	setting.setValue("Client/UserName", newUserName);

	m_bridge->slot_setUserName(newUserName);
	m_bridge->slot_tryLogin();
}

void MainDlg::onNatTypeConfirmed(NatType natType)
{
	m_labelNatType->setText(getNatDescription(natType));
	ui.btnTunnel->setEnabled(true);
}

void MainDlg::onClientUpnpStatusChanged(UpnpStatus upnpStatus)
{
	m_labelUpnp->setText(getUpnpStatusDescription(upnpStatus));
}

void MainDlg::onClientWarning(QString msg)
{
	ui.statusBar->showMessage(msg);
}

void MainDlg::onEditLocalPasswordChanged()
{
	if(m_bridge)
		m_bridge->slot_setLocalPassword(ui.editLocalPassword->text());
}

void MainDlg::onBtnQueryOnlineUser()
{
	if (m_bridge)
		m_bridge->slot_queryOnlineUser();
}

void MainDlg::onReplyQueryOnlineUser(QStringList onlineUserList)
{
	const QString currentText = ui.comboBoxPeerUserName->currentText();
	ui.comboBoxPeerUserName->clear();
	ui.comboBoxPeerUserName->addItems(onlineUserList);

	ui.comboBoxPeerUserName->setEditText(currentText);
	ui.btnQueryOnlineUser->setEnabled(true);
}

void MainDlg::onBtnTunnel()
{
	const QString peerUserName = ui.comboBoxPeerUserName->currentText();
	if (peerUserName.isEmpty())
		return;

	m_bridge->slot_tryTunneling(peerUserName);
	ui.btnTunnel->setEnabled(false);
}

void MainDlg::onReplyTryTunneling(QString peerUserName, bool canTunnel, bool needUpnp, QString failReason)
{
	if (!canTunnel)
	{
		QMessageBox::warning(this, U16("连接失败"), failReason);
		ui.btnTunnel->setEnabled(true);
		return;
	}

	insertTopUserName(peerUserName);
	const QString peerLocalPassword = QInputDialog::getText(this, U16("连接"), U16("输入 %1 的本地密码").arg(peerUserName), QLineEdit::Password);
	if (peerLocalPassword.isNull())
	{
		ui.btnTunnel->setEnabled(true);
		return;
	}

	m_bridge->slot_readyTunneling(peerUserName, peerLocalPassword, needUpnp);
}

void MainDlg::onReplyReadyTunneling(int requestId, int tunnelId, QString peerUserName)
{
	if (tunnelId == 0)
	{
		QMessageBox::warning(this, U16("连接失败"), U16("连接失败"));
		return;
	}

	ui.btnTunnel->setEnabled(true);
	updateTableRow(tunnelId, peerUserName, "", U16("准备中"));
}

void MainDlg::onTunnelStarted(int tunnelId, QString peerUserName, QHostAddress peerAddress)
{
	updateTableRow(tunnelId, peerUserName, peerAddress.toString(), U16("开始连接"));
	ui.statusBar->showMessage("");
}

void MainDlg::onTunnelHandShaked(int tunnelId)
{
	updateTableRow(tunnelId, QString(), QString(), U16("连接成功"));
}

void MainDlg::onTunnelClosed(int tunnelId, QString peerUserName, QString reason)
{
	deleteTableRow(tunnelId);
	ui.statusBar->showMessage(peerUserName + U16(" 连接断开 ") + reason);
}

void MainDlg::onBtnCloseTunneling()
{
	QPushButton * btnCloseTunneling = (QPushButton*)sender();
	if (!btnCloseTunneling)
		return;
	const int tunnelId = btnCloseTunneling->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	m_bridge->slot_closeTunneling(tunnelId);
	btnCloseTunneling->setEnabled(false);
}

void MainDlg::onBtnManageTransfer()
{
	QPushButton * btnManageTransfer = (QPushButton*)sender();
	if (!btnManageTransfer)
		return;
	const int tunnelId = btnManageTransfer->property("tunnelId").toInt();
	const QString peerUserName = btnManageTransfer->property("peerUserName").toString();
	if (tunnelId == 0 || peerUserName.isEmpty())
		return;

	TransferManageDlg dlg(this, tunnelId);
	connect(&dlg, SIGNAL(wannaQueryTransferIn(int)), m_bridge, SLOT(queryTransferIn(int)));
	connect(&dlg, SIGNAL(wannaQueryTransferOut(int)), m_bridge, SLOT(queryTransferOut(int)));
	connect(&dlg, SIGNAL(wannaAddTransfer(int, quint16, quint16, QHostAddress)),
		m_bridge, SLOT(addTransfer(int, quint16, quint16, QHostAddress)), Qt::QueuedConnection);
	connect(&dlg, SIGNAL(wannaDeleteTransfer(int, quint16)), m_bridge, SLOT(deleteTransfer(int, quint16)), Qt::QueuedConnection);
	dlg.init();
	dlg.exec();
}

void MainDlg::updateTableRow(int tunnelId, QString peerUserName, QString peerAddress, QString status)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_tableModel->findItems(key);
	if (lstItem.isEmpty())
	{
		lstItem << new QStandardItem(key) << new QStandardItem(peerUserName) << new QStandardItem(peerAddress)
			<< new QStandardItem(status) << new QStandardItem();
		m_tableModel->appendRow(lstItem);

		QPushButton * btnCloseTunneling = new QPushButton(U16("断开"));
		QPushButton * btnManageTransfer = new QPushButton(U16("管理转发"));
		btnCloseTunneling->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnCloseTunneling->setProperty("tunnelId", tunnelId);
		btnManageTransfer->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnManageTransfer->setProperty("tunnelId", tunnelId);
		btnManageTransfer->setProperty("peerUserName", peerUserName);

		connect(btnCloseTunneling, SIGNAL(clicked()), this, SLOT(onBtnCloseTunneling()));
		connect(btnManageTransfer, SIGNAL(clicked()), this, SLOT(onBtnManageTransfer()));

		QHBoxLayout * horizontalLayout = new QHBoxLayout();
		QWidget * containerWidget = new QWidget();
		containerWidget->setLayout(horizontalLayout);
		horizontalLayout->setMargin(0);
		horizontalLayout->setSpacing(0);
		horizontalLayout->addWidget(btnCloseTunneling);
		horizontalLayout->addWidget(btnManageTransfer);

		ui.tableView->setIndexWidget(m_tableModel->index(m_tableModel->rowCount() - 1, m_tableModel->columnCount() - 1), containerWidget);
	}
	else
	{
		const int row = lstItem.at(0)->row();
		if (!peerUserName.isNull())
			m_tableModel->item(row, 1)->setText(peerUserName);
		if (!peerAddress.isNull())
			m_tableModel->item(row, 2)->setText(peerAddress);
		if (!status.isNull())
			m_tableModel->item(row, 3)->setText(status);
	}
}

void MainDlg::deleteTableRow(int tunnelId)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_tableModel->findItems(key);
	if (lstItem.isEmpty())
		return;
	m_tableModel->removeRow(lstItem.at(0)->row());
}

void MainDlg::insertTopUserName(QString userName)
{
	bool noChange = false;
	for (int i = 0; i < ui.comboBoxPeerUserName->count(); ++i)
	{
		if (ui.comboBoxPeerUserName->itemText(i) == userName)
		{
			if (i > 0)
				ui.comboBoxPeerUserName->removeItem(i);
			else
				noChange = true;
			break;
		}
	}
	if (noChange)
		return;
	if (ui.comboBoxPeerUserName->count() >= 10)
		ui.comboBoxPeerUserName->removeItem(ui.comboBoxPeerUserName->count() - 1);
	ui.comboBoxPeerUserName->insertItem(0, userName);
	ui.comboBoxPeerUserName->setCurrentIndex(0);
}
