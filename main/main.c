#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pigpio.h>
#include <string.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// Definições de pinos (BCM GPIO numbers)
// #define STEP_PIN 27      // STEP
#define DIR_PIN 4        // DIRECTION
#define ENABLE_PIN 22    // ENABLE
#define CURSO_FINAL_PIN 24  // Final de curso
#define RELAY_PIN 6      // Relé
#define BUTTON_PIN 21    // Botão
#define STEP_PIN 13

// Frequência desejada para o motor: 12kHz (Driver - 1:16)
// Iteração 1: Passar frequência para 24kHz (Driver - 1:32)
#define FREQUENCIA_KHZ 24
#define DUTY_CICLE 500000       // 50%

// Endereço I2C do display OLED (SSD1306)
#define OLED_ADDR 0x3C

// ===========================================================
// Motor

// Rotina de Referenciamento (Procurando: Fim de curso)
int rotina_referenciamento() {
    gpioWrite(DIR_PIN, 0);  // Sobe até encontrar Fim de curso
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }

    int seconds = 30;
    while (gpioRead(CURSO_FINAL_PIN)) {
        usleep(100*1000);   // 100 ms
    }

    gpioHardwarePWM(STEP_PIN, 0, 0);

    return 0;
}

// Rotina de Descida (Descendo durante duração)
int rotina_descida() {
    gpioWrite(DIR_PIN, 1);  // Descendo durante duração
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }
    
    int duration = 30;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Descida.\n");

    return 0;
}

// Rotina de Subida (Subida durante duração)
int ascending_routine() {
    gpioWrite(DIR_PIN, 0);  // Subindo durante duração
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }

    int duration = 30;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Subida.\n");

    return 0;
}

// ===========================================================
// Main

int main() {
    // Configuração dos Pins
    int cfg = gpio_pin_config();
    if (cfg == 1)
        return 1;

    while(1) {
        // Segura esperando o botao
        while (gpioRead(BUTTON_PIN)) {
            sleep(100*1000);    // 100 ms
        }

        // Procura Fim de Curso
        rotina_referenciamento();

        // Começa descida (Provisorio: X segundos)
        rotina_descida();
    }

    gpioWrite(ENABLE_PIN, 1);   // Desabilita driver
    gpioTerminate();

    return 0
}