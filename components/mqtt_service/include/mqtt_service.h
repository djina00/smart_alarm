#pragma once

// Poziva se kad sa servera stigne komanda, npr. "arm" ili "disarm"
typedef void (*mqtt_service_command_cb_t)(const char *cmd);

// Vraća kod trenutnog stanja ("DISARMED", "ARMED"...); servis ga šalje
// kao status svaki put kad se poveže na broker
typedef const char *(*mqtt_service_state_cb_t)(void);

// Pravi MQTT klijenta i pokreće task za komunikaciju sa serverom.
// Povezivanje na broker počinje kad uređaj dobije WiFi vezu.
void mqtt_service_init(mqtt_service_command_cb_t on_command, mqtt_service_state_cb_t get_state);

// Prijavljuje serveru detektovan pokret (dok je alarm uključen).
// state_code mora biti stalni tekst (npr. iz alarm_state_code).
void mqtt_service_publish_motion(const char *state_code);

// Prijavljuje serveru novo stanje alarma
void mqtt_service_publish_state(const char *state_code);
