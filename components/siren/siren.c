#include "siren.h"

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "buzzer.h"

#define ALARM_TOGGLE_MS 400   // sirena: 400 ms pišti, 400 ms ćuti
#define BEEP_MS         150

static const char *TAG = "siren";

static TaskHandle_t siren_task_handle;

static void beep(int duration_ms)
{
    buzzer_set(true);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_set(false);
}

static void siren_task(void *arg)
{
    uint32_t cmd = SIREN_CMD_OFF;

    while (1) {
        if (cmd == SIREN_CMD_ALARM) {
            ESP_LOGI(TAG, "Sirena ukljucena");
            bool on = true;
            // Sirena radi dok ne stigne nova komanda
            while (1) {
                buzzer_set(on);
                on = !on;
                if (xTaskNotifyWait(0, UINT32_MAX, &cmd, pdMS_TO_TICKS(ALARM_TOGGLE_MS)) == pdTRUE) {
                    break;
                }
            }
            buzzer_set(false);
            ESP_LOGI(TAG, "Sirena iskljucena");
            continue;   // obradi komandu koja je prekinula sirenu
        }

        if (cmd == SIREN_CMD_BEEP_ONCE) {
            beep(BEEP_MS);
        } else if (cmd == SIREN_CMD_BEEP_TWICE) {
            beep(BEEP_MS);
            vTaskDelay(pdMS_TO_TICKS(BEEP_MS));
            beep(BEEP_MS);
        }
        buzzer_set(false);

        // Čekaj sledeću komandu
        xTaskNotifyWait(0, UINT32_MAX, &cmd, portMAX_DELAY);
    }
}

esp_err_t siren_init(gpio_num_t buzzer_gpio)
{
    ESP_RETURN_ON_ERROR(buzzer_init(buzzer_gpio), TAG, "zujalica nije podesena");

    if (xTaskCreate(siren_task, "siren", 3072, NULL, 5, &siren_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void siren_command(siren_cmd_t cmd)
{
    if (siren_task_handle != NULL) {
        xTaskNotify(siren_task_handle, cmd, eSetValueWithOverwrite);
    }
}
