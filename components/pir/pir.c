#include "pir.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "pir";

static gpio_num_t pir_gpio;
static pir_motion_cb_t motion_cb;
static QueueHandle_t edge_queue;

// Prekidna rutina: samo javlja tasku da se pin promenio
static void IRAM_ATTR pir_isr(void *arg)
{
    uint8_t edge = 1;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(edge_queue, &edge, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void pir_task(void *arg)
{
    uint8_t edge;
    while (1) {
        if (xQueueReceive(edge_queue, &edge, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        // Proveri da je pin zaista HIGH (zanemari kratke smetnje)
        if (gpio_get_level(pir_gpio) == 1) {
            ESP_LOGI(TAG, "Pokret");
            motion_cb();
        }
    }
}

esp_err_t pir_init(gpio_num_t gpio, pir_motion_cb_t on_motion)
{
    pir_gpio = gpio;
    motion_cb = on_motion;

    edge_queue = xQueueCreate(5, sizeof(uint8_t));
    if (edge_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // dok PIR ćuti, pin je sigurno 0
        .intr_type = GPIO_INTR_POSEDGE,         // prekid kad PIR pređe u HIGH
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "podesavanje nije uspelo");

    // Servis za prekide se instalira jednom za ceo čip; ako ga je već
    // instalirala druga komponenta, to nije greška
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(gpio, pir_isr, NULL), TAG, "podesavanje nije uspelo");

    if (xTaskCreate(pir_task, "pir", 3072, NULL, 10, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "PIR spreman na GPIO%d", gpio);
    return ESP_OK;
}
