#pragma once
#include <stdint.h>
uint32_t crc32(const char *buf, uint32_t size);
uint32_t crc32(uint32_t crc, const char *buf, uint32_t size);