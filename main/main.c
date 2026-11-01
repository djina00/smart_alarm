/*
 * Pametni kućni alarm: osnovni test PIR senzora i zujalice
 *
 * Povezivanje (isto na pravom hardveru i u Wokwi simulatoru):
 *   PIR OUT        -> GPIO4
 *   Zujalica I/O   -> GPIO5 (zujalica se pali na LOW, ćuti na HIGH)
 *
 * Zujalica se pali PWM tonom (pin se smenjuje LOW/HIGH 2000 puta u sekundi),
 * a kad ćuti, pin stoji na HIGH. Tako radi i prava aktivna zujalica
 * i zujalica u simulatoru.
 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define PIR_GPIO         GPIO_NUM_4
#define BUZZER_GPIO      GPIO_NUM_5

#define BUZZER_OFF_LEVEL 1
#define BUZZER_FREQ_HZ   2000

static const char *TAG = "alarm";

static void pir_init(void)
{
    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << PIR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&pir));
}

static void buzzer_init(void)
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
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_OFF_LEVEL);
}

static void buzzer_set(bool on)
{
    if (on) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_OFF_LEVEL);
    }
}

static void buzzer_beep(int duration_ms)
{
    buzzer_set(true);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_set(false);
}

void app_main(void)
{
    pir_init();
    buzzer_init();

    ESP_LOGI(TAG, "Alarm pokrenut, cekam pokret...");
    buzzer_beep(100);

    int last = -1;
    while (1) {
        int motion = gpio_get_level(PIR_GPIO);
        if (motion != last) {
            if (motion) {
                ESP_LOGW(TAG, "Pokret!");
                buzzer_beep(200);
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_beep(200);
            } else {
                ESP_LOGI(TAG, "Nema pokreta");
            }
            last = motion;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
