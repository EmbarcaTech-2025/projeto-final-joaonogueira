#include "lwip/apps/mqtt.h" // Biblioteca MQTT do lwIP
#include "mqtt_client.h" // Header file com as declarações locais
// Base: https://github.com/BitDogLab/BitDogLab-C/blob/main/wifi_button_and_led/lwipopts.h
#include "lwipopts.h" // Configurações customizadas do lwIP
#include <stdio.h>
#include <string.h>

/* Variável global estática para armazenar a instância do cliente MQTT
* 'static' limita o escopo deste arquivo */
static mqtt_client_t *client;
extern bool conct_status_mqtt;

/* Callback de conexão MQTT - chamado quando o status da conexão muda
* Parâmetros:
* - client: instância do cliente MQTT
* - arg: argumento opcional (não usado aqui)
* - status: resultado da tentativa de conexão */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Conectado ao broker MQTT com sucesso!\n");
        conct_status_mqtt=true;
    } else {
        printf("Falha ao conectar ao broker, código: %d\n", status);
        conct_status_mqtt=false;
    }
}
/* Função para configurar e iniciar a conexão MQTT
* Parâmetros:
* - client_id: identificador único para este cliente
* - broker_ip: endereço IP do broker como string (ex: "192.168.1.1") ou hostname (ex: "broker.hivemq.com")
* - status_mqtt: ponteiro para variável de status */
void mqtt_setup(const char *client_id, const char *broker_ip, bool *status_mqtt) {
    ip_addr_t broker_addr;
    
    // Lista de IPs de brokers públicos conhecidos
    const char* public_brokers[] = {
        "18.194.106.115",  // broker.emqx.io
        "18.185.216.207",  // broker.hivemq.com alternativo
        "test.mosquitto.org",
        NULL
    };
    
    // Se for um hostname conhecido, usar IP direto
    if (strcmp(broker_ip, "broker.emqx.io") == 0) {
        broker_ip = "18.194.106.115";
    } else if (strcmp(broker_ip, "broker.hivemq.com") == 0) {
        broker_ip = "18.185.216.207";
    }
    
    // Converte o IP de string para formato numérico
    if (!ip4addr_aton(broker_ip, &broker_addr)) {
        printf("Erro ao converter IP do broker: %s\n", broker_ip);
        *status_mqtt = false;
        return;
    }
    
    // Cria uma nova instância do cliente MQTT
    client = mqtt_client_new();
    if (client == NULL) {
        printf("Falha ao criar o cliente MQTT\n");
        *status_mqtt = false;
        return;
    }
    
    printf("Conectando ao broker MQTT: %s\n", broker_ip);
    
    // Configura as informações de conexão do cliente
    struct mqtt_connect_client_info_t ci = {
        .client_id = client_id
    };
    
    // Inicia a conexão com o broker
    int8_t err = mqtt_client_connect(client, &broker_addr, 1883, mqtt_connection_cb, NULL, &ci);
    if(err == ERR_OK){
        printf("Tentativa de conexão MQTT iniciada com sucesso\n");
        *status_mqtt = true;
    } else {
        printf("Erro ao iniciar conexão MQTT: %d\n", err);
        *status_mqtt = false;
    }
}
/* Callback de confirmação de publicação
* Chamado quando o broker confirma recebimento da mensagem (para QoS > 0)
* Parâmetros:
* - arg: argumento opcional
* - result: código de resultado da operação */
static void mqtt_pub_request_cb(void *arg, err_t result) {
    if (result == ERR_OK) {
        printf("Publicação MQTT enviada com sucesso!\n");
    } else {
        printf("Erro ao publicar via MQTT: %d\n", result);
    }
}
/* Função para publicar dados em um tópico MQTT
* Parâmetros:
* - topic: nome do tópico (ex: "sensor/temperatura")
* - data: payload da mensagem (bytes)
* - len: tamanho do payload */
void mqtt_comm_publish(const char *topic, const uint8_t *data, size_t len) {
    // Envia a mensagem MQTT
    err_t status = mqtt_publish(
    client, // Instância do cliente
    topic, // Tópico de publicação
    data, // Dados a serem enviados
    len, // Tamanho dos dados
    0, // QoS 0 (nenhuma confirmação)
    0, // Não reter mensagem
    mqtt_pub_request_cb, // Callback de confirmação
    NULL // Argumento para o callback
);
    if (status != ERR_OK) {
        printf("mqtt_publish falhou ao ser enviada: %d\n", status);
    }
}