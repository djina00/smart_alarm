#include "comm.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include "wifi.h"

/*
 * MQTT teme (npr. za uređaj alarm-01):
 *   smart_alarm/alarm-01/events  uređaj -> server: pokret i promene stanja
 *   smart_alarm/alarm-01/status  uređaj -> server: trenutno stanje (retained)
 *   smart_alarm/alarm-01/cmd     server -> uređaj: {"cmd":"arm"} ili {"cmd":"disarm"}
 */

#define MQTT_CONNECTED_BIT  BIT0
#define OUT_QUEUE_LENGTH    20
#define TOPIC_MAX_LEN       96

static const char *TAG = "comm";

typedef enum {
    MSG_MOTION,
    MSG_STATE,
} comm_msg_type_t;

// Poruka koju logika alarma predaje tasku za komunikaciju
typedef struct {
    comm_msg_type_t type;
    alarm_state_t state;
} comm_msg_t;

static esp_mqtt_client_handle_t client;
static EventGroupHandle_t comm_events;
static QueueHandle_t out_queue;

static char topic_events[TOPIC_MAX_LEN];
static char topic_status[TOPIC_MAX_LEN];
static char topic_cmd[TOPIC_MAX_LEN];

static const char *state_code(alarm_state_t state)
{
    switch (state) {
    case ALARM_STATE_DISARMED:  return "DISARMED";
    case ALARM_STATE_ARMED:     return "ARMED";
    case ALARM_STATE_TRIGGERED: return "TRIGGERED";
    }
    return "UNKNOWN";
}

// Trenutno stanje se šalje kao "retained" poruka: broker je pamti,
// pa svako ko se kasnije pretplati odmah dobije poslednje stanje
static void publish_status(alarm_state_t state)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"online\":true,\"state\":\"%s\"}", state_code(state));
    esp_mqtt_client_publish(client, topic_status, payload, 0, 1, 1);
}

static void handle_command(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (root == NULL) {
        ESP_LOGW(TAG, "Komanda nije ispravan JSON: %.*s", len, data);
        return;
    }

    const cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cJSON_IsString(cmd) && strcmp(cmd->valuestring, "arm") == 0) {
        ESP_LOGI(TAG, "Komanda sa servera: ukljuci alarm");
        alarm_post_event(ALARM_EVENT_ARM);
    } else if (cJSON_IsString(cmd) && strcmp(cmd->valuestring, "disarm") == 0) {
        ESP_LOGI(TAG, "Komanda sa servera: iskljuci alarm");
        alarm_post_event(ALARM_EVENT_DISARM);
    } else {
        ESP_LOGW(TAG, "Nepoznata komanda: %.*s", len, data);
    }

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Povezan na MQTT broker");
        esp_mqtt_client_subscribe(client, topic_cmd, 1);
        publish_status(alarm_get_state());
        xEventGroupSetBits(comm_events, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Veza sa MQTT brokerom prekinuta");
        xEventGroupClearBits(comm_events, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len == strlen(topic_cmd) &&
            strncmp(event->topic, topic_cmd, event->topic_len) == 0) {
            handle_command(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT greska");
        break;

    default:
        break;
    }
}

static void comm_task(void *arg)
{
    // Na broker se povezujemo tek kad postoji WiFi veza
    wifi_wait_connected(portMAX_DELAY);
    ESP_LOGI(TAG, "Povezujem se na broker %s", CONFIG_ALARM_MQTT_BROKER_URI);
    esp_mqtt_client_start(client);

    comm_msg_t msg;
    char payload[96];

    while (1) {
        xQueueReceive(out_queue, &msg, portMAX_DELAY);

        // Ako veza trenutno ne postoji, poruka čeka dok se veza ne vrati
        xEventGroupWaitBits(comm_events, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        if (msg.type == MSG_MOTION) {
            snprintf(payload, sizeof(payload), "{\"type\":\"motion\",\"state\":\"%s\"}",
                     state_code(msg.state));
        } else {
            snprintf(payload, sizeof(payload), "{\"type\":\"state\",\"state\":\"%s\"}",
                     state_code(msg.state));
            publish_status(msg.state);
        }

        if (esp_mqtt_client_publish(client, topic_events, payload, 0, 1, 0) < 0) {
            ESP_LOGE(TAG, "Slanje nije uspelo: %s", payload);
        } else {
            ESP_LOGI(TAG, "Poslato: %s", payload);
        }
    }
}

static void queue_message(comm_msg_type_t type, alarm_state_t state)
{
    if (out_queue == NULL) {
        return;   // komunikacija još nije pokrenuta
    }
    comm_msg_t msg = { .type = type, .state = state };
    if (xQueueSend(out_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Red za slanje je pun, poruka odbacena");
    }
}

void comm_publish_motion(alarm_state_t state)
{
    queue_message(MSG_MOTION, state);
}

void comm_publish_state(alarm_state_t state)
{
    queue_message(MSG_STATE, state);
}

void comm_init(void)
{
    snprintf(topic_events, sizeof(topic_events), "%s/%s/events",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_status, sizeof(topic_status), "%s/%s/status",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_cmd, sizeof(topic_cmd), "%s/%s/cmd",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);

    comm_events = xEventGroupCreate();
    out_queue = xQueueCreate(OUT_QUEUE_LENGTH, sizeof(comm_msg_t));

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_ALARM_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_ALARM_DEVICE_ID,
        // Ako uređaj nestane bez odjave, broker sam objavi da je offline
        .session.last_will = {
            .topic = topic_status,
            .msg = "{\"online\":false}",
            .qos = 1,
            .retain = 1,
        },
    };
    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    xTaskCreate(comm_task, "comm", 4096, NULL, 4, NULL);
}
