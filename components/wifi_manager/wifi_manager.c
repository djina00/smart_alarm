#include "wifi_manager.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_manager";

// Čita SSID i lozinku iz NVS-a. Ako ih nema (prvo pokretanje),
// upisuje podrazumevane vrednosti iz menija "WiFi Manager".
static void load_credentials(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &nvs));

    size_t ssid_len = ssid_size;
    size_t password_len = password_size;
    if (nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK &&
        nvs_get_str(nvs, "password", password, &password_len) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi podaci procitani iz NVS-a");
    } else {
        ESP_LOGW(TAG, "U NVS-u nema WiFi podataka, upisujem podrazumevane");
        strlcpy(ssid, CONFIG_ALARM_WIFI_SSID, ssid_size);
        strlcpy(password, CONFIG_ALARM_WIFI_PASSWORD, password_size);
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "password", password);
        nvs_commit(nvs);
    }

    nvs_close(nvs);
}

// Najčešći razlozi zbog kojih povezivanje ne uspeva
static const char *disconnect_hint(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return "mreza nije pronadjena (pogresan SSID, 5 GHz ili predaleko)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "pogresna lozinka";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return "nepodrzana zastita mreze";
    default:
        return "ostalo";
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *info = data;
        ESP_LOGW(TAG, "Veza prekinuta (razlog %d: %s), pokusavam ponovo",
                 info->reason, disconnect_hint(info->reason));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        ESP_LOGI(TAG, "Povezan na WiFi, IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_manager_init(void)
{
    char ssid[33] = {0};       // najduži SSID je 32 znaka
    char password[65] = {0};   // najduža lozinka je 64 znaka
    load_credentials(ssid, sizeof(ssid), password, sizeof(password));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t config = {0};
    strlcpy((char *)config.sta.ssid, ssid, sizeof(config.sta.ssid));
    strlcpy((char *)config.sta.password, password, sizeof(config.sta.password));
    // Bez lozinke dozvoljavamo otvorenu mrežu, inače bar WPA2 (WPA3 je takođe u redu)
    config.sta.threshold.authmode = (password[0] == '\0') ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    // WPA3 mreže (npr. hotspot na telefonu) koriste jedan od dva načina razmene lozinke
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Lozinku nikad ne ispisujemo u log
    ESP_LOGI(TAG, "Povezujem se na mrezu \"%s\"...", ssid);
}
