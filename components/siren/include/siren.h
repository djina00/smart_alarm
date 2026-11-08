#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

// Komande za sirenu
typedef enum {
    SIREN_CMD_OFF,         // ugasi sirenu
    SIREN_CMD_ALARM,       // sirena radi isprekidano dok se ne ugasi
    SIREN_CMD_BEEP_ONCE,   // jedan kratak pisak (alarm uključen)
    SIREN_CMD_BEEP_TWICE,  // dva kratka piska (alarm isključen)
} siren_cmd_t;

// Podešava zujalicu na zadatom pinu i pokreće task sirene
esp_err_t siren_init(gpio_num_t buzzer_gpio);

// Šalje komandu tasku sirene; nova komanda prekida prethodnu
void siren_command(siren_cmd_t cmd);
