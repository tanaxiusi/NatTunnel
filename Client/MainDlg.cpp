#include "MainDlg.h"
#include <QTcpServer>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>

MainDlg::MainDlg(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	m_labelStatus = new QLabel(U16("未连接"));
	m_labelNatType = new QLabel(U16(" "));
	m_labelUpnp = new QLabel(U16(" "));

	m_labelStatus->setMinimumWidth(60);
	m_labelNatType->setMinimumWidth(60);
	m_labelUpnp->setMinimumWidth(60);

	ui.statusBar->addPermanentWidget(m_labelUpnp);
	ui.statusBar->addPermanentWidget(m_labelNatType);
	ui.statusBar->addPermanentWidget(m_labelStatus);

	ui.btnTunnel->setEnabled(false);

	m_model = new QStandardItemModel(ui.tableView);
	ui.tableView->setModel(m_model);
	m_model->setHorizontalHeaderLabels(U16("tunnelId,对方用户名,对方IP地址,状态,,").split(",", QString::SkipEmptyParts));
	ui.tableView->setColumnHidden(0, true);
	ui.tableView->setColumnWidth(1, 80);
	ui.tableView->setColumnWidth(2, 80);
	ui.tableView->setColumnWidth(3, 80);
	ui.tableView->setColumnWidth(4, 80);
	ui.tableView->setColumnWidth(5, 80);

	connect(&m_client, SIGNAL(connected()), this, SLOT(connected()));
	connect(&m_client, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(&m_client, SIGNAL(logined()), this, SLOT(logined()));
	connect(&m_client, SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));
	connect(&m_client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(natTypeConfirmed(NatType)));
	connect(&m_client, SIGNAL(firewallWarning()), this, SLOT(firewallWarning()));

	connect(&m_upnpPortMapper, SIGNAL(discoverFinished(bool)), this, SLOT(onUpnpDiscoverFinished(bool)));
	connect(&m_upnpPortMapper, SIGNAL(queryExternalAddressFinished(QHostAddress,bool,QString)), this, SLOT(onUpnpQueryExternalAddressFinished(QHostAddress, bool, QString)));

	connect(ui.editLocalPassword, SIGNAL(textChanged(const QString &)), this, SLOT(onEditLocalPasswordChanged()));
	connect(ui.btnTunnel, SIGNAL(clicked()), this, SLOT(onBtnTunnel()));
	connect(&m_client, SIGNAL(replyTryTunneling(QString, bool, bool, QString)), this, SLOT(onReplyTryTunneling(QString, bool, bool, QString)));
	connect(&m_client, SIGNAL(replyReadyTunneling(int, int, QString)), this, SLOT(onReplyReadyTunneling(int, int, QString)));
	connect(&m_client, SIGNAL(tunnelStarted(int, QString, QHostAddress)), this, SLOT(onTunnelStarted(int, QString, QHostAddress)));
	connect(&m_client, SIGNAL(tunnelHandShaked(int)), this, SLOT(onTunnelHandShaked(int)));
	connect(&m_client, SIGNAL(tunnelData(int, QByteArray)), this, SLOT(onTunnelData(int, QByteArray)));
	connect(&m_client, SIGNAL(tunnelClosed(int)), this, SLOT(onTunnelClosed(int)));

	connect(&m_client, SIGNAL(wannaAddUpnpPortMapping(quint16)), this, SLOT(addUpnpPortMapping(quint16)));
	connect(&m_client, SIGNAL(wannaDeleteUpnpPortMapping(quint16)), this, SLOT(deleteUpnpPortMapping(quint16)));

	leadWindowsFirewallPopup();

	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	const QHostAddress serverAddress = QHostAddress(setting.value("Server/Address").toString());
	const int serverPort = setting.value("Server/Port").toInt();

	const QString userName = setting.value("Client/UserName").toString();
	const QString password = setting.value("Client/Password").toString();
	const QString localPassword = setting.value("Client/LocalPassword").toString();

	ui.editUserName->setText(userName);
	ui.editLocalPassword->setText(localPassword);

	m_client.setUserInfo(userName, password);
	m_client.setServerInfo(serverAddress, serverPort);
	m_client.start();
}

MainDlg::~MainDlg()
{
	m_client.stop();
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
void MainDlg::connected()
{
	m_labelStatus->setText(U16("正在登录"));

	const QHostAddress localAddress = m_client.getLocalAddress();
	if (isNatAddress(localAddress))
	{
		m_upnpPortMapper.open(m_client.getLocalAddress());
		m_upnpPortMapper.discover();
		m_labelUpnp->setText(U16("正在检测upnp支持"));
	}
	else
	{
		m_labelUpnp->setText(U16("当前网络环境无需upnp"));
	}
}

void MainDlg::disconnected()
{
	m_labelStatus->setText(U16("断开"));
	m_labelNatType->clear();
	m_labelUpnp->clear();
	ui.btnTunnel->setEnabled(false);
}

void MainDlg::logined()
{
	m_labelStatus->setText(U16("登录成功"));
	m_labelNatType->setText(U16("正在检测NAT类型"));
}

void MainDlg::loginFailed(QString msg)
{
	m_labelStatus->setText(msg);
}

void MainDlg::natTypeConfirmed(NatType natType)
{
	m_labelNatType->setText(getNatDescription(natType));
	ui.btnTunnel->setEnabled(true);
}

void MainDlg::firewallWarning()
{
	ui.statusBar->showMessage(U16("当前处于公网环境，但是防火墙可能拦截了本程序"));
}

void MainDlg::onUpnpDiscoverFinished(bool ok)
{
	if (ok)
	{
		m_upnpPortMapper.queryExternalAddress();
		m_labelUpnp->setText(U16("upnp正在查询公网地址"));
	}
	else
	{
		m_client.setUpnpAvailable(false);
		m_labelUpnp->setText(U16("upnp不可用"));
	}
}

void MainDlg::onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString)
{
	if (ok)
	{
		if (isNatAddress(address))
		{
			ui.statusBar->showMessage(U16("upnp返回的地址 %2 仍然是内网地址").arg(address.toString()));
			return;
		}

		m_client.setUpnpAvailable(true);
		m_labelUpnp->setText(U16("upnp可用"));

		if (!isSameHostAddress(address, m_client.getLocalPublicAddress()))
			ui.statusBar->showMessage(U16("服务器端返回的IP地址 %1 和upnp返回的地址 %2 不同").arg(m_client.getLocalPublicAddress().toString()).arg(address.toString()));
	}else
	{
		m_client.setUpnpAvailable(false);
		m_labelUpnp->setText(U16("upnp获取公网地址失败"));
	}
}

void MainDlg::onEditLocalPasswordChanged()
{
	m_client.setLocalPassword(ui.editLocalPassword->text());
}

void MainDlg::onBtnTunnel()
{
	const QString peerUserName = ui.comboBoxPeerUserName->currentText();
	if (peerUserName.isEmpty())
		return;

	m_client.tryTunneling(peerUserName);
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

	m_client.readyTunneling(peerUserName, peerLocalPassword, needUpnp);
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
}

void MainDlg::onTunnelHandShaked(int tunnelId)
{
	updateTableRow(tunnelId, QString(), QString(), U16("连接成功"));

	TcpTransfer *& tcpTransfer = m_mapTcpTransfer[tunnelId];
	Q_ASSERT(!tcpTransfer);
	tcpTransfer = new TcpTransfer();
	tcpTransfer->setProperty("tunnelId", tunnelId);
	connect(tcpTransfer, SIGNAL(dataOutput(QByteArray)), this, SLOT(onTcpTransferOutput(QByteArray)));
}

void MainDlg::onTunnelData(int tunnelId, QByteArray package)
{
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (tcpTransfer)
		tcpTransfer->dataInput(package);
}

void MainDlg::onTunnelClosed(int tunnelId)
{
	deleteTableRow(tunnelId);

	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (tcpTransfer)
	{
		delete tcpTransfer;
		m_mapTcpTransfer.remove(tunnelId);
	}
}

void MainDlg::updateTableRow(int tunnelId, QString peerUsername, QString peerAddress, QString status)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_model->findItems(key);
	if (lstItem.isEmpty())
	{
		lstItem << new QStandardItem(key) << new QStandardItem(peerUsername) << new QStandardItem(peerAddress)
			<< new QStandardItem(status) << new QStandardItem() << new QStandardItem();
		m_model->appendRow(lstItem);
		QPushButton * btnCloseTunneling = new QPushButton(U16("断开"));
		QPushButton * btnAddTransfer = new QPushButton(U16("添加转发"));
		btnCloseTunneling->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnCloseTunneling->setProperty("tunnelId", tunnelId);
		btnAddTransfer->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
		btnAddTransfer->setProperty("tunnelId", tunnelId);

		connect(btnCloseTunneling, SIGNAL(clicked()), this, SLOT(onBtnCloseTunneling()));
		connect(btnAddTransfer, SIGNAL(clicked()), this, SLOT(onBtnAddTransfer()));

		ui.tableView->setIndexWidget(m_model->index(m_model->rowCount() - 1, m_model->columnCount() - 2), btnCloseTunneling);
		ui.tableView->setIndexWidget(m_model->index(m_model->rowCount() - 1, m_model->columnCount() - 1), btnAddTransfer);
	}
	else
	{
		const int row = lstItem.at(0)->row();
		if(!peerUsername.isNull())
			m_model->item(row, 1)->setText(peerUsername);
		if(!peerAddress.isNull())
			m_model->item(row, 2)->setText(peerAddress);
		if(!status.isNull())
			m_model->item(row, 3)->setText(status);
	}
}

void MainDlg::deleteTableRow(int tunnelId)
{
	const QString key = QString::number(tunnelId);
	QList<QStandardItem*> lstItem = m_model->findItems(key);
	if (lstItem.isEmpty())
		return;
	m_model->removeRow(lstItem.at(0)->row());
}

quint16 MainDlg::addUpnpPortMapping(quint16 internalPort)
{
	quint16 externalPort = (rand() & 0x7FFF) + 25000;
	m_upnpPortMapper.addPortMapping(QAbstractSocket::UdpSocket, m_upnpPortMapper.localAddress(), internalPort, externalPort, "NatTunnelClient");
	return externalPort;
}

void MainDlg::deleteUpnpPortMapping(quint16 externalPort)
{
	m_upnpPortMapper.deletePortMapping(QAbstractSocket::UdpSocket, externalPort);
}

void MainDlg::onBtnCloseTunneling()
{
	QPushButton * btnCloseTunneling = (QPushButton*)sender();
	if (!btnCloseTunneling)
		return;
	const int tunnelId = btnCloseTunneling->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	m_client.closeTunneling(tunnelId);
	btnCloseTunneling->setEnabled(false);
}

void MainDlg::onBtnAddTransfer()
{
	QPushButton * btnAddTransfer = (QPushButton*)sender();
	if (!btnAddTransfer)
		return;
	const int tunnelId = btnAddTransfer->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	TcpTransfer * tcpTransfer = m_mapTcpTransfer.value(tunnelId);
	if (!tcpTransfer)
		return;
	tcpTransfer->addTransfer(6033, 3306, QHostAddress("127.0.0.1"));
}

void MainDlg::onTcpTransferOutput(QByteArray package)
{
	TcpTransfer * tcpTransfer = (TcpTransfer*)sender();
	if (!tcpTransfer)
		return;
	const int tunnelId = tcpTransfer->property("tunnelId").toInt();
	if (tunnelId == 0)
		return;
	m_client.tunnelWrite(tunnelId, package);
}
