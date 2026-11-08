#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

// Podešava zujalicu na zadatom pinu; posle poziva zujalica ćuti
esp_err_t buzzer_init(gpio_num_t gpio);

// Uključuje (true) ili isključuje (false) ton zujalice
void buzzer_set(bool on);
