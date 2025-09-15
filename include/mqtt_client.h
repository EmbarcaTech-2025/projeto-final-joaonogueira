#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Sets up the MQTT client and connects to the broker
 * 
 * @param client_id Unique identifier for this client
 * @param broker_ip Broker IP address as string (e.g., "192.168.1.1")
 * @param user Username for authentication (can be NULL)
 * @param pass Password for authentication (can be NULL)
 */
void mqtt_setup(const char *client_id, const char *broker_ip, bool *status_mqtt);

/**
 * @brief Publishes data to an MQTT topic
 * 
 * @param topic Topic name (e.g., "sensor/temperature")
 * @param data Message payload (bytes)
 * @param len Payload length
 */
void mqtt_comm_publish(const char *topic, const uint8_t *data, size_t len);

#endif // MQTT_CLIENT_H