#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/clocks.h"
#include <string.h>

int x = 0;
int y = 0;

const uint LED_GREEN = 11, LED_BLUE = 12, LED_RED = 13;
const uint BTN_A = 5;
#define I2C_SDA 14
#define I2C_SCL 15

const int JOY_X = 26;
const int JOY_Y = 27;
const int JOY_BTN = 22;
#define I2C_PORT i2c1
#define I2C_ADDR 0x3C
#define PWM_WRAP 4095

#define WIDTH 128 // Largura do display
#define HEIGHT 64 // Altura do display

// Definições de display
volatile bool led_state = true;        // Estado dos LEDs
volatile bool led_green_state = false; // Estado do LED verde
volatile int border_style = 0;         // Estilo da borda
volatile uint32_t last_press_time = 0; // Último tempo de pressionamento dos botões

// Função de interrupção
void gpio_irq_handler(uint gpio, uint32_t events)
{
    // Uso de debounce
    uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Tempo atual
    if (current_time - last_press_time > 200)
    { // Verifica se passou 200ms
        // Verifica qual botão foi pressionado
        if (gpio == BTN_A)
        {
            led_state = !led_state; // Alterna o estado dos LEDs caso o botão A seja pressionado
            if (!led_state)
            {
                pwm_set_gpio_level(LED_RED, 0);  // Desliga o LED vermelho
                pwm_set_gpio_level(LED_BLUE, 0); // Desliga o LED azul
            }
        }
        else if (gpio == JOY_BTN)
        {                                          // Se o botão do joystick for pressionado
            led_green_state = !led_green_state;    // Alterna o estado do LED verde
            gpio_put(LED_GREEN, led_green_state);  // Atualiza o estado do LED verde
            border_style = (border_style + 1) % 3; // Alterna o estilo da borda
        }
        last_press_time = current_time; // Atualiza o tempo de pressionamento
    }
}

// Função para configurar o PWM
void setup_pwm(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);        // Configura o pino para PWM
    uint slice = pwm_gpio_to_slice_num(pin);      // Obtém o slice do PWM
    pwm_config config = pwm_get_default_config(); // Configuração padrão do PWM
    pwm_config_set_wrap(&config, PWM_WRAP);       // Configura o wrap para 4095
    pwm_init(slice, &config, true);               // Inicializa o PWM
}

void i2c_setup()
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void joy_setup()
{
    adc_init();
    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);
    adc_gpio_init(JOY_BTN);
}

void setup()
{
    stdio_init_all();
    i2c_setup();
    joy_setup();
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_init(LED_RED);
    gpio_init(BTN_A);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Configura a interrupção para o botão A
    gpio_init(JOY_BTN);
    gpio_set_dir(JOY_BTN, GPIO_IN);
    gpio_pull_up(JOY_BTN);
    gpio_set_irq_enabled_with_callback(JOY_BTN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Configura a interrupção para o botão do joystick
    setup_pwm(LED_RED);
    setup_pwm(LED_BLUE);
}

void clear_display(ssd1306_t *ssd)
{
    memset(ssd->ram_buffer + 1, 0, ssd->bufsize - 1);
    ssd1306_send_data(ssd);
}

int main()
{
    setup();

    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, I2C_ADDR, I2C_PORT);
    ssd1306_config(&ssd);

    while (true)
    {
        uint16_t adc_x, adc_y;
        int cursor_x, cursor_y;

        // Leitura dos valores do joystick
        adc_select_input(1); // Seleciona o pino do joystick no eixo X
        adc_x = adc_read();  // Lê o valor do ADC
        adc_select_input(0); // Seleciona o pino do joystick no eixo Y
        adc_y = adc_read();  // Lê o valor do ADC

        // Cálculo da margem de zona morta
        int delta_x = abs(2048 - adc_x); // Calcula a diferença entre o valor lido e o valor central em relação ao eixo X
        int delta_y = abs(2048 - adc_y); // Calcula a diferença entre o valor lido e o valor central em relação ao eixo Y
        int dead_zone = 200;             // Define o limite de zona morta

        // Atualização dos LEDs
        if (led_state)
        {
            if (delta_x > dead_zone)
            {                                                           // Se a diferença for maior que o limite
                pwm_set_gpio_level(LED_RED, (delta_x - dead_zone) * 2); // Ajusta o brilho do LED vermelho
            }
            else
            {
                pwm_set_gpio_level(LED_RED, 0); // Desliga o LED vermelho
            }

            if (delta_y > dead_zone)
            {                                                            // Se a diferença for maior que o limite
                pwm_set_gpio_level(LED_BLUE, (delta_y - dead_zone) * 2); // Ajusta o brilho do LED azul
            }
            else
            {
                pwm_set_gpio_level(LED_BLUE, 0); // Desliga o LED azul
            }
        }
        else
        {
            pwm_set_gpio_level(LED_RED, 0);  // Desliga o LED vermelho
            pwm_set_gpio_level(LED_BLUE, 0); // Desliga o LED azul
        }

        // Ajuste para centralizar o cursor no display
        cursor_x = (adc_x * (WIDTH - 8)) / 4095;               // Calcula a posição do cursor no eixo X
        cursor_y = HEIGHT - 8 - (adc_y * (HEIGHT - 8)) / 4095; // Calcula a posição do cursor no eixo Y

        // Centraliza o cursor no display
        cursor_x = cursor_x - (WIDTH / 2) + 4;
        cursor_y = cursor_y - (HEIGHT / 2) + 4;

        clear_display(&ssd);
        // Desenha a borda
        if (border_style == 1)
        {
            ssd1306_rect(&ssd, 2, 2, WIDTH - 4, HEIGHT - 4, true, false);
        }
        else if (border_style == 2)
        {
            ssd1306_rect(&ssd, 0, 0, WIDTH, HEIGHT, true, false);
        }

        ssd1306_rect(&ssd, cursor_x, cursor_y, 8, 8, true, true);
        ssd1306_send_data(&ssd);

        sleep_ms(100);
    }

    return 0;
}