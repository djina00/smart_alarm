#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// Čita WiFi podatke iz NVS-a i pokreće povezivanje na mrežu.
// NVS mora biti inicijalizovan pre poziva.
void wifi_init(void);

// Čeka da uređaj dobije IP adresu; vraća true ako je povezan
bool wifi_wait_connected(TickType_t timeout);
