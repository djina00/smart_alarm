#include "wifi.h"

#include <string.h>
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "sdkconfig.h"

#define NVS_NAMESPACE      "wifi"
#define NVS_KEY_SSID       "ssid"
#define NVS_KEY_PASSWORD   "password"

// Pauza između pokušaja ponovnog povezivanja raste do ove vrednosti
#define RETRY_MAX_DELAY_MS 30000

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi";

static EventGroupHandle_t wifi_events;
static TimerHandle_t retry_timer;
static int retry_count;

// Poziva se kad istekne pauza pred novi pokušaj povezivanja
static void retry_timer_cb(TimerHandle_t timer)
{
    esp_wifi_connect();
}

// Čita SSID i lozinku iz NVS-a. Ako ih nema (prvo pokretanje),
// upisuje podrazumevane vrednosti iz podešavanja projekta.
static esp_err_t load_credentials(char *ssid, size_t ssid_size,
                                  char *password, size_t password_size)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = ssid_size;
    err = nvs_get_str(nvs, NVS_KEY_SSID, ssid, &len);
    if (err == ESP_OK) {
        len = password_size;
        err = nvs_get_str(nvs, NVS_KEY_PASSWORD, password, &len);
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "U NVS-u nema WiFi podataka, upisujem podrazumevane");
        strlcpy(ssid, CONFIG_ALARM_WIFI_SSID, ssid_size);
        strlcpy(password, CONFIG_ALARM_WIFI_PASSWORD, password_size);
        err = nvs_set_str(nvs, NVS_KEY_SSID, ssid);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs, NVS_KEY_PASSWORD, password);
        }
        if (err == ESP_OK) {
            err = nvs_commit(nvs);
        }
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi podaci procitani iz NVS-a");
    }

    nvs_close(nvs);
    return err;
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);

        // Svaki sledeći pokušaj čeka duže: 1 s, 2 s, 4 s ... do 30 s
        int delay_ms = 1000 << (retry_count < 5 ? retry_count : 5);
        if (delay_ms > RETRY_MAX_DELAY_MS) {
            delay_ms = RETRY_MAX_DELAY_MS;
        }
        retry_count++;
        ESP_LOGW(TAG, "Veza prekinuta, novi pokusaj za %d s", delay_ms / 1000);
        // Tajmer umesto čekanja ovde, da ne blokiramo obradu drugih događaja
        xTimerChangePeriod(retry_timer, pdMS_TO_TICKS(delay_ms), 0);
        xTimerStart(retry_timer, 0);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Povezan na WiFi, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void)
{
    wifi_events = xEventGroupCreate();
    retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(1000), pdFALSE, NULL, retry_timer_cb);

    char ssid[33] = {0};       // najduži SSID je 32 znaka
    char password[65] = {0};   // najduža WPA2 lozinka je 64 znaka
    ESP_ERROR_CHECK(load_credentials(ssid, sizeof(ssid), password, sizeof(password)));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password));
    // Bez lozinke dozvoljavamo i otvorenu mrežu, inače traži bar WPA2
    wifi_cfg.sta.threshold.authmode = (password[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Lozinku nikad ne ispisujemo u log
    ESP_LOGI(TAG, "Povezujem se na mrezu \"%s\"...", ssid);
}

bool wifi_wait_connected(TickType_t timeout)
{
    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdTRUE, timeout);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
