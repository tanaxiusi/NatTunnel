#include "GuiMain.h"
#include <QSettings>
#include <QTextCodec>
#include <time.h>
#include "Gui/UI/GuideDlg.h"
#include "Gui/UI/MainDlg.h"

bool showGuideReturnCanContinue()
{
	QSettings setting("NatTunnelClient.ini", QSettings::IniFormat);
	const bool inited = setting.value("Server/Inited").toBool();
	if (!inited)
	{
		GuideDlg guideDlg;
		guideDlg.setServerAddress(setting.value("Server/Address").toString());
		guideDlg.setServerPort(setting.value("Server/Port").toInt());
		guideDlg.setServerKey(setting.value("Server/Key").toString());
		if (guideDlg.exec() != QDialog::Accepted)
			return false;
		setting.setValue("Server/Inited", true);

		setting.setValue("Server/Address", guideDlg.serverAddress());
		setting.setValue("Server/Port", guideDlg.serverPort());
		setting.setValue("Server/Key", guideDlg.serverKey());
	}
	return true;
}

int GuiMain(int argc, char *argv[])
{
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#else
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf8"));
#endif

	srand(time(0));

	if (!showGuideReturnCanContinue())
		return 0;

	MainDlg wnd;
	wnd.show();
	return app.exec();
}
