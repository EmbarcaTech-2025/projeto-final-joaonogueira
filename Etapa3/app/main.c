#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"

#include "aht10.h"
#include "bh1750.h"


#define I2C_PORT_A i2c0
const uint I2C_SDA_PIN_A = 0;
const uint I2C_SCL_PIN_A = 1;

#define I2C_PORT_B i2c1
const uint I2C_SDA_PIN_B = 2;
const uint I2C_SCL_PIN_B = 3;

void hardware() {

    stdio_init_all();
    sleep_ms(3000);
    printf("Hardware inicializado. Aguarde inicialização dos sensores.\n");

    i2c_init(I2C_PORT_A, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN_A, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN_A, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN_A);
    gpio_pull_up(I2C_SCL_PIN_A);

    i2c_init(I2C_PORT_B, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN_B, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN_B, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN_B);
    gpio_pull_up(I2C_SCL_PIN_B);

    bh1750_init(I2C_PORT_A);
    printf("Sensor BH1750 inicializado e pronto para aferição.\n");
    
    aht10_init(I2C_PORT_B);
    printf("Sensor AHT10 inicializado e pronto para aferição.\n");

    printf("\nIniciando aferição...\n");
}

int main() {
    hardware();
    while (1) {
        float temperature, humidity, lux;

        if (aht10_read_data(&temperature, &humidity)) {
            printf("AHT10 -> Temperatura: %.2f ºC | Umidade: %.2f %%RH\n", temperature, humidity);
        } else {
            printf("Erro ao ler dados do sensor AHT10.\n");
        }

        if (bh1750_read_lux(&lux)) {
            printf("BH1750 -> Luminosidade: %.2f lux\n", lux);
        } else {
            printf("Erro ao ler dados do sensor BH1750.\n");
        }

        printf("-----------------------------\n");
        sleep_ms(1000);
    }
}
