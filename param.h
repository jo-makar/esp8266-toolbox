#ifndef PARAM_H
#define PARAM_H

#include <stdint.h>

#define PARAM_SMTP_OFFSET 0

int param_store(uint16_t offset, const uint8_t *val, uint16_t len);
int param_retrieve(uint16_t offset, uint8_t *val, uint16_t len);

#endif
