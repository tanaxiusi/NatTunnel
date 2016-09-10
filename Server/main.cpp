
#include <QCoreApplication>
#include <time.h>
#include "ClientManager.h"

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);

	qsrand(time(0));

	QMap<QString, QString> mapUserList;
	mapUserList["user1"] = "123456";
	mapUserList["user2"] = "654321";
	ClientManager clientManager;
	clientManager.setUserList(mapUserList);
	clientManager.start(7771, 7772, 7773);

	return app.exec();
}
