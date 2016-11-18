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
	m_client = NULL;
	m_transferManager = NULL;

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

	ui.btnRefreshOnlineUser->setIcon(QIcon(":/MainDlg/refresh.png"));

	connect(ui.editLocalPassword, SIGNAL(textChanged(const QString &)), this, SLOT(onEditLocalPasswordChanged()));
	connect(ui.btnRefreshOnlineUser, SIGNAL(clicked()), this, SLOT(onBtnRefreshOnlineUser()));
	connect(ui.btnTunnel, SIGNAL(clicked()), this, SLOT(onBtnTunnel()));

	leadWindowsFirewallPopup();

	start();
}

MainDlg::~MainDlg()
{
	stop();
}

void MainDlg::start()
{
	if (m_workingThread.isRunning())
		return;

	m_workingThread.start();

	m_client = new Client();
	m_transferManager = new TransferManager(NULL, m_client);

	m_client->moveToThread(&m_workingThread);
	m_transferManager->moveToThread(&m_workingThread);

	connect(m_client, SIGNAL(connected()), this, SLOT(onConnected()));
	connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	connect(m_client, SIGNAL(discarded(QString)), this, SLOT(onDiscarded(QString)));
	connect(m_client, SIGNAL(binaryError(QByteArray)), this, SLOT(onBinaryError(QByteArray)));
	connect(m_client, SIGNAL(logined()), this, SLOT(onLogined()));
	connect(m_client, SIGNAL(loginFailed(QString, QString)), this, SLOT(onLoginFailed(QString, QString)));
	connect(m_client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(onNatTypeConfirmed(NatType)));
	connect(m_client, SIGNAL(upnpStatusChanged(UpnpStatus)), this, SLOT(onClientUpnpStatusChanged(UpnpStatus)));
	connect(m_client, SIGNAL(warning(QString)), this, SLOT(onClientWarning(QString)));

	connect(m_client, SIGNAL(replyRefreshOnlineUser(QStringList)), this, SLOT(onReplyRefreshOnlineUser(QStringList)));
	connect(m_client, SIGNAL(replyTryTunneling(QString, bool, bool, QString)), this, SLOT(onReplyTryTunneling(QString, bool, bool, QString)));
	connect(m_client, SIGNAL(replyReadyTunneling(int, int, QString)), this, SLOT(onReplyReadyTunneling(int, int, QString)));
	connect(m_client, SIGNAL(tunnelStarted(int, QString, QHostAddress)), this, SLOT(onTunnelStarted(int, QString, QHostAddress)));
	connect(m_client, SIGNAL(tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)));
	connect(m_client, SIGNAL(tunnelClosed(int,QString,QString)), this, SLOT(onTunnelClosed(int,QString,QString)));

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

	m_client->setRandomIdentifierSuffix(randomIdentifierSuffix);
	m_client->setUserName(userName);
	m_client->setGlobalKey(serverKey);
	m_client->setServerInfo(serverAddress, serverPort);
	QMetaObject::invokeMethod(m_client, "start");
}

void MainDlg::stop()
{
	if (!m_workingThread.isRunning())
		return;

	m_client->deleteLater();
	m_transferManager->deleteLater();

	m_client = NULL;
	m_transferManager = NULL;

	m_workingThread.quit();
	m_workingThread.wait();
}


void MainDlg::leadWindowsFirewallPopup()
{
#if defined(Q_OS_WIN)
	// 创建一个TcpServer，这样Windows防火墙会弹出提示，引导用户点击加入白名单
	// 如果不加入白名单，Udp收不到未发过地址传来的数据包
	static bool invoked = false;
	if (invoked)
		return;
	QTcpServer tcpServer;
	tcpServer.listen(QHostAddress::Any, 0);
	invoked = true;
#endif
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
		scriptContent.replace("(PID)", QByteArray::number(QCoreApplication::applicationPid()));
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
	onBtnRefreshOnlineUser();
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

	QMetaObject::invokeMethod(m_client, "setUserName", Q_ARG(QString, newUserName));
	QMetaObject::invokeMethod(m_client, "tryLogin");
}

void MainDlg::onNatTypeConfirmed(NatType natType)
{
	m_labelNatType->setText(getNatDescription(natType));
	ui.btnTunnel->setEnabled(true);
}

void MainDlg::onClientUpnpStatusChanged(UpnpStatus upnpStatus)
{
	QString text;
	switch (upnpStatus)
	{
	case UpnpUnknownStatus:
		text = U16("Upnp未知状态");
		break;
	case UpnpDiscovering:
		text = U16("正在检测upnp支持");
		break;
	case UpnpUnneeded:
		text = U16("当前网络环境无需Upnp");
		break;
	case UpnpQueryingExternalAddress:
		text = U16("Upnp正在查询公网地址");
		break;
	case UpnpOk:
		text = U16("Upnp可用");
		break;
	case UpnpFailed:
		text = U16("Upnp不可用");
		break;
	default:
		break;
	}
	m_labelUpnp->setText(text);
}

void MainDlg::onClientWarning(QString msg)
{
	ui.statusBar->showMessage(msg);
}

void MainDlg::onEditLocalPasswordChanged()
{
	QMetaObject::invokeMethod(m_client, "setLocalPassword", Q_ARG(QString, ui.editLocalPassword->text()));
}

void MainDlg::onBtnRefreshOnlineUser()
{
	QMetaObject::invokeMethod(m_client, "refreshOnlineUser");
	ui.btnRefreshOnlineUser->setEnabled(false);
}

void MainDlg::onReplyRefreshOnlineUser(QStringList onlineUserList)
{
	const QString currentText = ui.comboBoxPeerUserName->currentText();
	ui.comboBoxPeerUserName->clear();
	ui.comboBoxPeerUserName->addItems(onlineUserList);

	ui.comboBoxPeerUserName->setEditText(currentText);
	ui.btnRefreshOnlineUser->setEnabled(true);
}

void MainDlg::onBtnTunnel()
{
	const QString peerUserName = ui.comboBoxPeerUserName->currentText();
	if (peerUserName.isEmpty())
		return;

	QMetaObject::invokeMethod(m_client, "tryTunneling", Q_ARG(QString, peerUserName));
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

	QMetaObject::invokeMethod(m_client, "readyTunneling",
		Q_ARG(QString, peerUserName), Q_ARG(QString, peerLocalPassword), Q_ARG(bool, needUpnp));
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
	QMetaObject::invokeMethod(m_client, "closeTunneling", Q_ARG(int, tunnelId));
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
	connect(&dlg, SIGNAL(wannaGetTransferOutList(int)), m_transferManager, SLOT(getTransferOutList(int)), Qt::BlockingQueuedConnection);
	connect(&dlg, SIGNAL(wannaGetTransferInList(int)), m_transferManager, SLOT(getTransferInList(int)), Qt::BlockingQueuedConnection);
	connect(&dlg, SIGNAL(wannaAddTransfer(int, quint16, quint16, QHostAddress)),
		m_transferManager, SLOT(addTransfer(int, quint16, quint16, QHostAddress)), Qt::QueuedConnection);
	connect(&dlg, SIGNAL(wannaDeleteTransfer(int, quint16)), m_transferManager, SLOT(deleteTransfer(int, quint16)), Qt::QueuedConnection);
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
