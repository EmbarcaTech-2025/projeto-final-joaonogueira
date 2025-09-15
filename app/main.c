#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"

#include "aht10.h"
#include "bh1750.h"
#include "display.h"
#include "mqtt_server.h"

//Sensores
#define I2C_PORT_A i2c0
const uint I2C_SDA_PIN_A = 0;
const uint I2C_SCL_PIN_A = 1;

//Display OLED
#define I2C_PORT_B i2c1
const uint8_t I2C_OLED_ADDR = 0x3C;
const uint I2C_SDA_PIN_B = 14;
const uint I2C_SCL_PIN_B = 15;

// Botões de navegação
#define BTN_A_PIN 5
#define BTN_B_PIN 6
#define BTN_C_PIN 22

// Configurações WiFi
#define WIFI_SSID "JOAO_2.4G"
#define WIFI_PASSWORD "30226280!"
#define TCP_PORT 4242

// Configurações MQTT
#define MQTT_PUBLISH_INTERVAL_MS 10000  // Publica a cada 10 segundos
#define MQTT_ALERT_INTERVAL_MS 30000    // Verifica alertas a cada 30 segundos

// Limites críticos dos sensores
#define TEMP_MIN 15.0f
#define TEMP_MAX 35.0f
#define HUMIDITY_MAX 80.0f
#define LUX_MIN 50.0f

// Enumeração dos menus
typedef enum {
    MENU_MEASUREMENTS = 0,
    MENU_WIFI,
    MENU_ALERTS,
    MENU_MQTT,
    MENU_COUNT
} MenuId;

// Estrutura dos dados dos sensores
typedef struct {
    float temperature, humidity;
    float lux;
    bool aht_ok, lux_ok;
} SensorData;

// Estrutura do status WiFi
typedef struct {
    char ssid[33];
    bool connected;
    char ip_address[16];
} WifiStatus;

// Estrutura de alertas
typedef struct {
    bool temp_critical;
    bool humidity_critical;
    bool lux_critical;
    bool any_critical;
} AlertStatus;

// Estado geral da aplicação
typedef struct {
    MenuId current_menu;
    SensorData sensors;
    WifiStatus wifi;
    AlertStatus alerts;
    uint32_t last_mqtt_publish;
    uint32_t last_mqtt_alert_check;
    absolute_time_t last_sensor_read;
    absolute_time_t last_display_update;
} AppState;

// Estrutura para debouncing dos botões
typedef struct {
    uint pin;
    bool last_state;
} DebounceButton;

static AppState app_state;
static DebounceButton btn_a, btn_b, btn_c;

// Função para inicializar botão com debounce
static void button_init(DebounceButton* btn, uint pin) {
    btn->pin = pin;
    gpio_init(pin);
    gpio_pull_up(pin);
    gpio_set_dir(pin, false);
    btn->last_state = gpio_get(pin);
}

// Função para detectar pressionamento do botão (com debounce)
static bool button_pressed(DebounceButton* btn) {
    bool current_state = gpio_get(btn->pin);
    bool pressed = (btn->last_state == 1 && current_state == 0);
    btn->last_state = current_state;
    return pressed;
}

// Função para conectar ao WiFi
static bool wifi_connect(void) {
    cyw43_arch_enable_sta_mode();
    
    printf("Conectando ao WiFi '%s'...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao WiFi\n");
        return false;
    }
    
    printf("WiFi conectado!\n");
    
    // Obter endereço IP
    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    snprintf(app_state.wifi.ip_address, sizeof(app_state.wifi.ip_address), 
             "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
    
    strncpy(app_state.wifi.ssid, WIFI_SSID, sizeof(app_state.wifi.ssid) - 1);
    app_state.wifi.connected = true;
    
    // Inicializar cliente MQTT após conectar WiFi
    mqtt_conect_init();
    printf("Cliente MQTT inicializado\n");
    
    return true;
}

// Função para verificar valores críticos
static void check_critical_values(void) {
    AlertStatus* alerts = &app_state.alerts;
    SensorData* sensors = &app_state.sensors;
    
    // Resetar alertas
    alerts->temp_critical = false;
    alerts->humidity_critical = false;
    alerts->lux_critical = false;
    
    if (sensors->aht_ok) {
        if (sensors->temperature < TEMP_MIN || sensors->temperature > TEMP_MAX) {
            alerts->temp_critical = true;
        }
        if (sensors->humidity > HUMIDITY_MAX) {
            alerts->humidity_critical = true;
        }
    }
    
    if (sensors->lux_ok) {
        if (sensors->lux < LUX_MIN) {
            alerts->lux_critical = true;
        }
    }
    
    alerts->any_critical = alerts->temp_critical || alerts->humidity_critical || alerts->lux_critical;
    
    // Piscar LED onboard se houver alertas
    if (alerts->any_critical) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    }
}

// Função para publicar dados via MQTT
static void mqtt_publish_sensor_data_func(void) {
    if (!app_state.wifi.connected) {
        return;
    }
    
    SensorData* sensors = &app_state.sensors;
    
    // Usar a função mqtt_get_and_publish com os dados dos sensores
    mqtt_get_and_publish(
        wifi_check(),           // status WiFi
        mqtt_check(),          // status MQTT
        sensors->aht_ok,       // AHT10 OK
        false,                 // BMP OK (não temos BMP280)
        sensors->lux_ok,       // LUX OK
        sensors->temperature,  // temperatura AHT
        0.0f,                 // temperatura BMP (não usado)
        sensors->humidity,     // umidade
        0.0f,                 // pressão (não usado)
        sensors->lux          // luminosidade
    );
    
    printf("Dados dos sensores publicados via MQTT\n");
}

// Função para publicar alertas via MQTT
static void mqtt_publish_alerts_func(void) {
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
