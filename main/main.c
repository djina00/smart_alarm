/*
 * Pametni kućni alarm
 *
 * Firmver je podeljen u FreeRTOS taskove:
 *   inputs - čita PIR senzor i dugme (preko prekida) i šalje događaje
 *   alarm  - mašina stanja: ISKLJUCEN -> UKLJUCEN -> ALARM
 *   siren  - upravlja zujalicom (sirena i kratki piskovi)
 *
 * Povezivanje:
 *   PIR OUT        -> GPIO4
 *   Zujalica I/O   -> GPIO5 (pali se na LOW)
 *   Dugme BOOT     -> GPIO0 (na ploči)
 */

#include "esp_log.h"
#include "alarm.h"
#include "inputs.h"
#include "siren.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Pametni alarm se pokrece");

    // Redosled je bitan: sirena i alarm moraju postojati pre nego što
    // ulazi počnu da šalju događaje
    siren_init();
    alarm_init();
    inputs_init();

    ESP_LOGI(TAG, "Pritisni dugme da ukljucis/iskljucis alarm");
}
