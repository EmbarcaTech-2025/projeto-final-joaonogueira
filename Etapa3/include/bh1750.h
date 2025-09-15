#ifndef BH1750_H
#define BH1750_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

void bh1750_init(i2c_inst_t *i2c);

bool bh1750_read_lux(float *lux);

#endif