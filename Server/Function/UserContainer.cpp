#include "UserContainer.h"
#include <QVariant>
#include <QtDebug>
#include <QSqlError>
#include <QSqlRecord>

bool UserContainer::open()
{
	if (m_database.isValid())
		return false;

	m_database = QSqlDatabase::addDatabase("QSQLITE");
	m_database.setDatabaseName(m_databaseName);
	m_database.setUserName(m_databaseUserName);
	m_database.setPassword(m_databasePassword);

	if (!m_database.open())
	{
		m_database = QSqlDatabase();
		return false;
	}
	else
	{
		checkVersion();
		return m_database.isValid();
	}
}

void UserContainer::close()
{
	if (!m_database.isValid())
		return;
	m_database.close();
	m_database = QSqlDatabase();
}

void UserContainer::setDatabaseConfig(QString name, QString userName, QString password)
{
	if (m_database.isValid())
		return;
	m_databaseName = name;
	m_databaseUserName = userName;
	m_databasePassword = password;
}

QString UserContainer::getBoundIdentifier(QString userName)
{
	QSqlQuery query(m_database);
	query.prepare("SELECT identifier FROM user WHERE userName=:userName;");
	query.bindValue(":userName", userName);
	if (!query.exec())
	{
		printError(query);
		return QString();
	}
	if (query.next())
		return query.record().value("identifier").toString();
	else
		return QString();
}

QString UserContainer::getBoundUserName(QString identifier)
{
	QSqlQuery query(m_database);
	query.prepare("SELECT userName FROM user WHERE identifier=:identifier;");
	query.bindValue(":identifier", identifier);
	if (!query.exec())
	{
		printError(query);
		return QString();
	}
	if (query.next())
		return query.record().value("userName").toString();
	else
		return QString();
}

bool UserContainer::removeBound(QString userName, QString identifier)
{
	QSqlQuery query(m_database);
	query.prepare("DELETE FROM user WHERE userName=:userName and identifier=:identifier;");
	query.bindValue(":userName", userName);
	query.bindValue(":identifier", identifier);
	if (!query.exec())
	{
		printError(query);
		return false;
	}
	return true;
}

bool UserContainer::newBound(QString userName, QString identifier)
{
	QSqlQuery query(m_database);
	query.prepare("INSERT INTO user (userName, identifier)VALUES(:userName, :identifier);");
	query.bindValue(":userName", userName);
	query.bindValue(":identifier", identifier);
	if (!query.exec())
	{
		printError(query);
		return false;
	}
	return true;
}

int UserContainer::getVersion()
{
	QSqlQuery query(m_database);
	if (!query.exec("SELECT version FROM globalInfo;"))
		return 0;
	if (query.next())
		return query.record().value(0).toInt();
	else
		return 0;
}

void UserContainer::checkVersion()
{
	static const int standardVersion = 1;
	const int version = getVersion();
	if (version == 0)
	{
		setVersion(1);
		createTableUser();
	}
	else if (version > standardVersion)
	{
		qWarning() << QString("UserContainer::checkVersion version %1 is higher than %2").arg(version).arg(standardVersion);
		close();
	}
}

bool UserContainer::createTableUser()
{
	QSqlQuery query(m_database);
	if (!query.exec("CREATE TABLE IF NOT EXISTS'user' ('userName' TEXT PRIMARY KEY UNIQUE, 'identifier' TEXT UNIQUE);"))
	{
		printError(query);
		return false;
	}
	return true;
}

bool UserContainer::setVersion(int version)
{
	QSqlQuery query(m_database);
	if (!query.exec("CREATE TABLE IF NOT EXISTS 'globalInfo' ('id' INTEGER PRIMARY KEY UNIQUE, 'version' INTEGER);"))
	{
		printError(query);
		return false;
	}
	query.exec(QString("INSERT INTO globalInfo (id,version)VALUES(0, %1);").arg(version));

	query.prepare("UPDATE globalInfo SET version=:version;");
	query.bindValue(":version", version);
	if (!query.exec())
	{
		printError(query);
		return false;
	}
	return true;
}

void UserContainer::printError(QSqlQuery & query)
{
	const QSqlError error = query.lastError();
	if (error.type() == QSqlError::NoError)
		return;
	qWarning() << QString("UserContainer '%1' error : %2, %3")
		.arg(query.lastQuery()).arg(error.databaseText()).arg(error.driverText());
}

