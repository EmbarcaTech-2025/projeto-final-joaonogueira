#ifndef AHT10_H
#define AHT10_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

void aht10_init(i2c_inst_t *i2c);

bool aht10_read_data(float *temp, float *humidity);

#endif