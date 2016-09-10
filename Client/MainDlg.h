#pragma once

#include <QWidget>
#include "ui_MainDlg.h"
#include "Client.h"

class MainDlg : public QWidget
{
	Q_OBJECT

public:
	MainDlg(QWidget *parent = 0);
	~MainDlg();

private:
	Ui::MainDlgClass ui;
	Client m_client;
};
