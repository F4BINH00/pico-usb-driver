#include "pico/stdlib.h"
#include "tusb.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#define LED_PIN 25

#define CMD_LED_ON   0x01
#define CMD_LED_OFF  0x02
#define CMD_BLINK    0x03
#define CMD_ECHO     0x10

static bool led_state = false;

static void board_led_init(void)
{
#ifdef CYW43_WL_GPIO_LED_PIN
    // Pico W
    (void) cyw43_arch_init();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#else
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
#endif
}

static void board_led_set(bool on)
{
    led_state = on;
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
#else
    gpio_put(LED_PIN, on ? 1 : 0);
#endif
}

// Envia uma string curta de resposta via Vendor IN
static void send_text(const char *s)
{
    if (!tud_mounted()) return;

    uint16_t len = (uint16_t) strlen(s);
    if (len == 0) return;

    // Se não houver espaço, simplesmente não envia (para manter simples)
    if (tud_vendor_write_available() < len) return;

    tud_vendor_write(s, len);
    tud_vendor_flush();
}

// Envia bytes via Vendor IN (usado no ECHO)
static void send_bytes(const uint8_t *buf, uint16_t len)
{
    if (!tud_mounted()) return;
    if (!buf || len == 0) return;

    if (tud_vendor_write_available() < len) return;

    tud_vendor_write(buf, len);
    tud_vendor_flush();
}

static void handle_packet(uint8_t *buf, uint32_t len)
{
    if (!buf || len < 1) return;

    switch (buf[0]) {
        case CMD_LED_ON:
            board_led_set(true);
            send_text("OK:LED_ON");
            break;

        case CMD_LED_OFF:
            board_led_set(false);
            send_text("OK:LED_OFF");
            break;

        case CMD_BLINK: {
            // [CMD, reps, period_ms/10]
            uint8_t reps     = (len >= 2) ? buf[1] : 5;
            uint8_t period10 = (len >= 3) ? buf[2] : 25; // default 250 ms
            uint32_t period_ms = (uint32_t) period10 * 10u;

            for (uint8_t i = 0; i < reps; i++) {
                board_led_set(true);
                sleep_ms(period_ms / 2);
                board_led_set(false);
                sleep_ms(period_ms / 2);
            }

            send_text("OK:BLINK");
            break;
        }

        case CMD_ECHO:
            send_bytes(buf, (uint16_t) len);
            break;

        default:
            send_text("ERR:UNKNOWN_CMD");
            break;
    }
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize)
{
    (void) itf;

    if (!buffer || bufsize == 0) return;

    // Se handle_packet precisa de buffer mutável, copiamos para um buffer local.
    uint8_t local[512];
    uint16_t n = bufsize;
    if (n > sizeof(local)) n = sizeof(local);

    memcpy(local, buffer, n);
    handle_packet(local, (uint32_t) n);
}

int main(void)
{
    stdio_init_all();

    board_led_init();
    board_led_set(false);

    // Inicializa TinyUSB (device stack)
    tusb_init();

    while (true) {
        tud_task();            // processa a pilha USB
        tight_loop_contents(); // idle
    }
}

