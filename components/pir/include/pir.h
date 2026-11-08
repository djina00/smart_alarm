#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

// Funkcija koja se poziva kad PIR detektuje pokret
typedef void (*pir_motion_cb_t)(void);

// Podešava PIR na zadatom pinu (ulaz, prekid na rastuću ivicu) i pokreće
// task koji poziva on_motion svaki put kad pokret počne
esp_err_t pir_init(gpio_num_t gpio, pir_motion_cb_t on_motion);
