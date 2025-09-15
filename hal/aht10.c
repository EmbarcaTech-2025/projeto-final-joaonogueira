/**
 * @file aht10.c
 * @brief AHT10 Temperature and Humidity Sensor Driver
 * 
 * High-precision digital temperature and humidity sensor driver implementing
 * I2C communication protocol. Provides calibrated readings with built-in
 * compensation algorithms for accurate environmental monitoring.
 */

#include "aht10.h"

/* ========== SENSOR CONFIGURATION CONSTANTS ========== */

static const uint8_t SENSOR_ADDR = 0x38;                    // Fixed I2C address for AHT10 sensor
static const uint8_t CMD_INITIALIZE[] = {0xE1, 0x08, 0x00}; // Sensor initialization command sequence
static const uint8_t CMD_TRIGGER[] = {0xAC, 0x33, 0x00};    // Measurement trigger command sequence
static const uint8_t CMD_SOFT_RESET[] = {0xBA};             // Software reset command

/* ========== PRIVATE VARIABLES ========== */

static i2c_inst_t* i2c_port; // I2C interface instance for sensor communication

/* ========== PUBLIC INTERFACE FUNCTIONS ========== */

/**
 * @brief Initialize AHT10 sensor for operation
 * 
 * Performs complete sensor initialization sequence including software reset,
 * initialization command transmission, and calibration setup. Must be called
 * before attempting to read sensor data.
 * 
 * @param i2c Pointer to I2C interface instance (i2c0 or i2c1)
 */
void aht10_init(i2c_inst_t *i2c) {
    i2c_port = i2c; // Store I2C interface reference for subsequent operations
    
    // Execute software reset to ensure clean sensor state
    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_SOFT_RESET, 1, false);
    sleep_ms(20); // Wait for reset completion as per datasheet timing
    
    // Send initialization command to configure sensor parameters
    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_INITIALIZE, 3, false);
    sleep_ms(300); // Wait for initialization completion and calibration
}

/**
 * @brief Read calibrated temperature and humidity values
 * 
 * Triggers a new measurement, waits for conversion completion, reads raw data,
 * and applies calibration formulas to provide accurate temperature (°C) and
 * relative humidity (%) values.
 * 
 * @param temp Pointer to store temperature reading (°C, range: -40 to +85)
 * @param humidity Pointer to store humidity reading (%, range: 0 to 100)
 * @return true if measurement successful, false on communication error or busy sensor
 */
bool aht10_read_data(float *temp, float *humidity) {
    // Trigger measurement conversion in sensor
    i2c_write_blocking(i2c_port, SENSOR_ADDR, CMD_TRIGGER, 3, false);
    sleep_ms(80); // Wait for measurement completion (typical: 75ms, max: 80ms)
    
    // Read 6-byte result data from sensor
    uint8_t data[6];
    if (i2c_read_blocking(i2c_port, SENSOR_ADDR, data, 6, false) != 6) {
        return false; // Communication error - insufficient data received
    }
    
    // Check busy flag in status byte (bit 7 should be 0 when ready)
    if ((data[0] & 0x80) != 0) {
        return false; // Sensor still busy - measurement not complete
    }
    
    // Extract 20-bit humidity value from data bytes [1:3]
    uint32_t raw_humidity = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    
    // Extract 20-bit temperature value from data bytes [3:5]
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    
    // Apply calibration formulas as specified in AHT10 datasheet
    *humidity = ((float)raw_humidity / 1048576.0) * 100.0;           // Convert to %RH (0-100%)
    *temp = ((float)raw_temp / 1048576.0) * 200.0 - 50.0;          // Convert to °C (-40 to +85°C)
    
    return true; // Measurement successful
}