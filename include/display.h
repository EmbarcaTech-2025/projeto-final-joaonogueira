#ifndef DISPLAY_H
#define DISPLAY_H
#include <stdbool.h>
#include "hardware/i2c.h"

void display_init(i2c_inst_t *i2c_port, uint8_t i2c_address);

void display_update(float aht_temp, float humidity, float pressure, bool bmp_ok, float lux, bool bh1750_ok);

void display_render_sensor_data(float temperature, float humidity, float lux);

void display_render_wifi_status(const char* text, bool status, bool is_alert);

void display_render_alerts(bool temp_critical, bool humidity_critical, bool lux_critical);

void display_clear(void);

void display_show(void);

#endif