#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"

// Definição dos pinos e variáveis globais
#define LED_COUNT 25      // Número de LEDs na matriz
#define LED_PIN 7         // GPIO do LED WS2812
#define LED_BLUE 11       // GPIO do canal azul do LED RGB
#define LED_GREEN 12      // GPIO do canal verde do LED RGB
#define LED_RED 13        // GPIO do canal vermelho do LED RGB
#define BUTTON_A 5        // GPIO do botão A
#define BUTTON_B 6        // GPIO do botão B

volatile int numero = 0;  // Variável que armazena o número atual exibido na matriz
volatile bool atualizar_display = true;  // Flag para atualização do display

// Estrutura para armazenar a cor de um pixel (LED da matriz WS2812)
struct pixel_t { uint32_t G, R, B; };
typedef struct pixel_t pixel_t;
pixel_t leds[LED_COUNT];
PIO np_pio;
uint sm;

// Inicializa o controlador PIO para os LEDs WS2812
void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);
}

// Define a cor de um LED específico na matriz
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}

// Apaga todos os LEDs da matriz
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i) npSetLED(i, 0, 0, 0);
}

// Atualiza a matriz de LEDs enviando os dados ao hardware
void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G << 24);
        pio_sm_put_blocking(np_pio, sm, leds[i].R << 24);
        pio_sm_put_blocking(np_pio, sm, leds[i].B << 24);
    }
}

// Função que mapeia os LEDs em zigue-zague, de acordo com a BitDogLab
int zigzag_map(int row, int col) {
    if (row % 2 == 0) {
        return (row * 5) + col;  // Linhas pares (esquerda para direita)
    } else {
        return (row * 5) + (4 - col);  // Linhas ímpares (direita para esquerda)
    }
}

// Exibe um número de 0 a 9 na matriz de LEDs
void exibir_numero(int num) {
    npClear();

    const uint8_t numeros[10][5][5] = {  
        { // Número 0
            {0,1,1,1,0},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,0,0,0,1},
            {0,1,1,1,0}
        },
        { // Número 1
            {0,0,1,0,0},
            {0,1,1,0,0},
            {1,0,1,0,0},
            {0,0,1,0,0},
            {1,1,1,1,1}
        },
        { // Número 2
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1}
        },
        { // Número 3
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 4
            {1,0,0,0,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,0,0,0,1}
        },
        { // Número 5
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 6
            {1,1,1,1,1},
            {1,0,0,0,0},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 7
            {1,1,1,1,1},
            {0,0,0,0,1},
            {0,0,1,1,0},
            {0,0,1,0,0},
            {0,0,1,0,0}
        },
        { // Número 8
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1}
        },
        { // Número 9
            {1,1,1,1,1},
            {1,0,0,0,1},
            {1,1,1,1,1},
            {0,0,0,0,1},
            {1,1,1,1,1}
        }
    };

    for (int row = 4; row >= 0; row--) {  
        for (int col = 4; col >= 0; col--) {  
            int led_index = zigzag_map(4 - row, 4 - col);  
            if (numeros[num][row][col]) {
                npSetLED(led_index, 0, 0, 255);  
            }
        }
    }

    npWrite();
}

// Função para piscar o LED vermelho do LED RGB 5x por segundo
void piscar_led_rgb() {
    static bool estado = false;
    estado = !estado;
    gpio_put(LED_RED, estado);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);
}

// Função de debounce para os botões
bool debounce(uint gpio) {
    static uint32_t ultimo_tempo_A = 0;
    static uint32_t ultimo_tempo_B = 0;
    
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (gpio == BUTTON_A) {
        if (tempo_atual - ultimo_tempo_A > 200) {
            ultimo_tempo_A = tempo_atual;
            return true;
        }
    } else if (gpio == BUTTON_B) {
        if (tempo_atual - ultimo_tempo_B > 200) {
            ultimo_tempo_B = tempo_atual;
            return true;
        }
    }

    return false;
}

// Interrupções dos botões A e B para incrementar ou decrementar o número
void isr_botoes(uint gpio, uint32_t events) {
    if (debounce(gpio)) {
        if (gpio == BUTTON_A && numero < 9) {
            numero++;
        } else if (gpio == BUTTON_B && numero > 0) {
            numero--;
        }
        atualizar_display = true;
    }
}

// Configuração inicial do sistema
void setup() {
    stdio_init_all();

    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Configura um único callback para ambos os botões
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &isr_botoes);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);

    npInit(LED_PIN);
    npClear();
    exibir_numero(numero); // inicia o display com 0
    npWrite();
}

// Loop principal
int main() {
    setup();
    repeating_timer_t timer;
    add_repeating_timer_ms(200, (repeating_timer_callback_t) piscar_led_rgb, NULL, &timer);
    while (true) {
        if (atualizar_display) {
            exibir_numero(numero);
            atualizar_display = false;
        }
    }
}
