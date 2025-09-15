#include "display.h"
#include "ssd1306.h"
#include <stdio.h>

static ssd1306_t disp;

void display_init(i2c_inst_t *i2c_port, uint8_t i2c_address) {
    ssd1306_init(&disp, 128, 64, i2c_address, i2c_port);
}

void display_update(float aht_temp, float humidity, float pressure, bool bmp_ok, float lux, bool bh1750_ok) {
    char line1[20], line2[20], line3[20], line4[20];
    ssd1306_clear(&disp);

    snprintf(line1, sizeof(line1), "Temp: %.1f C", aht_temp);
    snprintf(line2, sizeof(line2), "Umid: %.0f %%RH", humidity);

    if (bmp_ok) {
        snprintf(line3, sizeof(line3), "Pres: %.0f hPa", pressure);
    } else {
        snprintf(line3, sizeof(line3), "Pres: Falha");
    }

    if (bh1750_ok) {
        snprintf(line4, sizeof(line4), "Luz: %.0f lux", lux);
    } else {
        snprintf(line4, sizeof(line4), "Luz: Falha");
    }

    ssd1306_draw_string(&disp, 0, 0, 1, line1);
    ssd1306_draw_string(&disp, 0, 16, 1, line2);
    ssd1306_draw_string(&disp, 0, 32, 1, line3);
    ssd1306_draw_string(&disp, 0, 48, 1, line4);
    
    ssd1306_show(&disp);
}

void display_render_sensor_data(float temperature, float humidity, float lux) {
    char line1[20], line2[20], line3[20];
    ssd1306_clear(&disp);

    snprintf(line1, sizeof(line1), "Temp: %.1f C", temperature);
    snprintf(line2, sizeof(line2), "Umid: %.0f %%RH", humidity);
    snprintf(line3, sizeof(line3), "Luz: %.0f lux", lux);

    ssd1306_draw_string(&disp, 0, 0, 1, line1);
    ssd1306_draw_string(&disp, 0, 16, 1, line2);
    ssd1306_draw_string(&disp, 0, 32, 1, line3);
    
    ssd1306_show(&disp);
}

void display_render_wifi_status(const char* text, bool status, bool is_alert) {
    char line1[20], line2[20];
    ssd1306_clear(&disp);

    snprintf(line1, sizeof(line1), "WiFi: %s", text);
    if (is_alert) {
        snprintf(line2, sizeof(line2), "Status: %s", status ? "CRITICO" : "OK");
    } else {
        snprintf(line2, sizeof(line2), "Status: %s", status ? "Conectado" : "Desconectado");
    }

    ssd1306_draw_string(&disp, 0, 0, 1, line1);
    ssd1306_draw_string(&disp, 0, 16, 1, line2);
    
    ssd1306_show(&disp);
}

void display_render_alerts(bool temp_critical, bool humidity_critical, bool lux_critical) {
    char line1[20], line2[20], line3[20];
    ssd1306_clear(&disp);

    snprintf(line1, sizeof(line1), "Temp: %s", temp_critical ? "CRITICO" : "OK");
    snprintf(line2, sizeof(line2), "Umid: %s", humidity_critical ? "CRITICO" : "OK");
    snprintf(line3, sizeof(line3), "Luz: %s", lux_critical ? "CRITICO" : "OK");

    ssd1306_draw_string(&disp, 0, 0, 1, line1);
    ssd1306_draw_string(&disp, 0, 16, 1, line2);
    ssd1306_draw_string(&disp, 0, 32, 1, line3);
    
    ssd1306_show(&disp);
}

void display_clear(void) {
    ssd1306_clear(&disp);
}

void display_show(void) {
    ssd1306_show(&disp);
}