#include "Service/ServiceMain.h"
#include "Gui/GuiMain.h"
#include <string>
#include <QString>

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
	if (argc >= 2)
	{
		const bool isWindowsService = strcmp(argv[1], "WindowsService") == 0;
		const bool isCommandService = strcmp(argv[1], "CommandService") == 0;
		if (isWindowsService)
		{
#if defined(Q_OS_WIN)
			g_isWindowsService = true;
			SERVICE_TABLE_ENTRYW entrytable[2];
			std::wstring strServiceName = L"NatTunnelClient";
			entrytable[0].lpServiceName = (LPWSTR)strServiceName.c_str();
			entrytable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;
			entrytable[1].lpServiceName = NULL;
			entrytable[1].lpServiceProc = NULL;
			StartServiceCtrlDispatcherW(entrytable);
#endif
		}
		else if (isCommandService)
		{
			ServiceMain(argc, argv);
		}
	}
	else
	{
		GuiMain(argc, argv);
	}
}

