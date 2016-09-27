#pragma once
#include <QString>
#include <QMap>
#include <QVariant>

typedef QMap<QString, QString> QStringMap;

QStringMap toStringMap(const QVariantMap & v);
QVariantMap toVariantMap(const QStringMap & v);