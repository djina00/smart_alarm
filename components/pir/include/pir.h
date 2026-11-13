#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// Podešava pin PIR senzora kao ulaz
void pir_init(gpio_num_t gpio);

// Vraća true dok PIR vidi pokret
bool pir_motion_detected(void);
