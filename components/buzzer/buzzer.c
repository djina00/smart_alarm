#include "buzzer.h"

#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"

// Modul zujalice se pali na LOW, a ćuti na HIGH. Pali se PWM tonom
// (pin se smenjuje LOW/HIGH 2000 puta u sekundi), pa radi i prava aktivna
// zujalica i pasivna zujalica u Wokwi simulatoru.
#define BUZZER_OFF_LEVEL 1
#define BUZZER_FREQ_HZ   2000
#define BUZZER_DUTY_HALF 512    // pola od 1023: pola vremena LOW, pola HIGH

#define BUZZER_SPEED_MODE LEDC_LOW_SPEED_MODE
#define BUZZER_TIMER      LEDC_TIMER_0
#define BUZZER_CHANNEL    LEDC_CHANNEL_0

static const char *TAG = "buzzer";

esp_err_t buzzer_init(gpio_num_t gpio)
{
    ledc_timer_config_t timer = {
        .speed_mode = BUZZER_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BUZZER_TIMER,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "podesavanje nije uspelo");

    ledc_channel_config_t channel = {
        .gpio_num = gpio,
        .speed_mode = BUZZER_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "podesavanje nije uspelo");

    buzzer_set(false);
    ESP_LOGI(TAG, "Zujalica spremna na GPIO%d", gpio);
    return ESP_OK;
}

void buzzer_set(bool on)
{
    if (on) {
        ledc_set_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL, BUZZER_DUTY_HALF);
        ledc_update_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL);
    } else {
        // Zaustavi PWM i ostavi pin na nivou na kom zujalica ćuti
        ledc_stop(BUZZER_SPEED_MODE, BUZZER_CHANNEL, BUZZER_OFF_LEVEL);
    }
}
