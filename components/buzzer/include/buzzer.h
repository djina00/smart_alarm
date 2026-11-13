#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

// Podešava zujalicu na zadatom pinu; posle poziva zujalica ćuti
esp_err_t buzzer_init(gpio_num_t gpio);

void buzzer_on(void);
void buzzer_off(void);
