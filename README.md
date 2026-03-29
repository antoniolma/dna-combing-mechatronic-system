# Máquina Mecatronica para DNA Combing

Projeto de controle mecatrônico, realizado em parceria com o **Hospital A. C. Camargo**, para automação de rotinas de movimento (referenciamento/subida/descida) com Raspberry Pi, driver de motor de passo, botão, fim de curso e OLED.


## Estrutura

- `main/main.c`: rotina principal de operação do sistema.
- `teste/teste.c`: rotinas de teste para botão, motor e display OLED.

## Requisitos

- Raspberry Pi (GPIO)
- Biblioteca `pigpio`
- Linux com GCC (integrado na Raspberry)

## Compilação

```bash
# Compilar rotina principal:
gcc main/main.c -o main/app -lpigpio -lrt -lpthread

# Compilar rotina de testes:
gcc teste/teste.c -o teste/app -lpigpio -lrt -lpthread
```


## Execução

Inicie o daemon do pigpio:

```bash
sudo pigpiod
```

Execute o programa desejado:

```bash
sudo ./main/app
# ou
sudo ./teste/app
```


## Observações

- Os pinos GPIO estão definidos diretamente no código.
- Ajuste frequências, tempos e pinos conforme o hardware montado.