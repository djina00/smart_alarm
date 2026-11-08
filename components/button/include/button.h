#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

// Funkcija koja se poziva kad je dugme pritisnuto
typedef void (*button_press_cb_t)(void);

// Podešava dugme na zadatom pinu (ulaz sa pull-up otpornikom, pritisak
// spaja pin sa GND) i pokreće task koji poziva on_press za svaki pritisak
esp_err_t button_init(gpio_num_t gpio, button_press_cb_t on_press);
