#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>

class UserContainer
{
public:
	bool open();
	void close();

	void setDatabaseConfig(QString name, QString userName, QString password);

	QString getBoundIdentifier(QString userName);
	QString getBoundUserName(QString identifier);
	bool removeBound(QString userName, QString identifier);
	bool newBound(QString userName, QString identifier);

private:
	int getVersion();
	bool setVersion(int version);
	void checkVersion();
	bool createTableUser();
	void printError(QSqlQuery & query);

private:
	QString m_databaseName;
	QString m_databaseUserName;
	QString m_databasePassword;

	QSqlDatabase m_database;
};