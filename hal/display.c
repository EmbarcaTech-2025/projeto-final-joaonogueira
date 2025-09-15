/**
 * @file display.c
 * @brief High-Level Display Interface for Environmental Data Visualization
 * 
 * Provides abstracted interface for rendering environmental sensor data,
 * system status information, and alert states on SSD1306 OLED display.
 * Manages display formatting, layout, and content organization.
 */

#include "display.h"
#include "ssd1306.h"
#include <stdio.h>

/* ========== PRIVATE VARIABLES ========== */

static ssd1306_t disp; // Display driver instance for SSD1306 OLED

/* ========== PUBLIC INTERFACE FUNCTIONS ========== */

/**
 * @brief Initialize OLED display subsystem
 * 
 * Configures SSD1306 display driver with specified I2C interface and
 * device address. Sets up 128x64 pixel monochrome display for text rendering.
 * 
 * @param i2c_port Pointer to I2C interface instance (i2c0 or i2c1)
 * @param i2c_address I2C device address of OLED display (typically 0x3C)
 */
void display_init(i2c_inst_t *i2c_port, uint8_t i2c_address) {
    // Initialize SSD1306 driver with display parameters
    ssd1306_init(&disp, 128, 64, i2c_address, i2c_port);
}

/**
 * @brief Render comprehensive environmental sensor data display
 * 
 * Creates formatted multi-line display showing temperature, humidity,
 * atmospheric pressure, and light intensity with sensor status indicators.
 * Layout optimized for 128x64 OLED display readability.
 * 
 * @param aht_temp Temperature reading from AHT10 sensor (°C)
 * @param humidity Relative humidity reading (%)
 * @param pressure Atmospheric pressure reading (hPa) - may be unused
 * @param bmp_ok BMP280 sensor operational status
 * @param lux Light intensity reading (lux)
 * @param bh1750_ok BH1750 sensor operational status
 */
void display_update(float aht_temp, float humidity, float pressure, bool bmp_ok, float lux, bool bh1750_ok) {
    char line1[20], line2[20], line3[20], line4[20]; // Text buffer for each display line
    ssd1306_clear(&disp); // Clear display buffer for fresh content
    
    // Format temperature reading with single decimal precision
    snprintf(line1, sizeof(line1), "Temp: %.1f C", aht_temp);
    
    // Format humidity reading with integer precision and percentage symbol
    snprintf(line2, sizeof(line2), "Umid: %.0f %%RH", humidity);
    
    // Format pressure reading with sensor status indication
    if (bmp_ok) {
        snprintf(line3, sizeof(line3), "Pres: %.0f hPa", pressure);
    } else {
        snprintf(line3, sizeof(line3), "Pres: Falha"); // Indicate sensor failure
    }
    
    // Format light intensity reading with sensor status indication
    if (bh1750_ok) {
        snprintf(line4, sizeof(line4), "Luz: %.0f lux", lux);
    } else {
        snprintf(line4, sizeof(line4), "Luz: Falha"); // Indicate sensor failure
    }
    
    // Render text lines at 16-pixel intervals for proper spacing
    ssd1306_draw_string(&disp, 0, 0, 1, line1);   // Line 1: Temperature
    ssd1306_draw_string(&disp, 0, 16, 1, line2);  // Line 2: Humidity
    ssd1306_draw_string(&disp, 0, 32, 1, line3);  // Line 3: Pressure
    ssd1306_draw_string(&disp, 0, 48, 1, line4);  // Line 4: Light intensity
    
    ssd1306_show(&disp); // Update physical display with buffered content
}

/**
 * @brief Render simplified sensor data display
 * 
 * Displays core environmental parameters (temperature, humidity, light)
 * in compact format suitable for primary monitoring screen.
 * 
 * @param temperature Current temperature reading (°C)
 * @param humidity Current relative humidity reading (%)
 * @param lux Current light intensity reading (lux)
 */
void display_render_sensor_data(float temperature, float humidity, float lux) {
    char line1[20], line2[20], line3[20]; // Text buffer for each display line
    ssd1306_clear(&disp); // Clear display buffer for fresh content
    
    // Format core sensor readings for display
    snprintf(line1, sizeof(line1), "Temp: %.1f C", temperature);
    snprintf(line2, sizeof(line2), "Umid: %.0f %%RH", humidity);
    snprintf(line3, sizeof(line3), "Luz: %.0f lux", lux);
    
    // Render text lines with consistent spacing
    ssd1306_draw_string(&disp, 0, 0, 1, line1);   // Line 1: Temperature
    ssd1306_draw_string(&disp, 0, 16, 1, line2);  // Line 2: Humidity
    ssd1306_draw_string(&disp, 0, 32, 1, line3);  // Line 3: Light intensity
    
    ssd1306_show(&disp); // Update physical display with buffered content
}

/**
 * @brief Render network connectivity status display
 * 
 * Shows WiFi connection information including network name and connection
 * state. Can display both standard connectivity status and alert conditions.
 * 
 * @param text Network name or status text to display
 * @param status Connection state (true = connected/critical, false = disconnected/ok)
 * @param is_alert true to display as alert status, false for normal network status
 */
void display_render_wifi_status(const char* text, bool status, bool is_alert) {
    char line1[20], line2[20]; // Text buffer for display lines
    ssd1306_clear(&disp); // Clear display buffer for fresh content
    
    // Format network identification line
    snprintf(line1, sizeof(line1), "WiFi: %s", text);
    
    // Format status line based on display mode
    if (is_alert) {
        // Alert mode: show critical/ok status
        snprintf(line2, sizeof(line2), "Status: %s", status ? "CRITICO" : "OK");
    } else {
        // Normal mode: show connection status
        snprintf(line2, sizeof(line2), "Status: %s", status ? "Conectado" : "Desconectado");
    }
    
    // Render status information
    ssd1306_draw_string(&disp, 0, 0, 1, line1);   // Line 1: Network name
    ssd1306_draw_string(&disp, 0, 16, 1, line2);  // Line 2: Connection status
    
    ssd1306_show(&disp); // Update physical display with buffered content
}

/**
 * @brief Render environmental alert status display
 * 
 * Shows current alert state for all monitored environmental parameters.
 * Provides quick overview of which sensors have exceeded thresholds.
 * 
 * @param temp_critical true if temperature is outside acceptable range
 * @param humidity_critical true if humidity exceeds maximum threshold
 * @param lux_critical true if light intensity is below minimum threshold
 */
void display_render_alerts(bool temp_critical, bool humidity_critical, bool lux_critical) {
    char line1[20], line2[20], line3[20]; // Text buffer for each alert line
    ssd1306_clear(&disp); // Clear display buffer for fresh content
    
    // Format alert status for each monitored parameter
    snprintf(line1, sizeof(line1), "Temp: %s", temp_critical ? "CRITICO" : "OK");
    snprintf(line2, sizeof(line2), "Umid: %s", humidity_critical ? "CRITICO" : "OK");
    snprintf(line3, sizeof(line3), "Luz: %s", lux_critical ? "CRITICO" : "OK");
    
    // Render alert status information
    ssd1306_draw_string(&disp, 0, 0, 1, line1);   // Line 1: Temperature alert
    ssd1306_draw_string(&disp, 0, 16, 1, line2);  // Line 2: Humidity alert
    ssd1306_draw_string(&disp, 0, 32, 1, line3);  // Line 3: Light intensity alert
    
    ssd1306_show(&disp); // Update physical display with buffered content
}

/**
 * @brief Clear display buffer without updating screen
 * 
 * Clears the internal display buffer to blank state. Display will remain
 * unchanged until display_show() is called.
 */
void display_clear(void) {
    ssd1306_clear(&disp);
}

/**
 * @brief Update physical display with current buffer content
 * 
 * Transfers the current display buffer content to the physical OLED
 * display, making all buffered changes visible.
 */
void display_show(void) {
    ssd1306_show(&disp);
}