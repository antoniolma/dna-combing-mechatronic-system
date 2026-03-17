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

// 5x8 bitmap font, ASCII 32 (space) to 126 (~)
static const unsigned char font5x8[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, // 32 space
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, // 33 !
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, // 34 "
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, // 35 #
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, // 36 $
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, // 37 %
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, // 38 &
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, // 39 '
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, // 40 (
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, // 41 )
    { 0x08, 0x2A, 0x1C, 0x2A, 0x08 }, // 42 *
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, // 43 +
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, // 44 ,
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, // 45 -
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, // 46 .
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, // 47 /
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, // 48 0
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, // 49 1
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, // 50 2
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, // 51 3
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, // 52 4
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, // 53 5
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, // 54 6
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, // 55 7
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, // 56 8
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, // 57 9
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, // 58 :
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, // 59 ;
    { 0x00, 0x08, 0x14, 0x22, 0x41 }, // 60 <
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, // 61 =
    { 0x41, 0x22, 0x14, 0x08, 0x00 }, // 62 >
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, // 63 ?
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, // 64 @
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, // 65 A
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, // 66 B
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, // 67 C
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, // 68 D
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, // 69 E
    { 0x7F, 0x09, 0x09, 0x01, 0x01 }, // 70 F
    { 0x3E, 0x41, 0x41, 0x51, 0x32 }, // 71 G
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, // 72 H
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, // 73 I
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, // 74 J
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, // 75 K
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, // 76 L
    { 0x7F, 0x02, 0x04, 0x02, 0x7F }, // 77 M
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, // 78 N
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, // 79 O
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, // 80 P
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, // 81 Q
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, // 82 R
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, // 83 S
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, // 84 T
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, // 85 U
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, // 86 V
    { 0x7F, 0x20, 0x18, 0x20, 0x7F }, // 87 W
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, // 88 X
    { 0x03, 0x04, 0x78, 0x04, 0x03 }, // 89 Y
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, // 90 Z
    { 0x00, 0x7F, 0x41, 0x41, 0x00 }, // 91 [
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, // 92 \
    { 0x00, 0x41, 0x41, 0x7F, 0x00 }, // 93 ]
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, // 94 ^
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, // 95 _
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, // 96 `
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, // 97 a
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, // 98 b
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, // 99 c
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, // 100 d
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, // 101 e
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, // 102 f
    { 0x08, 0x54, 0x54, 0x54, 0x3C }, // 103 g
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, // 104 h
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, // 105 i
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, // 106 j
    { 0x00, 0x7F, 0x10, 0x28, 0x44 }, // 107 k
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, // 108 l
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, // 109 m
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, // 110 n
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, // 111 o
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, // 112 p
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, // 113 q
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, // 114 r
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, // 115 s
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, // 116 t
    { 0x3C, 0x40, 0x40, 0x20, 0x7C }, // 117 u
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, // 118 v
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, // 119 w
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, // 120 x
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, // 121 y
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, // 122 z
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, // 123 {
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, // 124 |
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, // 125 }
    { 0x08, 0x08, 0x2A, 0x1C, 0x08 }, // 126 ~
};

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
void show_routines() {
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
    gpioSetMode(RELAY_PIN, PI_OUTPUT);
    gpioSetMode(CURSO_FINAL_PIN, PI_INPUT);
    gpioSetPullUpDown(CURSO_FINAL_PIN, PI_PUD_UP);
    gpioSetMode(BUTTON_PIN, PI_INPUT);
    gpioSetPullUpDown(BUTTON_PIN, PI_PUD_UP);

    gpioWrite(DIR_PIN, 0);       // Direção Padrão (subida)
    gpioWrite(ENABLE_PIN, 0);    // Enable no Driver
    gpioWrite(RELAY_PIN, 1);     // Relé inicialmente desligado
    gpioSetPWMrange(STEP_PIN, 1000);
    return 0;
}

// ===========================================================
// Botão

// Rotina de teste: Botão
void test_button() {
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

// Envia comando para OLED
void oled_send_cmd(int fd, unsigned char cmd) {
    unsigned char buf[2] = {0x00, cmd};
    write(fd, buf, 2);
}

// Preenche OLED com o valor especificado
// Ex.: 0x00 - Preto, 0xFF - Branco, ...
void oled_fill(int fd, unsigned char pattern) {
    oled_send_cmd(fd, 0x21); oled_send_cmd(fd, 0);   oled_send_cmd(fd, 127); // Coluna: 0-127
    oled_send_cmd(fd, 0x22); oled_send_cmd(fd, 0);   oled_send_cmd(fd, 7);   // Página: 0-7
    unsigned char buf[129];
    buf[0] = 0x40;
    memset(buf + 1, pattern, 128);
    for (int i = 0; i < 8; i++)
        write(fd, buf, 129);
}

// Escreve UM char na posição: linha X [0-7] e coluna Y [0-128].
void oled_write_char(int fd, int page, int col, char c) {
    if (c < 32 || c > 126) c = '?';
    oled_send_cmd(fd, 0x21); oled_send_cmd(fd, col); oled_send_cmd(fd, col + 5);
    oled_send_cmd(fd, 0x22); oled_send_cmd(fd, page); oled_send_cmd(fd, page);
    unsigned char buf[7];
    buf[0] = 0x40;
    for (int i = 0; i < 5; i++) buf[i + 1] = font5x8[(unsigned char)c - 32][i];
    buf[6] = 0x00;  // Espaçamento de 1 px
    write(fd, buf, 7);
}

// Escreve uma palavra, começando na página (linha) X e coluna Y.
// Cada char = 6px largura -> 21 chars/linha max.
void oled_write_string(int fd, int page, int col, const char *str) {
    while (*str && col + 6 <= 128) {
        oled_write_char(fd, page, col, *str++);
        col += 6;
    }
}

// Escreve em múltiplas linhas, uma por página (0-7 linhas)
void write_oled(int fd, const char *lines[], int count) {
    oled_fill(fd, 0x00);    // Limpa Tela
    for (int i = 0; i < count && i < 8; i++)
        oled_write_string(fd, i, 0, lines[i]);
}

// Inicializa Tela OLED
int init_oled(int fd) {
    if (fd < 0) {
        perror("  [OLED] Erro: Não foi possível abrir barramento I2C");
        gpioTerminate();
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, OLED_ADDR) < 0) {
        perror("  [OLED] Erro: Não foi possível selecionar dispositivo OLED");
        close(fd);
        gpioTerminate();
        return 1;
    }

    char init[] = {
        0x00, 0xAE, // Display off
        0x00, 0x20, 0x00, // Horizontal addressing
        0x00, 0xB0, // Set page start address
        0x00, 0x02, // Set low column address
        0x00, 0x10, // Set high column address
        0x00, 0xC8, // Set com scan inc
        0x00, 0xDA, 0x12, // Set COM pins
        0x00, 0x81, 0xCF, // Set contrast
        0x00, 0xA1, // Set segment re-map
        0x00, 0xA6, // Normal display
        0x00, 0xA8, 0x3F, // Multiplex ratio
        0x00, 0xD3, 0x00, // Display offset
        0x00, 0xD5, 0x80, // Display clock divide
        0x00, 0xD9, 0xF1, // Precharge
        0x00, 0xDB, 0x40, // VCOM detect
        0x00, 0x8D, 0x14, // Charge pump
        0x00, 0xAF  // Display on
    };

    write(fd, init, sizeof(init));
    usleep(1000);   // 1 ms

    oled_fill(fd, 0x00);    // Limpa tela
    return 0;
}

// Rotina: Xadrez
void oled_chessboard(int fd) {
    oled_fill(fd, 0x00);    // Limpa tela
    oled_send_cmd(fd, 0x21); oled_send_cmd(fd, 0);   oled_send_cmd(fd, 127);
    oled_send_cmd(fd, 0x22); oled_send_cmd(fd, 0);   oled_send_cmd(fd, 7);
    unsigned char buf[129];
    buf[0] = 0x40;
    for (int col = 0; col < 128; col++)
        buf[col + 1] = (col % 2 == 0) ? 0xAA : 0x55;
    for (int i = 0; i < 8; i++)
        write(fd, buf, 129);
}

// Rotina de teste: OLED
void test_OLED(int fd) {
    print_section("Inicializando teste: OLED");

    // Teste 1 - Full White
    printf("  Teste [ 1 ] - Full white\n");
    oled_fill(fd, 0xFF);    // Tudo Branco
    usleep(3*1000000);

    // Teste 2 - Full Black
    printf("  Teste [ 2 ] - Full Black\n");
    oled_fill(fd, 0x00);    // Tudo Preto
    usleep(3*1000000);

    // Teste 3 - Xadrez
    printf("  Teste [ 3 ] - Xadrez\n");
    oled_chessboard(fd);
    usleep(3*1000000);

    // Teste 4 - Hello
    printf("  Teste [ 4 ] - Hello, world!\n");
    const char *text[] = {"Hello,", "world!"};
    write_oled(fd, text, 2);
    usleep(3*1000000);

    // Limpa OLED...
    oled_fill(fd, 0x00);

    printf("\n");
    printf("  Finalizando Teste...\n\n");
    
}

// ===========================================================
// Motor

// Rotina de Referenciamento (Procurando: Fim de curso)
int reference_routine() {
    gpioWrite(DIR_PIN, 0);  // Sobe até encontrar Fim de curso
    // gpioSetPWMfrequency(STEP_PIN, FREQUENCIA_KHZ*1000);
    // gpioPWM(STEP_PIN, 500);
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
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

    // gpioPWM(STEP_PIN, 0);   // Parar o Motor
    gpioHardwarePWM(STEP_PIN, 0, 0);

    return 0;
}

// Rotina de Descida (Descendo durante duração)
int descending_routine() {
    gpioWrite(DIR_PIN, 1);  // Descendo durante duração
    // gpioSetPWMfrequency(STEP_PIN, FREQUENCIA_KHZ*1000);
    // gpioPWM(STEP_PIN, 500);
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }
    
    int duration = 5;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    // gpioPWM(STEP_PIN, 0);   // Parar o Motor
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Descida.\n");

    return 0;
}

// Rotina de Subida (Subida durante duração)
int ascending_routine() {
    gpioWrite(DIR_PIN, 0);  // Subindo durante duração
    // gpioSetPWMfrequency(STEP_PIN, FREQUENCIA_KHZ*1000);
    // gpioPWM(STEP_PIN, 500);
    int pwm_exit = gpioHardwarePWM(STEP_PIN, FREQUENCIA_KHZ*1000, DUTY_CICLE);
    if (pwm_exit < 0) {
	    fprintf(stderr, "  [Motor] Erro no gpioHardwarePWM: exit=%d\n", pwm_exit);
	    return 1;
    }

    int duration = 5;
    printf("     Duração: %d segundos.\n", duration);
    sleep(duration);
    
    // gpioPWM(STEP_PIN, 0);   // Parar o Motor
    gpioHardwarePWM(STEP_PIN, 0, 0);
    printf("     Finalizado: Subida.\n");

    return 0;
}

// Rotina de teste: Motor
void test_motor() {
    print_section("Inicializando teste: Motor");
    
    gpioWrite(RELAY_PIN, 0);    // Liga relé

    // Teste 1 - Rotina de Referenciamento
    // printf("  Teste [ 1 ] - Rotina de Referenciamento\n");
    // int ref_exit = reference_routine();
    // if (ref_exit == 1)
    //     return;

    // Teste 2 - Rotina de Descida
    printf("  Teste [ 2 ] - Rotina de Descida\n");
    int dsc_exit = descending_routine();
    if (dsc_exit == 1)
        return;
    sleep(2);

    // Teste 3 - Routina de Subida
    printf("  Teste [ 3 ] - Routina de Subida\n");
    int asc_exit = ascending_routine();
    if (asc_exit == 1)
        return;
    sleep(2);

    gpioWrite(RELAY_PIN, 1);    // Desliga Relé

    printf("\n");
    printf("  Finalizando Teste...\n\n");
}

// ===========================================================

int main() {
    // Configuração dos Pins
    int cfg = gpio_pin_config();
    if (cfg == 1)
        return 1;

    // Inicia OLED
    int oled_fd = open("/dev/i2c-1", O_RDWR);
    int oled_cfg = init_oled(oled_fd);
    if (oled_cfg == 1)
        return 1;

    // Seleção de Rotinas
    int id_teste;
    print_header("Rotinas de Teste");
    while (1) {
        show_routines();
        printf("  Selectione um teste: ");
        scanf("%d", &id_teste);

        switch (id_teste) {
            case 0:
                print_section("Finalizando...");
                gpioWrite(ENABLE_PIN, 1);   // Desabilita driver
                gpioWrite(RELAY_PIN, 1);    // Desabilita relé
                close(oled_fd);
                gpioTerminate();
                return 0;
            case 1:
                // Botão
                test_button();
                break;
            case 2:
                // OLED
                test_OLED(oled_fd);
                break;
            case 3:
                // Motor
                test_motor();
                break;
            default:
                printf("\n  [!] ID inválido. Tente novamente.\n\n");
        }
    }

    return 0;
}
