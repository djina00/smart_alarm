#pragma once

// Komande za sirenu (zujalicu)
typedef enum {
    SIREN_CMD_OFF,         // ugasi sirenu
    SIREN_CMD_ALARM,       // sirena radi isprekidano dok se ne ugasi
    SIREN_CMD_BEEP_ONCE,   // jedan kratak pisak (alarm uključen)
    SIREN_CMD_BEEP_TWICE,  // dva kratka piska (alarm isključen)
} siren_cmd_t;

// Podešava zujalicu i pokreće task sirene
void siren_init(void);

// Šalje komandu tasku sirene; nova komanda prekida prethodnu
void siren_command(siren_cmd_t cmd);
