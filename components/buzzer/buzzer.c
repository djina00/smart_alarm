#include "buzzer.h"

#include "driver/ledc.h"

// Modul zujalice se pali na LOW, a ćuti na HIGH. Pali se PWM tonom od
// 2000 Hz (pin se smenjuje LOW/HIGH), pa radi i prava zujalica i
// zujalica u Wokwi simulatoru.
#define BUZZER_OFF_LEVEL 1
#define BUZZER_FREQ_HZ   2000
#define BUZZER_DUTY      512    // pola od 1023: pola vremena LOW, pola HIGH

esp_err_t buzzer_init(gpio_num_t gpio)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        return err;
    }

    ledc_channel_config_t channel = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        return err;
    }

    buzzer_off();
    return ESP_OK;
}

void buzzer_on(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void buzzer_off(void)
{
    // Zaustavi ton i ostavi pin na nivou na kom zujalica ćuti
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BUZZER_OFF_LEVEL);
}
