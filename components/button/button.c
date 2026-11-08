#include "button.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"

#define DEBOUNCE_MS       50    // vreme da se kontakt dugmeta smiri
#define BUTTON_LOCKOUT_MS 300   // pritisci brži od ovoga se zanemaruju

static const char *TAG = "button";

static gpio_num_t button_gpio;
static button_press_cb_t press_cb;
static QueueHandle_t edge_queue;

// Prekidna rutina: samo javlja tasku da se pin promenio
static void IRAM_ATTR button_isr(void *arg)
{
    uint8_t edge = 1;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(edge_queue, &edge, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg)
{
    TickType_t last_press = 0;
    uint8_t edge;

    while (1) {
        if (xQueueReceive(edge_queue, &edge, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        // Mehanički kontakt "odskače" nekoliko puta; sačekaj da se smiri
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
        TickType_t now = xTaskGetTickCount();
        if (gpio_get_level(button_gpio) == 0 &&
            now - last_press > pdMS_TO_TICKS(BUTTON_LOCKOUT_MS)) {
            last_press = now;
            ESP_LOGI(TAG, "Dugme pritisnuto");
            press_cb();
        }
    }
}

esp_err_t button_init(gpio_num_t gpio, button_press_cb_t on_press)
{
    button_gpio = gpio;
    press_cb = on_press;

    edge_queue = xQueueCreate(5, sizeof(uint8_t));
    if (edge_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,       // nepritisnuto dugme čita 1
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,         // prekid kad pritisak spusti pin na 0
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "podesavanje nije uspelo");

    // Servis za prekide se instalira jednom za ceo čip; ako ga je već
    // instalirala druga komponenta, to nije greška
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(gpio, button_isr, NULL), TAG, "podesavanje nije uspelo");

    if (xTaskCreate(button_task, "button", 3072, NULL, 9, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Dugme spremno na GPIO%d", gpio);
    return ESP_OK;
}
