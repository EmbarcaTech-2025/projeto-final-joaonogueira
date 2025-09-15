/**
 * @file bh1750.c
 * @brief BH1750 Digital Light Intensity Sensor Driver
 * 
 * High-resolution digital ambient light sensor driver providing calibrated
 * luminosity measurements in lux units. Implements continuous high-resolution
 * measurement mode for real-time light monitoring applications.
 */

#include "bh1750.h"

/* ========== SENSOR CONFIGURATION CONSTANTS ========== */

static const uint8_t SENSOR_ADDR = 0x23;      // Standard I2C address for BH1750 sensor
static const uint8_t CONT_HRES_MODE = 0x10;   // Continuous high-resolution measurement mode (1 lx resolution)

/* ========== PRIVATE VARIABLES ========== */

static i2c_inst_t* i2c_port; // I2C interface instance for sensor communication

/* ========== PUBLIC INTERFACE FUNCTIONS ========== */

/**
 * @brief Initialize BH1750 sensor for continuous light measurement
 * 
 * Configures the sensor for continuous high-resolution mode providing
 * 1 lux resolution with automatic measurement cycling. Sensor will
 * continuously update internal measurement register.
 * 
 * @param i2c Pointer to I2C interface instance (i2c0 or i2c1)
 */
void bh1750_init(i2c_inst_t *i2c) {
    i2c_port = i2c; // Store I2C interface reference for subsequent operations
    
    // Configure sensor for continuous high-resolution measurement mode
    uint8_t cmd = CONT_HRES_MODE;
    i2c_write_blocking(i2c_port, SENSOR_ADDR, &cmd, 1, false);
    
    // Wait for first measurement completion (typical: 120ms, max: 180ms)
    sleep_ms(180);
}

/**
 * @brief Read current light intensity measurement
 * 
 * Retrieves the most recent light intensity measurement from the sensor's
 * internal register. In continuous mode, this value is automatically
 * updated by the sensor approximately every 120ms.
 * 
 * @param lux Pointer to store light intensity reading (range: 0 to 65535 lux)
 * @return true if measurement successful, false on communication error
 */
bool bh1750_read_lux(float *lux) {
    // Read 16-bit measurement data from sensor (MSB first)
    uint8_t data[2];
    int bytes_read = i2c_read_blocking(i2c_port, SENSOR_ADDR, data, 2, false);
    
    // Verify complete data reception
    if (bytes_read != 2) {
        *lux = 0; // Set safe default value on communication failure
        return false;
    }
    
    // Combine MSB and LSB to form 16-bit raw measurement value
    uint16_t raw = (data[0] << 8) | data[1];
    
    // Apply calibration factor to convert raw value to lux
    // BH1750 datasheet specifies: Lux = Raw_Value / 1.2 (at default sensitivity)
    *lux = raw / 1.2f;
    
    return true; // Measurement successful
}