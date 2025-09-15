/**
 * @file main.c
 * @brief Sistema de Monitoramento Ambiental IoT com Raspberry Pi Pico W
 * 
 * Este sistema realiza monitoramento contínuo de sensores ambientais (temperatura,
 * umidade e luminosidade), exibe dados em display OLED, publica via MQTT e 
 * implementa sistema de alertas para valores críticos.
 */

// Standard C libraries
#include <stdio.h>          // Standard I/O operations
#include <string.h>         // String manipulation functions
#include <math.h>           // Mathematical functions (NAN, etc.)

// Pico SDK core libraries
#include "pico/stdlib.h"    // Pico standard library (GPIO, time, etc.)
#include "hardware/i2c.h"   // Hardware I2C interface
#include "hardware/gpio.h"  // Hardware GPIO control
#include "pico/cyw43_arch.h" // WiFi chip (CYW43) architecture support

// Application-specific modules
#include "aht10.h"          // AHT10 temperature/humidity sensor driver
#include "bh1750.h"         // BH1750 light intensity sensor driver
#include "display.h"        // SSD1306 OLED display interface
#include "mqtt_server.h"    // MQTT communication manager

/* ========== HARDWARE CONFIGURATION ========== */

// I2C Bus A: Environmental sensors (AHT10 + BH1750)
#define I2C_PORT_A i2c0            // Primary I2C interface for sensors
const uint I2C_SDA_PIN_A = 0;      // GPIO 0: I2C SDA line for sensors
const uint I2C_SCL_PIN_A = 1;      // GPIO 1: I2C SCL line for sensors

// I2C Bus B: OLED Display (SSD1306)
#define I2C_PORT_B i2c1            // Secondary I2C interface for display
const uint8_t I2C_OLED_ADDR = 0x3C; // Standard I2C address for SSD1306 OLED
const uint I2C_SDA_PIN_B = 14;     // GPIO 14: I2C SDA line for display
const uint I2C_SCL_PIN_B = 15;     // GPIO 15: I2C SCL line for display

// User interface buttons with pull-up configuration
#define BTN_A_PIN 5                // GPIO 5: Previous menu navigation
#define BTN_B_PIN 6                // GPIO 6: Next menu navigation  
#define BTN_C_PIN 22               // GPIO 22: WiFi reconnection trigger

/* ========== NETWORK CONFIGURATION ========== */

// WiFi access point credentials (modify for your network)
#define WIFI_SSID "JOAO_2.4G"      // Target WiFi network name (2.4GHz required)
#define WIFI_PASSWORD "30226280!"  // WiFi network password
#define TCP_PORT 4242              // Reserved TCP port for future expansions

/* ========== MQTT PUBLISHING INTERVALS ========== */

#define MQTT_PUBLISH_INTERVAL_MS 10000  // Sensor data publication frequency (10s)
#define MQTT_ALERT_INTERVAL_MS 30000    // Alert checking and publication frequency (30s)

/* ========== ENVIRONMENTAL THRESHOLDS ========== */

// Temperature monitoring range (Celsius)
#define TEMP_MIN 15.0f             // Minimum acceptable temperature threshold
#define TEMP_MAX 35.0f             // Maximum acceptable temperature threshold

// Humidity monitoring (Relative Humidity %)
#define HUMIDITY_MAX 80.0f         // Maximum acceptable humidity threshold

// Light intensity monitoring (Lux)
#define LUX_MIN 50.0f              // Minimum acceptable light intensity threshold

/* ========== DATA STRUCTURES ========== */

/**
 * @brief Menu system enumeration
 * Defines available display screens for user navigation
 */
typedef enum {
    MENU_MEASUREMENTS = 0,  // Real-time sensor readings display
    MENU_WIFI,             // Network connectivity status
    MENU_ALERTS,           // Critical value alerts summary  
    MENU_MQTT,             // MQTT broker connection status
    MENU_COUNT             // Total number of menus (for navigation bounds)
} MenuId;

/**
 * @brief Environmental sensor data container
 * Stores current readings and operational status of all sensors
 */
typedef struct {
    float temperature;      // Current temperature reading (°C)
    float humidity;        // Current relative humidity reading (%)
    float lux;             // Current light intensity reading (lux)
    bool aht_ok;           // AHT10 sensor communication status
    bool lux_ok;           // BH1750 sensor communication status
} SensorData;

/**
 * @brief WiFi network connection status
 * Maintains current network connectivity information
 */
typedef struct {
    char ssid[33];         // Connected network name (max 32 chars + null terminator)
    bool connected;        // Current connection state
    char ip_address[16];   // Assigned IP address in dotted decimal notation
} WifiStatus;

/**
 * @brief Environmental alert monitoring system
 * Tracks which sensors have exceeded their configured thresholds
 */
typedef struct {
    bool temp_critical;    // Temperature outside acceptable range
    bool humidity_critical; // Humidity above maximum threshold
    bool lux_critical;     // Light intensity below minimum threshold
    bool any_critical;     // Consolidated alert status (OR of all above)
} AlertStatus;

/**
 * @brief Complete application state container
 * Central data structure maintaining all system operational data
 */
typedef struct {
    MenuId current_menu;           // Currently displayed menu screen
    SensorData sensors;            // Latest environmental sensor readings
    WifiStatus wifi;               // Network connectivity information
    AlertStatus alerts;            // Environmental threshold monitoring
    uint32_t last_mqtt_publish;    // Timestamp of last MQTT data publication
    uint32_t last_mqtt_alert_check; // Timestamp of last alert verification
    absolute_time_t last_sensor_read;   // High-precision sensor reading timestamp
    absolute_time_t last_display_update; // High-precision display refresh timestamp
} AppState;

/**
 * @brief Button debouncing mechanism
 * Prevents false triggering from mechanical switch bounce
 */
typedef struct {
    uint pin;              // GPIO pin number for this button
    bool last_state;       // Previous button state for edge detection
} DebounceButton;

/* ========== GLOBAL STATE VARIABLES ========== */

// Primary application state - contains all system operational data
static AppState app_state;

// Button instances for user interface navigation
static DebounceButton btn_a, btn_b, btn_c;

/* ========== BUTTON INTERFACE FUNCTIONS ========== */

/**
 * @brief Initialize GPIO button with debouncing configuration
 * 
 * Configures a GPIO pin as an input with internal pull-up resistor
 * and initializes the debouncing mechanism for reliable button detection.
 * 
 * @param btn Pointer to button structure to initialize
 * @param pin GPIO pin number to configure for button input
 */
static void button_init(DebounceButton* btn, uint pin) {
    btn->pin = pin;                    // Store pin number for future reference
    gpio_init(pin);                    // Initialize GPIO pin for use
    gpio_pull_up(pin);                 // Enable internal pull-up resistor (button grounds when pressed)
    gpio_set_dir(pin, false);          // Configure as input (GPIO_IN = false)
    btn->last_state = gpio_get(pin);   // Initialize debouncing with current pin state
}

/**
 * @brief Detect button press with debouncing mechanism
 * 
 * Implements edge detection to identify button press events (high-to-low transition)
 * while filtering out mechanical bounce that could cause false triggers.
 * 
 * @param btn Pointer to button structure containing state information
 * @return true if button was pressed (falling edge detected), false otherwise
 */
static bool button_pressed(DebounceButton* btn) {
    bool current_state = gpio_get(btn->pin);                        // Read current GPIO state
    bool pressed = (btn->last_state == 1 && current_state == 0);    // Detect falling edge (press event)
    btn->last_state = current_state;                                // Update state for next comparison
    return pressed;
}

/* ========== NETWORK CONNECTIVITY FUNCTIONS ========== */

/**
 * @brief Establish WiFi connection and initialize MQTT client
 * 
 * Configures the CYW43 wireless chip for station mode, attempts connection
 * to the configured access point, retrieves network information, and 
 * initializes the MQTT communication subsystem.
 * 
 * @return true if WiFi connection successful and MQTT initialized, false on failure
 */
static bool wifi_connect(void) {
    // Configure WiFi chip for client (station) mode
    cyw43_arch_enable_sta_mode();
    
    printf("Conectando ao WiFi '%s'...\n", WIFI_SSID);
    
    // Attempt WiFi connection with 30-second timeout
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao WiFi\n");
        return false;
    }
    
    printf("WiFi conectado!\n");
    
    // Extract assigned IP address from network interface
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    snprintf(app_state.wifi.ip_address, sizeof(app_state.wifi.ip_address), 
             "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
    
    // Update application state with connection details
    strncpy(app_state.wifi.ssid, WIFI_SSID, sizeof(app_state.wifi.ssid) - 1);
    app_state.wifi.connected = true;
    
    // Initialize MQTT communication subsystem after successful WiFi connection
    mqtt_conect_init();
    printf("Cliente MQTT inicializado\n");
    
    return true;
}

/* ========== ENVIRONMENTAL MONITORING FUNCTIONS ========== */

/**
 * @brief Evaluate sensor readings against configured thresholds
 * 
 * Analyzes current environmental sensor data to determine if any values
 * exceed acceptable operational limits. Updates alert flags and provides
 * visual indication via onboard LED when critical conditions are detected.
 */
static void check_critical_values(void) {
    AlertStatus* alerts = &app_state.alerts;      // Reference to alert status structure
    SensorData* sensors = &app_state.sensors;     // Reference to current sensor readings
    
    // Reset all alert flags for fresh evaluation
    alerts->temp_critical = false;
    alerts->humidity_critical = false;
    alerts->lux_critical = false;
    
    // Evaluate temperature and humidity if AHT10 sensor is operational
    if (sensors->aht_ok) {
        // Check temperature against acceptable range
        if (sensors->temperature < TEMP_MIN || sensors->temperature > TEMP_MAX) {
            alerts->temp_critical = true;
        }
        // Check humidity against maximum threshold
        if (sensors->humidity > HUMIDITY_MAX) {
            alerts->humidity_critical = true;
        }
    }
    
    // Evaluate light intensity if BH1750 sensor is operational
    if (sensors->lux_ok) {
        // Check light level against minimum threshold
        if (sensors->lux < LUX_MIN) {
            alerts->lux_critical = true;
        }
    }
    
    // Consolidate alert status - true if any individual alert is active
    alerts->any_critical = alerts->temp_critical || alerts->humidity_critical || alerts->lux_critical;
    
    // Provide visual indication of critical conditions via onboard LED
    if (alerts->any_critical) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);    // Turn on LED
        sleep_ms(100);                                     // Brief illumination period
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);    // Turn off LED
    }
}

/* ========== MQTT COMMUNICATION FUNCTIONS ========== */

/**
 * @brief Publish current sensor readings to MQTT broker
 * 
 * Formats environmental sensor data into JSON payload and publishes
 * to the designated MQTT topic for remote monitoring. Only executes
 * if WiFi connectivity is established.
 */
static void mqtt_publish_sensor_data_func(void) {
    // Verify WiFi connectivity before attempting MQTT publication
    if (!app_state.wifi.connected) {
        return;
    }
    
    SensorData* sensors = &app_state.sensors;    // Reference to current sensor data
    
    // Publish sensor data using high-level MQTT interface
    mqtt_get_and_publish(
        wifi_check(),           // Current WiFi connection status
        mqtt_check(),          // Current MQTT broker connection status
        sensors->aht_ok,       // AHT10 temperature/humidity sensor status
        false,                 // BMP280 sensor status (not present in this system)
        sensors->lux_ok,       // BH1750 light intensity sensor status
        sensors->temperature,  // Current temperature reading (°C)
        0.0f,                 // BMP280 temperature (unused - set to 0)
        sensors->humidity,     // Current humidity reading (%)
        0.0f,                 // Atmospheric pressure (unused - set to 0)
        sensors->lux          // Current light intensity reading (lux)
    );
    
    printf("Dados dos sensores publicados via MQTT\n");
}

/**
 * @brief Publish environmental alerts to MQTT broker
 * 
 * Generates and transmits alert notifications when sensor readings
 * exceed configured thresholds. Alert data is formatted as JSON
 * and published to dedicated alert topic.
 */
static void mqtt_publish_alerts_func(void) {
    // Verify WiFi connectivity before attempting MQTT publication
    if (!app_state.wifi.connected) {
        return;
    }
    
    AlertStatus* alerts = &app_state.alerts;
    
    if (alerts->any_critical) {
        char alert_json[256];
        snprintf(alert_json, sizeof(alert_json),
                "{\"alerta\":\"critico\", \"temperatura_critica\":%s, \"umidade_critica\":%s, \"luz_critica\":%s}",
                alerts->temp_critical ? "true" : "false",
                alerts->humidity_critical ? "true" : "false",
                alerts->lux_critical ? "true" : "false");
        
        mqtt_get_and_publish2(wifi_check(), mqtt_check(), alert_json);
        printf("Alertas críticos publicados via MQTT\n");
    }
}

// Função para ler todos os sensores
static void read_sensors(void) {
    SensorData* sensors = &app_state.sensors;
    
    sensors->aht_ok = aht10_read_data(&sensors->temperature, &sensors->humidity);
    sensors->lux_ok = bh1750_read_lux(&sensors->lux);
    
    if (sensors->aht_ok) {
        printf("Temperatura: %.2f°C | Umidade: %.2f%%\n", sensors->temperature, sensors->humidity);
    }
    
    if (sensors->lux_ok) {
        printf("Luminosidade: %.2f lux\n", sensors->lux);
    }
    
    check_critical_values();
}

// Função para enviar dados via TCP (simulando envio para celular)
static void send_data_to_phone(void) {
    if (!app_state.wifi.connected) return;
    
    SensorData* sensors = &app_state.sensors;
    AlertStatus* alerts = &app_state.alerts;
    
    // Criar JSON com os dados
    char json_data[512];
    snprintf(json_data, sizeof(json_data),
        "{"
        "\"temperatura\":%.2f,"
        "\"umidade\":%.2f,"
        "\"luminosidade\":%.2f,"
        "\"alertas\":{"
            "\"temperatura\":%s,"
            "\"umidade\":%s,"
            "\"luminosidade\":%s"
        "}"
        "}",
        sensors->aht_ok ? sensors->temperature : NAN,
        sensors->aht_ok ? sensors->humidity : NAN,
        sensors->lux_ok ? sensors->lux : NAN,
        alerts->temp_critical ? "true" : "false",
        alerts->humidity_critical ? "true" : "false",
        alerts->lux_critical ? "true" : "false"
    );
    
    printf("Dados JSON: %s\n", json_data);
    
    // Aqui você pode implementar um servidor TCP ou HTTP para enviar os dados
    // Por enquanto, apenas exibimos no console
}

// Função para renderizar diferentes telas no display
static void update_display(void) {
    switch (app_state.current_menu) {
        case MENU_MEASUREMENTS: {
            SensorData* sensors = &app_state.sensors;
            float temp = sensors->aht_ok ? sensors->temperature : NAN;
            float hum = sensors->aht_ok ? sensors->humidity : NAN;
            float lux = sensors->lux_ok ? sensors->lux : NAN;
            
            display_update(temp, hum, 0.0f, false, lux, sensors->lux_ok);
            break;
        }
        case MENU_WIFI: {
            WifiStatus* wifi = &app_state.wifi;
            display_render_wifi_status(wifi->ssid, wifi->connected, false);
            break;
        }
        case MENU_ALERTS: {
            AlertStatus* alerts = &app_state.alerts;
            // Aqui você pode criar uma função específica para mostrar alertas
            // Por enquanto, usamos a função de WiFi como placeholder
            display_render_wifi_status("ALERTAS", alerts->any_critical, false);
            break;
        }
        case MENU_MQTT: {
            bool mqtt_connected = mqtt_check();
            display_render_wifi_status("MQTT", mqtt_connected, false);
            break;
        }
        default:
            break;
    }
}

void setup_hardware() {
    stdio_init_all();
    sleep_ms(3000);
    printf("=== Sistema de Monitoramento Ambiental ===\n");
    printf("Hardware inicializado. Aguarde inicialização dos sensores.\n");

    // Inicializar WiFi
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar WiFi\n");
    }

    // Configuração I2C Port A para sensores
    i2c_init(I2C_PORT_A, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN_A, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN_A, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN_A);
    gpio_pull_up(I2C_SCL_PIN_A);

    // Configuração I2C Port B para display
    i2c_init(I2C_PORT_B, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN_B, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN_B, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN_B);
    gpio_pull_up(I2C_SCL_PIN_B);

    // Inicializar botões
    button_init(&btn_a, BTN_A_PIN);
    button_init(&btn_b, BTN_B_PIN);
    button_init(&btn_c, BTN_C_PIN);

    // Inicializar periféricos
    display_init(I2C_PORT_B, I2C_OLED_ADDR);
    aht10_init(I2C_PORT_A);
    bh1750_init(I2C_PORT_A);
    
    printf("Sensores inicializados:\n");
    printf("- AHT10 (Temperatura/Umidade)\n");
    printf("- BH1750 (Luminosidade)\n");
    printf("- Display OLED\n");

    // Inicializar estado da aplicação
    app_state.current_menu = MENU_MEASUREMENTS;
    app_state.wifi.connected = false;
    app_state.last_sensor_read = make_timeout_time_ms(0);
    app_state.last_display_update = make_timeout_time_ms(0);
    
    printf("\nIniciando sistema...\n");
}

int main() {
    setup_hardware();
    
    // Tentar conectar ao WiFi
    printf("Tentando conectar ao WiFi...\n");
    if (wifi_connect()) {
        printf("WiFi conectado com sucesso!\n");
        printf("IP: %s\n", app_state.wifi.ip_address);
    } else {
        printf("Continuando sem WiFi...\n");
    }
    
    // Timers para diferentes tarefas
    absolute_time_t sensor_timer = make_timeout_time_ms(2000);    // Ler sensores a cada 2s
    absolute_time_t display_timer = make_timeout_time_ms(200);    // Atualizar display a cada 200ms
    absolute_time_t wifi_timer = make_timeout_time_ms(5000);      // Enviar dados a cada 5s
    absolute_time_t mqtt_timer = make_timeout_time_ms(MQTT_PUBLISH_INTERVAL_MS);      // MQTT a cada 10s
    absolute_time_t mqtt_alert_timer = make_timeout_time_ms(MQTT_ALERT_INTERVAL_MS);  // Alertas MQTT a cada 30s
    
    // Inicializar timers MQTT
    app_state.last_mqtt_publish = 0;
    app_state.last_mqtt_alert_check = 0;
    
    printf("\n=== Sistema Iniciado ===\n");
    printf("Botões:\n");
    printf("- Botão A (GPIO %d): Menu Anterior\n", BTN_A_PIN);
    printf("- Botão B (GPIO %d): Próximo Menu\n", BTN_B_PIN);
    printf("- Botão C (GPIO %d): Reconectar WiFi\n", BTN_C_PIN);
    printf("\nMenus disponíveis:\n");
    printf("0: Medições dos Sensores\n");
    printf("1: Status WiFi\n");
    printf("2: Alertas Críticos\n");
    printf("3: Status MQTT\n");
    printf("========================\n\n");

    while (true) {
        // Processar botões
        if (button_pressed(&btn_a)) {
            app_state.current_menu = (MenuId)((app_state.current_menu + MENU_COUNT - 1) % MENU_COUNT);
            printf("Menu alterado para: %d\n", app_state.current_menu);
        }
        
        if (button_pressed(&btn_b)) {
            app_state.current_menu = (MenuId)((app_state.current_menu + 1) % MENU_COUNT);
            printf("Menu alterado para: %d\n", app_state.current_menu);
        }
        
        if (button_pressed(&btn_c)) {
            printf("Tentando reconectar WiFi...\n");
            if (wifi_connect()) {
                printf("WiFi reconectado!\n");
            }
        }
        
        // Ler sensores periodicamente
        if (absolute_time_diff_us(get_absolute_time(), sensor_timer) <= 0) {
            printf("\n--- Leitura dos Sensores ---\n");
            read_sensors();
            
            if (app_state.alerts.any_critical) {
                printf("⚠️  ALERTA CRÍTICO DETECTADO! ⚠️\n");
                if (app_state.alerts.temp_critical) {
                    printf("- Temperatura fora do limite (%.1f°C - %.1f°C)\n", TEMP_MIN, TEMP_MAX);
                }
                if (app_state.alerts.humidity_critical) {
                    printf("- Umidade muito alta (> %.1f%%)\n", HUMIDITY_MAX);
                }
                if (app_state.alerts.lux_critical) {
                    printf("- Luminosidade muito baixa (< %.1f lux)\n", LUX_MIN);
                }
            }
            
            sensor_timer = delayed_by_ms(sensor_timer, 2000);
        }
        
        // Atualizar display periodicamente
        if (absolute_time_diff_us(get_absolute_time(), display_timer) <= 0) {
            update_display();
            display_timer = delayed_by_ms(display_timer, 200);
        }
        
        // Enviar dados via WiFi periodicamente
        if (absolute_time_diff_us(get_absolute_time(), wifi_timer) <= 0) {
            if (app_state.wifi.connected) {
                printf("\n--- Enviando dados via WiFi ---\n");
                send_data_to_phone();
            }
            wifi_timer = delayed_by_ms(wifi_timer, 5000);
        }
        
        // Publicar dados dos sensores via MQTT periodicamente
        if (absolute_time_diff_us(get_absolute_time(), mqtt_timer) <= 0) {
            if (app_state.wifi.connected) {
                printf("\n--- Publicando dados via MQTT ---\n");
                mqtt_publish_sensor_data_func();
            }
            mqtt_timer = delayed_by_ms(mqtt_timer, MQTT_PUBLISH_INTERVAL_MS);
        }
        
        // Verificar e publicar alertas via MQTT
        if (absolute_time_diff_us(get_absolute_time(), mqtt_alert_timer) <= 0) {
            if (app_state.wifi.connected) {
                mqtt_publish_alerts_func();
            }
            mqtt_alert_timer = delayed_by_ms(mqtt_alert_timer, MQTT_ALERT_INTERVAL_MS);
        }
        
        // Permitir outras tarefas do sistema
        tight_loop_contents();
    }
    
    return 0;
}
