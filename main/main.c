/*
 * Pametni kućni alarm
 *
 * Komponente (folder components/):
 *   pir, button   - čitanje PIR senzora i dugmeta
 *   buzzer        - zujalica
 *   alarm         - mašina stanja ISKLJUCEN -> UKLJUCEN -> ALARM i sirena
 *   wifi_manager  - WiFi, podaci o mreži se čitaju iz NVS-a
 *   mqtt_service  - MQTT: šalje događaje serveru i prima komande
 *
 * FreeRTOS taskovi:
 *   sensor_task (ovde) - 10 puta u sekundi čita PIR i dugme
 *   alarm (komponenta) - obrađuje događaje i upravlja sirenom
 * Taskovi razmenjuju događaje preko reda (alarm_post_event).
 *
 * Povezivanje:
 *   PIR OUT        -> GPIO4
 *   Zujalica I/O   -> GPIO5 (pali se na LOW)
 *   Dugme BOOT     -> GPIO0 (na ploči)
 */

#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "alarm.h"
#include "button.h"
#include "buzzer.h"
#include "mqtt_service.h"
#include "pir.h"
#include "wifi_manager.h"

#define PIR_GPIO    GPIO_NUM_4
#define BUZZER_GPIO GPIO_NUM_5
#define BUTTON_GPIO GPIO_NUM_0

#define SENSOR_PERIOD_MS 100

static const char *TAG = "main";

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS particija je puna ili iz starije verzije: obriši i počni ispočetka
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// Alarm -> server: pokret dok je alarm uključen ili novo stanje
static void on_alarm_report(const char *type, alarm_state_t state)
{
    mqtt_service_publish(type, alarm_state_code(state));
}

// Server -> alarm: komanda "arm" ili "disarm"
static void on_server_command(const char *cmd)
{
    if (strcmp(cmd, "arm") == 0) {
        alarm_post_event(ALARM_EVENT_ARM);
    } else if (strcmp(cmd, "disarm") == 0) {
        alarm_post_event(ALARM_EVENT_DISARM);
    }
}

// Čita senzore i javlja alarmu samo trenutak kad pokret počne,
// odnosno kad se dugme pritisne (prelaz iz "ne" u "da")
static void sensor_task(void *arg)
{
    bool last_motion = false;
    bool last_button = false;

    while (1) {
        bool motion = pir_motion_detected();
        bool button = button_is_pressed();

        if (motion && !last_motion) {
            ESP_LOGI(TAG, "PIR: pokret");
            alarm_post_event(ALARM_EVENT_MOTION);
        }
        if (button && !last_button) {
            ESP_LOGI(TAG, "Dugme pritisnuto");
            alarm_post_event(ALARM_EVENT_BUTTON);
        }

        last_motion = motion;
        last_button = button;
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Pametni alarm se pokrece");

    nvs_init();

    pir_init(PIR_GPIO);
    button_init(BUTTON_GPIO);
    ESP_ERROR_CHECK(buzzer_init(BUZZER_GPIO));

    alarm_init(on_alarm_report);

    // Alarm radi i bez mreže; WiFi i MQTT se povezuju u pozadini
    wifi_manager_init();
    mqtt_service_init(on_server_command);

    xTaskCreate(sensor_task, "sensor", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "Pritisni dugme da ukljucis/iskljucis alarm");
}
