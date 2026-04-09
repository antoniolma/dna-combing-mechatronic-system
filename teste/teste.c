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
#define CURSO_FINAL_PIN 24  // Final de curso
#define BUTTON_PIN 21       // Botão
#define RX_PIN 15           // OLED (Recebe Bytes de controle)

// Frequência desejada para o motor: 12kHz (Driver - 1:16)
// Iteração 1: Passar frequência para 24kHz (Driver - 1:32)
#define FREQUENCIA_HZ 21135
#define DUTY_CICLE 500000       // 50%

// Endereço I2C do display OLED (SSD1306)
#define OLED_ADDR 0x3C

// ===========================================================
// Auxiliares para print
void print_header(const char *title) {
    printf("\n");
    printf("  +-----------------------------------+\n");
    printf("  |  %-32s|\n", title);
    printf("  +-----------------------------------+\n");
    printf("\n");
}

void print_section(const char *label) {
    printf("\n  >> %s\n\n", label);
}

// Mostra opções de rotinas de teste
void mostra_rotinas() {
    printf("  [ 1 ]  Botão\n");
    printf("  [ 2 ]  OLED\n");
    printf("  [ 3 ]  Motor\n");
    printf("  [ 0 ]  Cancelar\n");
    printf("\n");
}

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
// Botão

// Rotina de teste: Botão
void teste_botao() {
    print_section("Inicializando Teste: Botão");
    
    printf("  Esperando botão ser pressionado...\n\n");
    int count = 0;
    bool wasPressed = true;     // Flag para capturar erro
    int seconds = 10;           // Espere por X
    while (gpioRead(BUTTON_PIN)) {
        usleep(100*1000);  // 100ms
        count++;

        if (count >= seconds*10) {  // X * (1000 ms) = X segundos
            wasPressed = false;
            break;
        }
    }

    if (!wasPressed) {
        printf("  [Botão] Esperei %d segundos, NENHUM sinal foi recebido. Tente novamente.\n\n", seconds);
        return;
    }

    printf("  Botão foi pressionado!\n");
    printf("  Finalizando Teste...\n\n");
}

// ===========================================================
// OLED

void teste_OLED(int serial)
{
    char i = 'i';   // Iniciar
    char p = 'p';   // Parar

    print_section("Inicializando teste: Motor");
    sleep(1);
    printf("  Aguardando receber input...\n\n");

    // Limpa arquivo que le os inputs
    // Isso garante que so pego o input mais novo
    tcflush(serial, TCIFLUSH);

    char c;
    int seconds = 30;  // Segundos
    int count = 0;
    while (1) {
        int n = read(serial, &c, 1);
        if (n > 0) {
            printf("    Char. Recebido: %c\n", c);

            // Printa tipo de comando recebido
            if (c == i)
                printf("    Comando: INICIAR\n\n");
            else if (c == p)
                printf("    Comando: PARAR\n\n");
            else {
                printf("    Comando: Desconhecido\n");
                printf("    Atenção: Verifique a conexão e tente novamente...\n\n");
            }

            fflush(stdout);
            break;
        } else if (n < 0) {
            perror("Erro na leitura da serial");
            break;
        }
    }
    printf("  Finalizando Teste...\n\n");
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

    int count = 0;
    int seconds = 30;
    while (gpioRead(CURSO_FINAL_PIN)) {
        usleep(100*1000);   // 100 ms
        count++;

        if (count >= seconds*10) {  // 30 * (1000 ms) = 30 segundos
            gpioPWM(STEP_PIN, 0);   // Parar o Motor
            printf("  [Motor] Erro: Fim de curso não encontrado (Rotina de Referenciamento).\n\n");
            printf("  Finalizando Teste...\n\n");
            sleep(1);
            return 1;
        }
    }

    gpioHardwarePWM(STEP_PIN, 0, 0);

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
    
    int duration = 20;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Descida.\n");

    return 0;
}

// Rotina de Subida (Subida durante duração)
int rotina_subida() {
    gpioWrite(DIR_PIN, 0);  // Subindo durante duração
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_HZ, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }

    int duration = 15;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Subida.\n");

    return 0;
}

// Rotina de teste: Motor
void teste_motor() {
    print_section("Inicializando teste: Motor");

    // Teste 1 - Rotina de Referenciamento
    printf("  Teste [ 1 ] - Rotina de Referenciamento\n");
    int ref_exit = rotina_referenciamento();
    if (ref_exit == 1)
        return;

    // Teste 2 - Rotina de Descida
    printf("  Teste [ 2 ] - Rotina de Descida\n");
    int dsc_exit = rotina_descida();
    if (dsc_exit == 1)
        return;
    sleep(2);

    // Teste 3 - Routina de Subida
    printf("  Teste [ 3 ] - Routina de Subida\n");
    int asc_exit = rotina_subida();
    if (asc_exit == 1)
        return;
    sleep(2);

    printf("\n");
    printf("  Finalizando Teste...\n\n");
}

// ===========================================================

int main() {
    // Configuração dos Pins
    int cfg_pin = gpio_pin_config();
    if (cfg_pin == 1)
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

    // Seleção de Rotinas
    int id_teste;
    print_header("Rotinas de Teste");
    while (1) {
        mostra_rotinas();
        printf("  Selectione um teste: ");
        scanf("%d", &id_teste);

        switch (id_teste) {
            case 0:
                print_section("Finalizando...");
                gpioWrite(ENABLE_PIN, 1);   // Desabilita driver
                close(serial);
                gpioTerminate();
                return 0;
            case 1:
                // Botão
                teste_botao();
                break;
            case 2:
                // OLED
                teste_OLED(serial);
                break;
            case 3:
                // Motor
                teste_motor();
                break;
            default:
                printf("\n  [!] ID inválido. Tente novamente.\n\n");
        }
    }

    return 0;
}
