#include "mqtt_server.h"
#include "mqtt_client.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

bool conct_status_mqtt=false;
// char nome_rede_ssid[64]={};


void mqtt_conect_init(){
    // const char *ssid = "CLARO_5G5D1167";
    // const char *password = "385D1167";
    
    // Não precisa conectar WiFi aqui, já está conectado no main
    // connect_to_wifi(SSID, PASSWD);
    // sprintf(nome_rede_ssid,SSID);

    // Usando test.mosquitto.org (broker público confiável)
    mqtt_setup("pico_w_sensor", "91.121.93.94",&conct_status_mqtt);
    // mqtt_setup("pico_w_sensor", "broker.emqx.io",&conct_status_mqtt);
    // mqtt_setup("pico_w_sensor", "broker.hivemq.com",&conct_status_mqtt);
    // mqtt_setup("bitdog1", "192.168.1.76",&conct_status_mqtt);
    // mqtt_setup("bitdog1", "192.168.0.165");
}

void mqtt_get_and_publish(bool wifi_connected,bool mqtt_connected, bool aht_ok,bool bmp_ok,bool lux_ok,
    float aht_temp, float bmp_temp,float humidity,float pressure,float lux_val){
    // 2. Prepara os dados para o JSON (lógica similar à do seu display)
    float temp = aht_ok ? aht_temp : (bmp_ok ? bmp_temp : NAN);
    float hum  = aht_ok ? humidity : NAN;
    float pres = bmp_ok ? pressure : NAN;
    float lux  = lux_ok ? lux_val : NAN;

    // 3. Cria a string JSON
    char json_payload[256];
    sprintf(json_payload,
            "{\"temperatura\":%.2f, \"umidade\":%.2f, \"pressao\":%.2f, \"luminosidade\":%.1f}",
            temp,
            hum,
            pres / 100.0f, // Converte de Pa para hPa (opcional, mas comum)
            lux);

    // 4. Publica a mensagem via MQTT
    //    Verifica se o Wi-Fi e o MQTT estão conectados antes de enviar
    if (wifi_connected && mqtt_connected) {
        mqtt_comm_publish("pico_w/sensors/data", (const uint8_t *)json_payload, strlen(json_payload));
    }
}

void mqtt_get_and_publish2(bool wifi_connected,bool mqtt_connected,char *str){

    // 3. Cria a string JSON
    char json_payload[256];

    // 4. Publica a mensagem via MQTT
    //    Verifica se o Wi-Fi e o MQTT estão conectados antes de enviar
    if (wifi_connected && mqtt_connected) {
        mqtt_comm_publish("pico_w/sensors/alerts", (const uint8_t *)str, strlen(str));
    }
}

bool wifi_check(){
    // Verifica o status da conexão WiFi
    int8_t status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    // Retorna true se estiver conectado
    return (status == CYW43_LINK_JOIN || status == CYW43_LINK_UP);
}

bool mqtt_check(){
    return conct_status_mqtt;
}