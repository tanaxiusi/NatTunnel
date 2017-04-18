#include "ServiceMain.h"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QTcpServer>
#include <QCryptographicHash>
#include <QTextCodec>
#include <time.h>
#include "BridgeForService.h"

static QFile g_fileLog;
static QMutex g_mutexFileLog;

#if defined Q_OS_WIN
#include <Windows.h>
static SERVICE_STATUS g_serviceStatus;
static SERVICE_STATUS_HANDLE g_statusHandle;
bool g_isWindowsService = false;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
static void MyMessageHandler(QtMsgType type, const QMessageLogContext & context, const QString & text)
#else
static void MyMessageHandler(QtMsgType type, const char * text)
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
	if (g_fileLog.isOpen())
	{
		QMutexLocker locker(&g_mutexFileLog);
		if (g_fileLog.size() == 0)
			g_fileLog.write("\xef\xbb\xbf");
		g_fileLog.write(finalText.toUtf8());
		g_fileLog.flush();
		locker.unlock();
	}
}

static void stopAndUninstallWindowsService()
{
#if defined(Q_OS_WIN)
	g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_statusHandle, &g_serviceStatus);

	const QString serviceName = "NatTunnelClient_" + QCryptographicHash::hash(QCoreApplication::applicationFilePath().toUtf8(), QCryptographicHash::Sha1).toHex();
	SC_HANDLE hSCM = OpenSCManager(0, 0, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	if (!hSCM)
		return;
	SC_HANDLE hMyService = OpenServiceW(hSCM, (LPCWSTR)serviceName.constData(), SERVICE_ALL_ACCESS);
	if (hMyService)
	{
		DeleteService(hMyService);
		CloseServiceHandle(hMyService);
	}
	CloseServiceHandle(hSCM);
#endif
}

#if defined(Q_OS_WIN)
static void WINAPI WindowsServiceCtrlHandler(DWORD request)
{
	if (request == SERVICE_CONTROL_STOP || request == SERVICE_CONTROL_SHUTDOWN)
	{
		stopAndUninstallWindowsService();
	}
}
#endif

static void leadWindowsFirewallPopup()
{
#if defined Q_OS_WIN
	// 创建一个UdpSocket，这样Windows防火墙会弹出提示，引导用户点击加入白名单
	// 如果不加入白名单，Udp收不到未发过地址传来的数据包
	QTcpServer tcpServer;
	tcpServer.listen(QHostAddress::Any, 0);
#endif
}

int ServiceMain(int argc, char *argv[])
{
#if defined(Q_OS_WIN)
	if (g_isWindowsService)
	{
		g_serviceStatus.dwServiceType = SERVICE_WIN32 | SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
		g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
		g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
		g_serviceStatus.dwWin32ExitCode = 0;
		g_serviceStatus.dwServiceSpecificExitCode = 0;
		g_serviceStatus.dwCheckPoint = 0;
		g_serviceStatus.dwWaitHint = 0;
		g_statusHandle = RegisterServiceCtrlHandlerW(L"NatTunnelClient", WindowsServiceCtrlHandler);
		if (g_statusHandle == 0)
			return 1;

		g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
		SetServiceStatus(g_statusHandle, &g_serviceStatus);
	}
#endif
	QCoreApplication app(argc, argv);

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	qInstallMessageHandler(MyMessageHandler);
#else
	QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf8"));
	qInstallMsgHandler(MyMessageHandler);
#endif

	srand(time(0));

	QDir::setCurrent(app.applicationDirPath());

	g_fileLog.setFileName(app.applicationDirPath() + "/NatTunnelClient.log");
	g_fileLog.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

	leadWindowsFirewallPopup();

	BridgeForService bridge;
	bridge.start();

	app.exec();
	stopAndUninstallWindowsService();
	return 0;
}