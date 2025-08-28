#include "bh1750.h"

static const uint8_t SENSOR_ADDR = 0x23;
static const uint8_t CONT_HRES_MODE = 0x10;

static i2c_inst_t* i2c_port;

void bh1750_init(i2c_inst_t *i2c) {
    i2c_port = i2c;

    uint8_t cmd = CONT_HRES_MODE;
    i2c_write_blocking(i2c_port, SENSOR_ADDR, &cmd, 1, false);
    
    sleep_ms(180);
}

bool bh1750_read_lux(float *lux) {
    uint8_t data[2];
    int bytes_read = i2c_read_blocking(i2c_port, SENSOR_ADDR, data, 2, false);

    if (bytes_read != 2) {
        *lux = 0;
        return false;
    }

    uint16_t raw = (data[0] << 8) | data[1];
    
    *lux = raw / 1.2f;

    return true;
}