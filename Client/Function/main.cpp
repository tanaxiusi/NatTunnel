#include "UI/MainDlg.h"
#include "UI/GuideDlg.h"
#include <QApplication>
#include <QFile>
#include <QMutex>
#include <QSettings>
#include <QTextCodec>
#include <time.h>
#include <iostream>

static QFile fileLog;
static QMutex mutexFileLog;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
void MyMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString & text)
#else
void MyMessageHandler(QtMsgType type, const char * text)
#endif
{
	const QDateTime datetime = QDateTime::currentDateTime();
	const char * typeText = NULL;
	switch (type)
	{
	case QtDebugMsg:
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	case QtInfoMsg:
#endif
		typeText = "Info";
		break;
	case QtWarningMsg:
		typeText = "Warning";
		break;
	case QtCriticalMsg:
		typeText = "Critical";
		break;
	case QtFatalMsg:
		abort();
	}
	const QString finalText = QString("%1 %2 %3\n").arg(datetime.toString("yyyyMMdd/hh:mm:ss.zzz")).arg(typeText).arg(text);
	if (fileLog.isOpen())
	{
		QMutexLocker locker(&mutexFileLog);
		if (fileLog.size() == 0)
			fileLog.write("\xef\xbb\xbf");
		fileLog.write(finalText.toUtf8());
		fileLog.flush();
		locker.unlock();
	}

	std::cout << finalText.toLocal8Bit().constData();
}

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

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	fileLog.setFileName(app.applicationDirPath() + "/NatTunnelClient.log");
	fileLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	qInstallMessageHandler(MyMessageHandler);
#else
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf8"));
	qInstallMsgHandler(MyMessageHandler);
#endif

	srand(time(0));

	if (!showGuideReturnCanContinue())
		return 0;

	MainDlg wnd;
	wnd.show();
	return app.exec();
}
