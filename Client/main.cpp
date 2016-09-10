#include "MainDlg.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	MainDlg w;
	w.show();
	return app.exec();
}
