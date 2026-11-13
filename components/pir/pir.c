#include "pir.h"

static gpio_num_t pir_gpio;

void pir_init(gpio_num_t gpio)
{
    pir_gpio = gpio;

    gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // dok PIR ćuti, pin je sigurno 0
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
}

bool pir_motion_detected(void)
{
    return gpio_get_level(pir_gpio) == 1;
}
