#include "MainDlg.h"
#include <QApplication>
#include <QFile>
#include <QMutex>
#include <time.h>
#include <iostream>

static QFile fileLog;
static QMutex mutexFileLog;

#ifdef _DEBUG
#include "vld.h"
#endif

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
	QApplication app(argc, argv);

	fileLog.setFileName(app.applicationDirPath() + "/NatTunnelClient.log");
	fileLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
	qInstallMessageHandler(MyMessageHandler);

	srand(time(0));

	MainDlg wnd;
	wnd.show();
	return app.exec();
}
