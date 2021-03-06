﻿#include "MainDlg.h"
#include <QTcpServer>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QProcess>
#include "Util/Other.h"
#include "GuideDlg.h"
#include "TransferManageDlg.h"

static const int Column_TunnelId = 0;
static const int Column_PeerUserName = 1;
static const int Column_PeerAddress = 2;
static const int Column_Status = 3;

MainDlg::MainDlg(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_trayIconIsValid = false;
	m_labelStatus = NULL;
	m_labelNatType = NULL;
	m_labelUpnp = NULL;
	m_tableModel = NULL;
	m_bridge = NULL;
	m_alwaysUseUpnp = false;

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
	connect(&m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(onTrayIconActivated(QSystemTrayIcon::ActivationReason)));

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

	const QByteArray serverKey = setting.value("Server/Key").toByteArray();
	const QHostAddress serverAddress = QHostAddress(setting.value("Server/Address").toString());
	const int serverPort = setting.value("Server/Port").toInt();

	const QString userName = setting.value("Client/UserName").toString();
	const QString localPassword = setting.value("Client/LocalPassword", QString::number(rand_u32())).toString();
	QString randomIdentifierSuffix = setting.value("Client/RandomIdentifierSuffix").toString();
	if (randomIdentifierSuffix.isEmpty())
	{
		randomIdentifierSuffix = QString::number(rand_u32());
		setting.setValue("Client/RandomIdentifierSuffix", randomIdentifierSuffix);
	}


	if(setting.value("Other/ShowTunnelId").toBool())
		ui.tableView->setColumnHidden(0, false);
	if (setting.value("Other/ShowAddress").toBool())
		ui.tableView->setColumnHidden(2, false);

	const bool disableBinaryCheck = setting.value("Other/DisableBinaryCheck").toBool();
	m_alwaysUseUpnp = setting.value("Other/AlwaysUseUpnp").toBool();

	const bool disableUpnpPublicNetworkCheck = setting.value("Other/DisableUpnpPublicNetworkCheck").toBool();

	ui.editUserName->setText(userName);
	ui.editLocalPassword->setText(localPassword);

	if (disableBinaryCheck)
		QMessageBox::warning(NULL, U16("警告"), U16("DisableBinaryCheck选项已开启，该功能仅用作测试"));

	m_trayIcon.setIcon(QIcon(setting.value("Other/TrayIcon", "TrayIcon.png").toString()));
	m_trayIconIsValid = m_trayIcon.icon().availableSizes().size() > 0;
	m_trayIcon.setToolTip("NatTunnelClient");
	m_trayIcon.show();

	m_bridge->slot_setConfig(serverKey, randomIdentifierSuffix, serverAddress, serverPort, disableBinaryCheck, disableUpnpPublicNetworkCheck);
	m_bridge->slot_setUserName(userName);
	m_bridge->slot_start();

	const bool hideAfterStart = setting.value("Other/HideAfterStart").toBool();
	const bool shouldHide = hideAfterStart && m_trayIconIsValid;
	if (!shouldHide)
		this->show();
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

	return QMainWindow::closeEvent(event);
}

void MainDlg::changeEvent(QEvent * event)
{
	if (event->type() == QEvent::WindowStateChange && this->isMinimized())
	{
		if(m_trayIconIsValid)
			this->hide();
	}
	return QMainWindow::changeEvent(event);
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

void MainDlg::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger)
	{
		if (this->isHidden())
		{
			this->show();
			if (this->isMinimized())
				this->showNormal();
			this->activateWindow();
		}
		else
		{
			this->hide();
		}
	}

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

	const QString peerLocalPassword = QInputDialog::getText(this, U16("连接"), U16("输入 %1 的本地密码").arg(peerUserName), QLineEdit::Password);
	if (peerLocalPassword.isNull())
	{
		ui.btnTunnel->setEnabled(true);
		return;
	}

	const bool useUpnp = needUpnp || m_alwaysUseUpnp;

	m_bridge->slot_readyTunneling(peerUserName, peerLocalPassword, useUpnp);
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

	const QString peerUserName = getPeerUserName(tunnelId);
	if (peerUserName.isEmpty())
		return;

	QSettings preset("Preset.ini", QSettings::IniFormat);
	preset.beginGroup(peerUserName);
	foreach(QString key, preset.childKeys())
	{
		const quint16 localPort = key.toInt();
		const Peer peer = Peer::fromString(preset.value(key).toString());

		m_bridge->slot_addTransfer(tunnelId, localPort, peer.port, peer.address);
	}
	preset.endGroup();
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

	TransferManageDlg dlg(this, tunnelId, peerUserName);
	connect(&dlg, SIGNAL(wannaGetTransferInList(int)), m_bridge, SLOT(slot_getTransferInList(int)));
	connect(&dlg, SIGNAL(wannaGetTransferOutList(int)), m_bridge, SLOT(slot_getTransferOutList(int)));
	connect(&dlg, SIGNAL(wannaAddTransfer(int, quint16, quint16, QHostAddress)),
		m_bridge, SLOT(slot_addTransfer(int, quint16, quint16, QHostAddress)));
	connect(&dlg, SIGNAL(wannaDeleteTransfer(int, quint16)), m_bridge, SLOT(slot_deleteTransfer(int, quint16)));
	connect(m_bridge, SIGNAL(event_onGetTransferInList(int, QMap<quint16, Peer>)),
		&dlg, SLOT(onGetTransferInList(int, QMap<quint16, Peer>)));
	connect(m_bridge, SIGNAL(event_onGetTransferOutList(int, QMap<quint16, Peer>)),
		&dlg, SLOT(onGetTransferOutList(int, QMap<quint16, Peer>)));
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
			m_tableModel->item(row, Column_PeerUserName)->setText(peerUserName);
		if (!peerAddress.isNull())
			m_tableModel->item(row, Column_PeerAddress)->setText(peerAddress);
		if (!status.isNull())
			m_tableModel->item(row, Column_Status)->setText(status);
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

QString MainDlg::getPeerUserName(int tunnelId)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_tableModel->findItems(key);
	if (lstItem.isEmpty())
		return QString();
	const int row = lstItem.at(0)->row();
	return m_tableModel->item(row, Column_PeerUserName)->text();
}
