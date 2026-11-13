#pragma once

// Poziva se kad sa servera stigne komanda, npr. "arm" ili "disarm"
typedef void (*mqtt_command_cb_t)(const char *cmd);

// Priprema MQTT klijenta; povezivanje na broker počinje kad uređaj
// dobije IP adresu na WiFi mreži
void mqtt_service_init(mqtt_command_cb_t on_command);

// Šalje serveru događaj: type je "motion" ili "state",
// state je kod stanja ("DISARMED", "ARMED", "TRIGGERED")
void mqtt_service_publish(const char *type, const char *state);
