#include "MainDlg.h"

MainDlg::MainDlg(QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);

	m_client.setUserInfo("user1", "123456");
	m_client.setServerInfo(QHostAddress("127.0.0.1"), 7771);
	m_client.start();
}

MainDlg::~MainDlg()
{
	m_client.stop();
}
