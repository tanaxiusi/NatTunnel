#pragma once
#include <QByteArray>
#include <QMap>
#include <QHostAddress>

enum NatType
{
	UnknownNatType = 0,
	PublicNetwork,
	FullOrRestrictedConeNat,
	PortRestrictedConeNat,
	SymmetricNat
};

typedef QMap<QByteArray, QByteArray> QByteArrayMap;

QByteArray parseRequest(QByteArray line, QByteArrayMap * outArgument);
QByteArray serializeResponse(QByteArray type, QByteArrayMap argument);
QByteArray checksumThenUnpackUdpPackage(QByteArray package);
bool isSameHostAddress(const QHostAddress & a, const QHostAddress & b);
QHostAddress tryConvertToIpv4(const QHostAddress & hostAddress);