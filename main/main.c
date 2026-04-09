#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pigpio.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// Definições de pinos (BCM GPIO numbers)
#define STEP_PIN 13         // STEP - Motor (Passo)
#define DIR_PIN 4           // DIRECTION
#define ENABLE_PIN 22       // ENABLE
#define CURSO_FINAL_PIN 16  // Final de curso
#define BUTTON_PIN 21       // Botão

// Frequência desejada para o motor: 21.135kHz (Driver - 1:32)
// Velocidade desejada: 3mm/s
#define FREQUENCIA_HZ 21135
#define DUTY_CICLE 500000       // 50%

// Endereço I2C do display OLED (SSD1306)
#define OLED_ADDR 0x3C

// ===========================================================
// Config. Pinos

int gpio_pin_config(){
    if (gpioInitialise() < 0) {
        fprintf(stderr, "  [OLED] Erro: GPIO não foi inicializado.\n");
        return 1;
    }

    gpioSetMode(STEP_PIN, PI_OUTPUT);
    gpioSetMode(DIR_PIN, PI_OUTPUT);
    gpioSetMode(ENABLE_PIN, PI_OUTPUT);
    gpioSetMode(CURSO_FINAL_PIN, PI_INPUT);
    gpioSetPullUpDown(CURSO_FINAL_PIN, PI_PUD_UP);
    gpioSetMode(BUTTON_PIN, PI_INPUT);
    gpioSetPullUpDown(BUTTON_PIN, PI_PUD_UP);

    gpioWrite(DIR_PIN, 0);       // Direção Padrão (subida)
    gpioWrite(ENABLE_PIN, 0);    // Enable no Driver
    gpioSetPWMrange(STEP_PIN, 1000);
    return 0;
}

// Configuração da comunicação UART
int configurar_uart(int fd, speed_t baudrate)
{
    struct termios tty;

    memset(&tty, 0, sizeof tty);

    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }

    // Baud rate
    if (cfsetispeed(&tty, baudrate) != 0) {
        perror("cfsetispeed");
        return -1;
    }

    if (cfsetospeed(&tty, baudrate) != 0) {
        perror("cfsetospeed");
        return -1;
    }

    // 8 bits, sem paridade, 1 stop bit
    tty.c_cflag &= ~PARENB;      // sem paridade
    tty.c_cflag &= ~CSTOPB;      // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;          // 8 bits por byte

    // Habilita recepção e ignora modem control lines
    tty.c_cflag |= (CLOCAL | CREAD);

    // Modo raw
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(ICRNL | INLCR | IGNCR);

    // Leitura:
    // VMIN = 1 -> retorna quando chegar 1 byte
    // VTIME = 0 -> sem timeout
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

// ===========================================================
// OLED

// Rotina de Eespera
void rotina_espera_botao(int serial) {
    tcflush(serial, TCIFLUSH);

    char c;
    char init = 'i';
    while (1) {
        int n = read(serial, &c, 1);
        if (n > 0 && c == init) {
            printf("     Esperando Input...\n\n");
            fflush(stdout);
            break;
        }
    }
}

// ===========================================================
// Motor

// Rotina de Referenciamento (Procurando: Fim de curso)
int rotina_referenciamento() {
    gpioWrite(DIR_PIN, 0);  // Sobe até encontrar Fim de curso
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_HZ, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }

    while (gpioRead(CURSO_FINAL_PIN)) {
        usleep(100*1000);   // 100 ms
    }

    gpioHardwarePWM(STEP_PIN, 0, 0);
    sleep(1);

    return 0;
}

// Rotina de Descida (Descendo durante duração)
int rotina_descida() {
    gpioWrite(DIR_PIN, 1);  // Descendo durante duração
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_HZ, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }
    
    int duration = 240;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Descida.\n");
    sleep(1);

    return 0;
}

// Rotina de Subida (Subida durante duração)
// int rotina_subida() {
//     gpioWrite(DIR_PIN, 0);  // Subindo durante duração
//     int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_HZ, DUTY_CICLE);
//     if (pwm_exit < 0) {
// 	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
// 	    return 1;
//     }

//     int duration = 30;
//     printf("     Duração: %d segundos.\n", duration);
//     sleep(duration);
    
//     gpioHardwarePWM(STEP_PIN, 0, 0);
//     printf("     Finalizado: Subida.\n");
//     usleep(10*1000);

//     return 0;
// }

// ===========================================================
// Main

int main() {
    // Configuração dos Pins
    int cfg = gpio_pin_config();
    if (cfg == 1)
        return 1;

    // Abre leitura para Serial (OLED)
    int serial = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
    if (serial < 0) {
        perror("Erro ao abrir serial");
        return 1;
    }

    // Configuração OLED
    int cfg_oled = configurar_uart(serial, B9600);
    if (cfg_oled != 0) {
        close(serial);
        return 1;
    }

    while(1) {
        // Segura o codigo, esperando o botao (OLED)
        rotina_espera_botao(serial);

        // MOTOR
        rotina_referenciamento();   // Procura Fim de Curso
        rotina_descida();           // Comeca descida
        sleep(5);                   // Aguarda X segundos
        rotina_referenciamento();   // Começa subida até o final do curso.
    }

    gpioWrite(ENABLE_PIN, 1);   // Desabilita driver
    gpioTerminate();

    return 0;
}