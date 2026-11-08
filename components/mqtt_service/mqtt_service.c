#include "mqtt_service.h"

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
#include "wifi_manager.h"

/*
 * MQTT teme (npr. za uređaj alarm-01):
 *   smart_alarm/alarm-01/events  uređaj -> server: pokret i promene stanja
 *   smart_alarm/alarm-01/status  uređaj -> server: trenutno stanje (retained)
 *   smart_alarm/alarm-01/cmd     server -> uređaj: {"cmd":"arm"} ili {"cmd":"disarm"}
 */

#define MQTT_CONNECTED_BIT  BIT0
#define OUT_QUEUE_LENGTH    20
#define TOPIC_MAX_LEN       96
#define CMD_MAX_LEN         16

static const char *TAG = "mqtt_service";

typedef enum {
    MSG_MOTION,
    MSG_STATE,
} mqtt_msg_type_t;

// Poruka koja čeka u redu za slanje
typedef struct {
    mqtt_msg_type_t type;
    const char *state_code;
} mqtt_msg_t;

static esp_mqtt_client_handle_t client;
static EventGroupHandle_t mqtt_events;
static QueueHandle_t out_queue;

static mqtt_service_command_cb_t command_cb;
static mqtt_service_state_cb_t state_cb;

static char topic_events[TOPIC_MAX_LEN];
static char topic_status[TOPIC_MAX_LEN];
static char topic_cmd[TOPIC_MAX_LEN];

// Trenutno stanje se šalje kao "retained" poruka: broker je pamti,
// pa svako ko se kasnije pretplati odmah dobije poslednje stanje
static void publish_status(const char *state_code)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"online\":true,\"state\":\"%s\"}", state_code);
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
    if (cJSON_IsString(cmd) && strlen(cmd->valuestring) < CMD_MAX_LEN) {
        ESP_LOGI(TAG, "Komanda sa servera: %s", cmd->valuestring);
        if (command_cb != NULL) {
            command_cb(cmd->valuestring);
        }
    } else {
        ESP_LOGW(TAG, "Poruka bez ispravnog polja cmd: %.*s", len, data);
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
        if (state_cb != NULL) {
            publish_status(state_cb());
        }
        xEventGroupSetBits(mqtt_events, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Veza sa MQTT brokerom prekinuta");
        xEventGroupClearBits(mqtt_events, MQTT_CONNECTED_BIT);
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

static void mqtt_service_task(void *arg)
{
    // Na broker se povezujemo tek kad postoji WiFi veza
    wifi_manager_wait_connected(portMAX_DELAY);
    ESP_LOGI(TAG, "Povezujem se na broker %s", CONFIG_ALARM_MQTT_BROKER_URI);
    esp_mqtt_client_start(client);

    mqtt_msg_t msg;
    char payload[96];

    while (1) {
        xQueueReceive(out_queue, &msg, portMAX_DELAY);

        // Ako veza trenutno ne postoji, poruka čeka dok se veza ne vrati
        xEventGroupWaitBits(mqtt_events, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        if (msg.type == MSG_MOTION) {
            snprintf(payload, sizeof(payload), "{\"type\":\"motion\",\"state\":\"%s\"}", msg.state_code);
        } else {
            snprintf(payload, sizeof(payload), "{\"type\":\"state\",\"state\":\"%s\"}", msg.state_code);
            publish_status(msg.state_code);
        }

        if (esp_mqtt_client_publish(client, topic_events, payload, 0, 1, 0) < 0) {
            ESP_LOGE(TAG, "Slanje nije uspelo: %s", payload);
        } else {
            ESP_LOGI(TAG, "Poslato: %s", payload);
        }
    }
}

static void queue_message(mqtt_msg_type_t type, const char *state_code)
{
    if (out_queue == NULL) {
        return;   // servis još nije pokrenut
    }
    mqtt_msg_t msg = { .type = type, .state_code = state_code };
    if (xQueueSend(out_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Red za slanje je pun, poruka odbacena");
    }
}

void mqtt_service_publish_motion(const char *state_code)
{
    queue_message(MSG_MOTION, state_code);
}

void mqtt_service_publish_state(const char *state_code)
{
    queue_message(MSG_STATE, state_code);
}

void mqtt_service_init(mqtt_service_command_cb_t on_command, mqtt_service_state_cb_t get_state)
{
    command_cb = on_command;
    state_cb = get_state;

    snprintf(topic_events, sizeof(topic_events), "%s/%s/events",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_status, sizeof(topic_status), "%s/%s/status",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_cmd, sizeof(topic_cmd), "%s/%s/cmd",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);

    mqtt_events = xEventGroupCreate();
    out_queue = xQueueCreate(OUT_QUEUE_LENGTH, sizeof(mqtt_msg_t));

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

    xTaskCreate(mqtt_service_task, "mqtt_service", 4096, NULL, 4, NULL);
}
