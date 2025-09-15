/**
 * @file mqtt_server.c
 * @brief High-Level MQTT Communication Manager
 * 
 * Provides abstracted interface for MQTT broker communication including
 * sensor data publication, alert management, and connection status monitoring.
 * Manages JSON payload formatting and topic organization for IoT data streams.
 */

#include "mqtt_server.h"
#include "mqtt_client.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== GLOBAL STATE VARIABLES ========== */

bool conct_status_mqtt = false; // MQTT broker connection status flag

/* ========== PUBLIC INTERFACE FUNCTIONS ========== */

/**
 * @brief Initialize MQTT client and establish broker connection
 * 
 * Configures MQTT client with predefined parameters and attempts connection
 * to public test broker. Uses test.mosquitto.org for reliable public access
 * without authentication requirements.
 */
void mqtt_conect_init() {
    // Initialize MQTT client with test.mosquitto.org public broker
    // Using direct IP address (91.121.93.94) to avoid DNS resolution issues
    mqtt_setup("pico_w_sensor", "91.121.93.94", &conct_status_mqtt);
}

/**
 * @brief Publish environmental sensor data to MQTT broker
 * 
 * Formats sensor readings into standardized JSON payload and publishes
 * to designated data topic. Handles sensor availability and provides
 * fallback values for missing measurements.
 * 
 * @param wifi_connected Current WiFi connection status
 * @param mqtt_connected Current MQTT broker connection status
 * @param aht_ok AHT10 temperature/humidity sensor operational status
 * @param bmp_ok BMP280 pressure sensor operational status (unused in this system)
 * @param lux_ok BH1750 light intensity sensor operational status
 * @param aht_temp Temperature reading from AHT10 (°C)
 * @param bmp_temp Temperature reading from BMP280 (°C, unused)
 * @param humidity Relative humidity reading (%)
 * @param pressure Atmospheric pressure reading (Pa, unused)
 * @param lux_val Light intensity reading (lux)
 */
void mqtt_get_and_publish(bool wifi_connected, bool mqtt_connected, bool aht_ok, bool bmp_ok, bool lux_ok,
    float aht_temp, float bmp_temp, float humidity, float pressure, float lux_val) {
    
    // Select best available temperature source (prioritize AHT10 over BMP280)
    float temp = aht_ok ? aht_temp : (bmp_ok ? bmp_temp : NAN);
    
    // Use sensor readings if available, otherwise set to NaN for JSON compatibility
    float hum = aht_ok ? humidity : NAN;
    float pres = bmp_ok ? pressure : NAN;
    float lux = lux_ok ? lux_val : NAN;
    
    // Create standardized JSON payload for sensor data publication
    char json_payload[256];
    sprintf(json_payload,
            "{\"temperatura\":%.2f, \"umidade\":%.2f, \"pressao\":%.2f, \"luminosidade\":%.1f}",
            temp,
            hum,
            pres / 100.0f, // Convert Pascal to hectoPascal (hPa) for standard meteorological units
            lux);
    
    // Publish sensor data only if both WiFi and MQTT connections are active
    if (wifi_connected && mqtt_connected) {
        mqtt_comm_publish("pico_w/sensors/data", (const uint8_t *)json_payload, strlen(json_payload));
    }
}

/**
 * @brief Publish environmental alert messages to MQTT broker
 * 
 * Transmits pre-formatted alert messages to dedicated alerts topic.
 * Used for notifying remote systems when environmental thresholds
 * are exceeded or critical conditions are detected.
 * 
 * @param wifi_connected Current WiFi connection status
 * @param mqtt_connected Current MQTT broker connection status
 * @param str Pre-formatted JSON alert message string
 */
void mqtt_get_and_publish2(bool wifi_connected, bool mqtt_connected, char *str) {
    // Publish alert message only if both WiFi and MQTT connections are active
    if (wifi_connected && mqtt_connected) {
        mqtt_comm_publish("pico_w/sensors/alerts", (const uint8_t *)str, strlen(str));
    }
}

/* ========== CONNECTION STATUS FUNCTIONS ========== */

/**
 * @brief Check current WiFi connection status
 * 
 * Queries the CYW43 wireless chip to determine current connection state
 * to the configured access point. Provides real-time connectivity status
 * for network-dependent operations.
 * 
 * @return true if WiFi is connected to access point, false otherwise
 */
bool wifi_check() {
    // Query WiFi chip for current station interface link status
    int8_t status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    // Return true for any connected state (joining or fully connected)
    return (status == CYW43_LINK_JOIN || status == CYW43_LINK_UP);
}

/**
 * @brief Check current MQTT broker connection status
 * 
 * Returns the current state of MQTT broker connection as maintained
 * by the low-level MQTT client. Updated by connection callbacks.
 * 
 * @return true if connected to MQTT broker, false otherwise
 */
bool mqtt_check() {
    return conct_status_mqtt; // Return global connection status flag
}