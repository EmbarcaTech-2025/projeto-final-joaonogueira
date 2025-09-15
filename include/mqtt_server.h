#ifndef MQTT_MANAGER
#define MQTT_MANAGER

#include <math.h>
#include <stdbool.h>

// #define SSID "JOAO_2.4G"
// #define PASSWD "30226280!"

#define SSID "JOAO_2.4G"
#define PASSWD "30226280!"

void mqtt_conect_init();

void mqtt_get_and_publish(bool wifi_connected,bool mqtt_connected, bool aht_ok,bool bmp_ok,bool lux_ok,
    float aht_temp, float bmp_temp,float humidity,float pressure,float lux_val);

void mqtt_get_and_publish2(bool wifi_connected,bool mqtt_connected,char *str);

bool wifi_check();
bool mqtt_check();


#endif