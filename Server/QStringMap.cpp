#include "QStringMap.h"

QStringMap toStringMap( const QVariantMap & v )
{
	QStringMap ret;
	for(QVariantMap::const_iterator iter = v.constBegin(); iter != v.constEnd(); ++iter)
		ret[iter.key()] = iter->toString();
	return ret;
}

QVariantMap toVariantMap( const QStringMap & v )
{
	QVariantMap ret;
	for(QStringMap::const_iterator iter = v.constBegin(); iter != v.constEnd(); ++iter)
		ret[iter.key()] = *iter;
	return ret;
}
