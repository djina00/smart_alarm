/*
 * Pametni kućni alarm
 *
 * Firmver je podeljen u FreeRTOS taskove:
 *   inputs - čita PIR senzor i dugme (preko prekida) i šalje događaje
 *   alarm  - mašina stanja: ISKLJUCEN -> UKLJUCEN -> ALARM
 *   siren  - upravlja zujalicom (sirena i kratki piskovi)
 * WiFi radi u pozadini (ESP-IDF event loop), a podaci o mreži
 * čitaju se iz NVS memorije.
 *
 * Povezivanje:
 *   PIR OUT        -> GPIO4
 *   Zujalica I/O   -> GPIO5 (pali se na LOW)
 *   Dugme BOOT     -> GPIO0 (na ploči)
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "alarm.h"
#include "inputs.h"
#include "siren.h"
#include "wifi.h"

static const char *TAG = "main";

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS particija je puna ili iz starije verzije: obriši i počni ispočetka
        ESP_LOGW(TAG, "NVS se brise i ponovo inicijalizuje");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Pametni alarm se pokrece");

    nvs_init();

    // Redosled je bitan: sirena i alarm moraju postojati pre nego što
    // ulazi počnu da šalju događaje
    siren_init();
    alarm_init();
    inputs_init();

    // Alarm radi i bez mreže; WiFi se povezuje u pozadini
    wifi_init();

    ESP_LOGI(TAG, "Pritisni dugme da ukljucis/iskljucis alarm");
}
