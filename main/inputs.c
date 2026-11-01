#include "inputs.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "alarm.h"

#define PIR_GPIO          GPIO_NUM_4
// Dugme BOOT na ploči; privremeno služi za uključivanje/isključivanje
// alarma, dok komande ne budu stizale sa servera
#define BUTTON_GPIO       GPIO_NUM_0

#define DEBOUNCE_MS       50
#define BUTTON_LOCKOUT_MS 300

static const char *TAG = "inputs";

static QueueHandle_t gpio_queue;

// Prekidna rutina: samo javlja tasku koji pin se promenio
static void IRAM_ATTR gpio_isr(void *arg)
{
    uint32_t pin = (uint32_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(gpio_queue, &pin, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void inputs_task(void *arg)
{
    TickType_t last_button = 0;
    uint32_t pin;

    while (1) {
        if (xQueueReceive(gpio_queue, &pin, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (pin == PIR_GPIO) {
            if (gpio_get_level(PIR_GPIO) == 1) {
                ESP_LOGI(TAG, "PIR: pokret");
                alarm_post_event(ALARM_EVENT_MOTION);
            }
        } else if (pin == BUTTON_GPIO) {
            // Zanemari treperenje kontakta dugmeta
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            TickType_t now = xTaskGetTickCount();
            if (gpio_get_level(BUTTON_GPIO) == 0 &&
                now - last_button > pdMS_TO_TICKS(BUTTON_LOCKOUT_MS)) {
                last_button = now;
                ESP_LOGI(TAG, "Dugme pritisnuto");
                alarm_post_event(ALARM_EVENT_TOGGLE_ARM);
            }
        }
    }
}

void inputs_init(void)
{
    gpio_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << PIR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,   // prekid kad PIR pređe u HIGH
    };
    ESP_ERROR_CHECK(gpio_config(&pir));

    gpio_config_t button = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,   // prekid kad se dugme pritisne
    };
    ESP_ERROR_CHECK(gpio_config(&button));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIR_GPIO, gpio_isr, (void *)PIR_GPIO));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, gpio_isr, (void *)BUTTON_GPIO));

    xTaskCreate(inputs_task, "inputs", 3072, NULL, 10, NULL);
}
