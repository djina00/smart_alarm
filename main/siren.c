#include "siren.h"

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

// Zujalica se pali na LOW, a ćuti na HIGH. Pali se PWM tonom, pa radi
// i prava aktivna zujalica i zujalica u Wokwi simulatoru.
#define BUZZER_GPIO       5
#define BUZZER_OFF_LEVEL  1
#define BUZZER_FREQ_HZ    2000

#define ALARM_TOGGLE_MS   400   // sirena: 400 ms pišti, 400 ms ćuti
#define BEEP_MS           150

static const char *TAG = "siren";

static TaskHandle_t siren_task_handle;

static void buzzer_output(bool on)
{
    if (on) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_OFF_LEVEL);
    }
}

static void beep(int duration_ms)
{
    buzzer_output(true);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_output(false);
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
                buzzer_output(on);
                on = !on;
                if (xTaskNotifyWait(0, UINT32_MAX, &cmd, pdMS_TO_TICKS(ALARM_TOGGLE_MS)) == pdTRUE) {
                    break;
                }
            }
            buzzer_output(false);
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
        buzzer_output(false);

        // Čekaj sledeću komandu
        xTaskNotifyWait(0, UINT32_MAX, &cmd, portMAX_DELAY);
    }
}

void siren_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    buzzer_output(false);

    xTaskCreate(siren_task, "siren", 3072, NULL, 5, &siren_task_handle);
}

void siren_command(siren_cmd_t cmd)
{
    xTaskNotify(siren_task_handle, cmd, eSetValueWithOverwrite);
}
