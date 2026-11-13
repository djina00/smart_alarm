#include "alarm.h"

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "buzzer.h"

#define SIREN_TOGGLE_MS   400     // sirena: 400 ms pišti, 400 ms ćuti
#define SIREN_MAX_MS      60000   // posle 60 s sirena se sama gasi
#define BEEP_MS           150

static const char *TAG = "alarm";

static QueueHandle_t event_queue;
static alarm_report_cb_t report_cb;

static alarm_state_t state = ALARM_STATE_DISARMED;
static TickType_t siren_start;
static bool siren_sound_on;

static const char *state_name(alarm_state_t s)
{
    switch (s) {
    case ALARM_STATE_DISARMED:  return "ISKLJUCEN";
    case ALARM_STATE_ARMED:     return "UKLJUCEN";
    case ALARM_STATE_TRIGGERED: return "ALARM";
    }
    return "?";
}

// Kratki piskovi kao potvrda: 1 = uključen, 2 = isključen
static void beep(int count)
{
    for (int i = 0; i < count; i++) {
        buzzer_on();
        vTaskDelay(pdMS_TO_TICKS(BEEP_MS));
        buzzer_off();
        vTaskDelay(pdMS_TO_TICKS(BEEP_MS));
    }
}

static void set_state(alarm_state_t new_state)
{
    ESP_LOGW(TAG, "Stanje: %s -> %s", state_name(state), state_name(new_state));
    state = new_state;
    report_cb("state", new_state);
}

static void arm(void)
{
    if (state == ALARM_STATE_DISARMED) {
        set_state(ALARM_STATE_ARMED);
        beep(1);
    }
}

static void disarm(void)
{
    if (state != ALARM_STATE_DISARMED) {
        buzzer_off();
        set_state(ALARM_STATE_DISARMED);
        beep(2);
    }
}

static void handle_event(alarm_event_t event)
{
    switch (event) {
    case ALARM_EVENT_MOTION:
        if (state == ALARM_STATE_ARMED) {
            ESP_LOGW(TAG, "Pokret dok je alarm ukljucen!");
            // Serveru se javlja samo pokret detektovan dok je alarm uključen
            report_cb("motion", state);
            set_state(ALARM_STATE_TRIGGERED);
            siren_start = xTaskGetTickCount();
            siren_sound_on = true;
            buzzer_on();
        } else if (state == ALARM_STATE_DISARMED) {
            ESP_LOGI(TAG, "Pokret, ali alarm je iskljucen (ignorise se)");
        }
        break;

    case ALARM_EVENT_BUTTON:
        if (state == ALARM_STATE_DISARMED) {
            arm();
        } else {
            disarm();
        }
        break;

    case ALARM_EVENT_ARM:
        arm();
        break;

    case ALARM_EVENT_DISARM:
        disarm();
        break;
    }
}

// Poziva se svakih 400 ms dok je stanje ALARM
static void siren_step(void)
{
    if (xTaskGetTickCount() - siren_start >= pdMS_TO_TICKS(SIREN_MAX_MS)) {
        ESP_LOGW(TAG, "Sirena ugasena posle 60 s, alarm ostaje ukljucen");
        buzzer_off();
        siren_sound_on = false;
        set_state(ALARM_STATE_ARMED);
        return;
    }

    siren_sound_on = !siren_sound_on;
    if (siren_sound_on) {
        buzzer_on();
    } else {
        buzzer_off();
    }
}

static void alarm_task(void *arg)
{
    alarm_event_t event;

    while (1) {
        // Dok traje alarm, budimo se svakih 400 ms da sirena pišti isprekidano;
        // inače spavamo dok ne stigne događaj
        TickType_t wait = (state == ALARM_STATE_TRIGGERED)
                              ? pdMS_TO_TICKS(SIREN_TOGGLE_MS)
                              : portMAX_DELAY;

        if (xQueueReceive(event_queue, &event, wait) == pdTRUE) {
            handle_event(event);
        } else if (state == ALARM_STATE_TRIGGERED) {
            siren_step();
        }
    }
}

void alarm_init(alarm_report_cb_t on_report)
{
    report_cb = on_report;
    event_queue = xQueueCreate(10, sizeof(alarm_event_t));
    xTaskCreate(alarm_task, "alarm", 4096, NULL, 5, NULL);
}

void alarm_post_event(alarm_event_t event)
{
    xQueueSend(event_queue, &event, 0);
}

const char *alarm_state_code(alarm_state_t s)
{
    switch (s) {
    case ALARM_STATE_DISARMED:  return "DISARMED";
    case ALARM_STATE_ARMED:     return "ARMED";
    case ALARM_STATE_TRIGGERED: return "TRIGGERED";
    }
    return "UNKNOWN";
}
