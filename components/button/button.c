#include "button.h"

static gpio_num_t button_gpio;

void button_init(gpio_num_t gpio)
{
    button_gpio = gpio;

    gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,       // nepritisnuto dugme čita 1
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);
}

bool button_is_pressed(void)
{
    return gpio_get_level(button_gpio) == 0;
}
