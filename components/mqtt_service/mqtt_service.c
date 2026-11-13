#include "mqtt_service.h"

#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "sdkconfig.h"

/*
 * MQTT teme (npr. za uređaj alarm-01):
 *   smart_alarm/alarm-01/events  uređaj -> server: pokret i promene stanja
 *   smart_alarm/alarm-01/status  uređaj -> server: trenutno stanje (retained)
 *   smart_alarm/alarm-01/cmd     server -> uređaj: {"cmd":"arm"} ili {"cmd":"disarm"}
 */

static const char *TAG = "mqtt_service";

static esp_mqtt_client_handle_t client;
static mqtt_command_cb_t command_cb;
static bool started = false;

static char topic_events[96];
static char topic_status[96];
static char topic_cmd[96];

// Poslednje stanje alarma; šalje se kao status pri svakom povezivanju
static char last_state[16] = "DISARMED";

// Trenutno stanje se šalje kao "retained" poruka: broker je pamti, pa svako
// ko se kasnije pretplati odmah dobije poslednje stanje
static void publish_status(void)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"online\":true,\"state\":\"%s\"}", last_state);
    // enqueue: poruka se čuva i šalje čim veza sa brokerom postoji
    esp_mqtt_client_enqueue(client, topic_status, payload, 0, 1, 1, true);
}

static void handle_command(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    const cJSON *cmd = cJSON_GetObjectItem(root, "cmd");

    if (cJSON_IsString(cmd)) {
        ESP_LOGI(TAG, "Komanda sa servera: %s", cmd->valuestring);
        command_cb(cmd->valuestring);
    } else {
        ESP_LOGW(TAG, "Neispravna komanda: %.*s", len, data);
    }
    cJSON_Delete(root);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = data;

    if (id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "Povezan na MQTT broker");
        esp_mqtt_client_subscribe(client, topic_cmd, 1);
        publish_status();
    } else if (id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "Veza sa MQTT brokerom prekinuta");
    } else if (id == MQTT_EVENT_DATA &&
               event->topic_len == strlen(topic_cmd) &&
               strncmp(event->topic, topic_cmd, event->topic_len) == 0) {
        handle_command(event->data, event->data_len);
    }
}

// Na broker se povezujemo tek kad uređaj dobije IP adresu
static void got_ip_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (!started) {
        ESP_LOGI(TAG, "Povezujem se na broker %s", CONFIG_ALARM_MQTT_BROKER_URI);
        esp_mqtt_client_start(client);
        started = true;
    }
}

void mqtt_service_init(mqtt_command_cb_t on_command)
{
    command_cb = on_command;

    snprintf(topic_events, sizeof(topic_events), "%s/%s/events",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_status, sizeof(topic_status), "%s/%s/status",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);
    snprintf(topic_cmd, sizeof(topic_cmd), "%s/%s/cmd",
             CONFIG_ALARM_MQTT_TOPIC_ROOT, CONFIG_ALARM_DEVICE_ID);

    esp_mqtt_client_config_t config = {
        .broker.address.uri = CONFIG_ALARM_MQTT_BROKER_URI,
        .credentials.client_id = CONFIG_ALARM_DEVICE_ID,
        // Last Will: ako uređaj nestane bez odjave, broker sam objavi da je offline
        .session.last_will = {
            .topic = topic_status,
            .msg = "{\"online\":false}",
            .qos = 1,
            .retain = 1,
        },
    };
    client = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler, NULL);
}

void mqtt_service_publish(const char *type, const char *state)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"%s\",\"state\":\"%s\"}", type, state);
    esp_mqtt_client_enqueue(client, topic_events, payload, 0, 1, 0, true);
    ESP_LOGI(TAG, "Poslato: %s", payload);

    if (strcmp(type, "state") == 0) {
        strlcpy(last_state, state, sizeof(last_state));
        publish_status();
    }
}
