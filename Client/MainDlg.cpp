#include "MainDlg.h"
#include <QTcpServer>

MainDlg::MainDlg(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	leadWindowsFirewallPopup();

	connect(&m_client, SIGNAL(connected()), this, SLOT(connected()));
	connect(&m_client, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(&m_client, SIGNAL(logined()), this, SLOT(logined()));
	connect(&m_client, SIGNAL(loginFailed(QString)), this, SLOT(loginFailed(QString)));
	connect(&m_client, SIGNAL(natTypeConfirmed(NatType)), this, SLOT(natTypeConfirmed(NatType)));
	connect(&m_client, SIGNAL(firewallWarning()), this, SLOT(firewallWarning()));

	connect(&m_upnpPortMapper, SIGNAL(discoverFinished(bool)), this, SLOT(onUpnpDiscoverFinished(bool)));
	connect(&m_upnpPortMapper, SIGNAL(queryExternalAddressFinished(QHostAddress,bool,QString)), this, SLOT(onUpnpQueryExternalAddressFinished(QHostAddress, bool, QString)));

	m_client.setUserInfo("user1", "123456");
	m_client.setServerInfo(QHostAddress("127.0.0.1"), 7771);
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

void MainDlg::connected()
{
	ui.plainTextEdit->appendPlainText("connected");

	const QHostAddress localAddress = m_client.getLocalAddress();
	if (isNatAddress(localAddress))
	{
		m_upnpPortMapper.open(m_client.getLocalAddress());
		m_upnpPortMapper.discover();
	}
}

void MainDlg::disconnected()
{
	ui.plainTextEdit->appendPlainText("disconnected");
}

void MainDlg::logined()
{
	ui.plainTextEdit->appendPlainText("logined");
}

void MainDlg::loginFailed(QString msg)
{
	ui.plainTextEdit->appendPlainText("loginFailed " + msg);
}

void MainDlg::natTypeConfirmed(NatType natType)
{
	ui.plainTextEdit->appendPlainText("natType = " + getNatDescription(natType));
}

void MainDlg::firewallWarning()
{
	ui.plainTextEdit->appendPlainText("WARNING! your firewall is blocking the connection");
}

void MainDlg::onUpnpDiscoverFinished(bool ok)
{
	if (ok)
	{
		m_client.setUpnpAvailable(true);
		ui.plainTextEdit->appendPlainText("upnp discoverd");
		m_upnpPortMapper.queryExternalAddress();
	}
	else
	{
		m_client.setUpnpAvailable(false);
		ui.plainTextEdit->appendPlainText("WARNING! upnp not enable");
	}
}

void MainDlg::onUpnpQueryExternalAddressFinished(QHostAddress address, bool ok, QString errorString)
{
	if (ok)
	{
		ui.plainTextEdit->appendPlainText(QString("upnp exteral address = %1").arg(tryConvertToIpv4(address).toString()));
		if (!isSameHostAddress(address, m_client.getLocalPublicAddress()))
			ui.plainTextEdit->appendPlainText(QString("WARNING! but not same as %1").arg(m_client.getLocalPublicAddress().toString()));
	}else
	{
		ui.plainTextEdit->appendPlainText("WARNING! upnp GetExternalIPAddress failed");
	}
}
