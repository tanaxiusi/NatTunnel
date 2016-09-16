
#include <QCoreApplication>
#include <QFile>
#include <QMutex>
#include <QSettings>
#include <time.h>
#include <iostream>
#include "ClientManager.h"

static QFile fileLog;
static QMutex mutexFileLog;

void MyMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString & text)
{
	const QDateTime datetime = QDateTime::currentDateTime();
	const char * typeText = nullptr;
	switch (type)
	{
	case QtDebugMsg:
	case QtInfoMsg:
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

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);

	fileLog.setFileName(app.applicationDirPath() + "/NatTunnelServer.log");
	fileLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
	qInstallMessageHandler(MyMessageHandler);

	QSettings setting("NatTunnelServer.ini", QSettings::IniFormat);
	QMap<QString, QString> mapUser;
	setting.beginGroup("User");
	for (QString userName : setting.childKeys())
		mapUser[userName] = setting.value(userName).toString();
	setting.endGroup();

	const int tcpPort = setting.value("Port/Tcp").toInt();
	const int udp1Port = setting.value("Port/Udp1").toInt();
	const int udp2Port = setting.value("Port/Udp2").toInt();

	ClientManager clientManager;
	clientManager.setUserList(mapUser);
	if (!clientManager.start(tcpPort, udp1Port, udp2Port))
		return 0;

	return app.exec();
}
