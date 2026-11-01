#include "alarm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "siren.h"

// Posle ovoliko vremena sirena se sama gasi, a alarm ostaje uključen
#define SIREN_MAX_DURATION_MS 60000

#define EVENT_QUEUE_LENGTH 10

static const char *TAG = "alarm";

static QueueHandle_t event_queue;
static alarm_state_t state = ALARM_STATE_DISARMED;
static TickType_t siren_deadline;

static const char *state_name(alarm_state_t s)
{
    switch (s) {
    case ALARM_STATE_DISARMED:  return "ISKLJUCEN";
    case ALARM_STATE_ARMED:     return "UKLJUCEN";
    case ALARM_STATE_TRIGGERED: return "ALARM";
    }
    return "?";
}

static void set_state(alarm_state_t new_state)
{
    ESP_LOGW(TAG, "Stanje: %s -> %s", state_name(state), state_name(new_state));
    state = new_state;
}

static void handle_event(alarm_event_t event)
{
    switch (event) {
    case ALARM_EVENT_MOTION:
        if (state == ALARM_STATE_ARMED) {
            ESP_LOGW(TAG, "Pokret dok je alarm ukljucen!");
            set_state(ALARM_STATE_TRIGGERED);
            siren_command(SIREN_CMD_ALARM);
            siren_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(SIREN_MAX_DURATION_MS);
        } else if (state == ALARM_STATE_DISARMED) {
            ESP_LOGI(TAG, "Pokret, ali alarm je iskljucen (ignorise se)");
        }
        // U stanju ALARM sirena već radi, novi pokret ništa ne menja
        break;

    case ALARM_EVENT_TOGGLE_ARM:
        if (state == ALARM_STATE_DISARMED) {
            set_state(ALARM_STATE_ARMED);
            siren_command(SIREN_CMD_BEEP_ONCE);
        } else {
            // Iz stanja UKLJUCEN ili ALARM ide se u ISKLJUCEN
            set_state(ALARM_STATE_DISARMED);
            siren_command(SIREN_CMD_BEEP_TWICE);
        }
        break;
    }
}

static void alarm_task(void *arg)
{
    ESP_LOGI(TAG, "Logika alarma pokrenuta, stanje: %s", state_name(state));

    while (1) {
        // Dok sirena radi, čekamo događaj najduže do isteka vremena sirene
        TickType_t wait = portMAX_DELAY;
        if (state == ALARM_STATE_TRIGGERED) {
            TickType_t now = xTaskGetTickCount();
            wait = (siren_deadline > now) ? (siren_deadline - now) : 0;
        }

        alarm_event_t event;
        if (xQueueReceive(event_queue, &event, wait) == pdTRUE) {
            handle_event(event);
        } else if (state == ALARM_STATE_TRIGGERED) {
            ESP_LOGW(TAG, "Sirena ugasena posle %d s, alarm ostaje ukljucen",
                     SIREN_MAX_DURATION_MS / 1000);
            siren_command(SIREN_CMD_OFF);
            set_state(ALARM_STATE_ARMED);
        }
    }
}

void alarm_init(void)
{
    event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(alarm_event_t));
    xTaskCreate(alarm_task, "alarm", 4096, NULL, 8, NULL);
}

void alarm_post_event(alarm_event_t event)
{
    if (xQueueSend(event_queue, &event, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Red dogadjaja je pun, dogadjaj odbacen");
    }
}
