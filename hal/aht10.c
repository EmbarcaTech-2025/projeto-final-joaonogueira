#include "aht10.h"

static const uint8_t SENSOR_ADDR = 0x38;
static const uint8_t CMD_INITIALIZE[] = {0xE1, 0x08, 0x00};
static const uint8_t CMD_TRIGGER[] = {0xAC, 0x33, 0x00};
static const uint8_t CMD_SOFT_RESET[] = {0xBA};

static i2c_inst_t* i2c_port;

void aht10_init(i2c_inst_t *i2c) {
    i2c_port = i2c;
    
    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_SOFT_RESET, 1, false);
    sleep_ms(20);

    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_INITIALIZE, 3, false);
    sleep_ms(300);
}

bool aht10_read_data(float *temp, float *humidity) {
    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_TRIGGER, 3, false);
    sleep_ms(80);

    uint8_t data[6];
    if (i2c_read_blocking(i2c_port, SENSOR_ADDR, data, 6, false) != 6) {
        return false;
    }

    if ((data[0] & 0x80) != 0) {
        return false;
    }

    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    *humidity = ((float)raw_humidity / 1048576.0) * 100.0;
    *temp = ((float)raw_temp / 1048576.0) * 200.0 - 50.0;

    return true;
}