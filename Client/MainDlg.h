#ifndef MAINDLG_H
#define MAINDLG_H

#include <QtWidgets/QWidget>
#include "ui_MainDlg.h"

class MainDlg : public QWidget
{
	Q_OBJECT

public:
	MainDlg(QWidget *parent = 0);
	~MainDlg();

private:
	Ui::MainDlgClass ui;
};

#endif // MAINDLG_H
