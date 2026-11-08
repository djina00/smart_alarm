/*
 * Pametni kućni alarm
 *
 * Svaki deo sistema je posebna komponenta (folder components/):
 *   pir, button   - ulazi: javljaju pokret i pritisak dugmeta
 *   buzzer, siren - zujalica i ponašanje sirene (sirena, piskovi)
 *   alarm         - mašina stanja: ISKLJUCEN -> UKLJUCEN -> ALARM
 *   wifi_manager  - WiFi, podaci o mreži se čitaju iz NVS-a
 *   mqtt_service  - MQTT: šalje događaje serveru i prima komande
 *
 * Komponente ne znaju jedna za drugu. main.c ih pokreće i povezuje
 * preko callback funkcija (funkcija koju komponenta pozove kad se
 * nešto desi).
 *
 * Povezivanje:
 *   PIR OUT        -> GPIO4
 *   Zujalica I/O   -> GPIO5 (pali se na LOW)
 *   Dugme BOOT     -> GPIO0 (na ploči)
 */

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "alarm.h"
#include "button.h"
#include "mqtt_service.h"
#include "pir.h"
#include "siren.h"
#include "wifi_manager.h"

#define PIR_GPIO    GPIO_NUM_4
#define BUZZER_GPIO GPIO_NUM_5
#define BUTTON_GPIO GPIO_NUM_0

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

// --- Veze između komponenti ---

// PIR -> alarm
static void on_motion(void)
{
    alarm_post_event(ALARM_EVENT_MOTION);
}

// Dugme -> alarm
static void on_button_press(void)
{
    alarm_post_event(ALARM_EVENT_TOGGLE_ARM);
}

// Alarm -> MQTT: novo stanje
static void on_alarm_state_change(alarm_state_t new_state)
{
    mqtt_service_publish_state(alarm_state_code(new_state));
}

// Alarm -> MQTT: pokret dok je alarm uključen
static void on_armed_motion(alarm_state_t state)
{
    mqtt_service_publish_motion(alarm_state_code(state));
}

// MQTT -> alarm: komanda sa servera
static void on_server_command(const char *cmd)
{
    if (strcmp(cmd, "arm") == 0) {
        alarm_post_event(ALARM_EVENT_ARM);
    } else if (strcmp(cmd, "disarm") == 0) {
        alarm_post_event(ALARM_EVENT_DISARM);
    } else {
        ESP_LOGW(TAG, "Nepoznata komanda: %s", cmd);
    }
}

// MQTT pita za trenutno stanje kad se poveže na broker
static const char *current_state_code(void)
{
    return alarm_state_code(alarm_get_state());
}

void app_main(void)
{
    ESP_LOGI(TAG, "Pametni alarm se pokrece");

    nvs_init();

    // Redosled je bitan: sirena i alarm moraju postojati pre nego što
    // ulazi počnu da šalju događaje
    ESP_ERROR_CHECK(siren_init(BUZZER_GPIO));
    ESP_ERROR_CHECK(alarm_init(on_alarm_state_change, on_armed_motion));
    ESP_ERROR_CHECK(pir_init(PIR_GPIO, on_motion));
    ESP_ERROR_CHECK(button_init(BUTTON_GPIO, on_button_press));

    // Alarm radi i bez mreže; WiFi i MQTT se povezuju u pozadini
    wifi_manager_init();
    mqtt_service_init(on_server_command, current_state_code);

    ESP_LOGI(TAG, "Pritisni dugme da ukljucis/iskljucis alarm");
}
