#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// Podešava pin dugmeta kao ulaz (pritisak spaja pin sa GND)
void button_init(gpio_num_t gpio);

// Vraća true dok je dugme pritisnuto
bool button_is_pressed(void);
