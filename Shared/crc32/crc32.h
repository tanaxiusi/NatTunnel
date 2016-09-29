#pragma once
#include <QtGlobal>
quint32 crc32(const char *buf, quint32 size);
quint32 crc32(quint32 crc, const char *buf, quint32 size);