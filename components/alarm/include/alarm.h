#pragma once

// Događaji koje prima logika alarma
typedef enum {
    ALARM_EVENT_MOTION,   // PIR je video pokret
    ALARM_EVENT_BUTTON,   // dugme: uključi ako je isključen, inače isključi
    ALARM_EVENT_ARM,      // komanda sa servera: uključi
    ALARM_EVENT_DISARM,   // komanda sa servera: isključi
} alarm_event_t;

// Stanja alarma
typedef enum {
    ALARM_STATE_DISARMED,   // isključen: pokret se ignoriše
    ALARM_STATE_ARMED,      // uključen: čeka pokret
    ALARM_STATE_TRIGGERED,  // alarm: sirena radi
} alarm_state_t;

// Poziva se kad se desi nešto što treba javiti serveru:
// type je "motion" (pokret dok je alarm uključen) ili "state" (novo stanje)
typedef void (*alarm_report_cb_t)(const char *type, alarm_state_t state);

// Pokreće task alarma. Zujalica mora biti podešena pre poziva.
void alarm_init(alarm_report_cb_t on_report);

// Šalje događaj alarmu (iz bilo kog taska)
void alarm_post_event(alarm_event_t event);

// Kod stanja za server: "DISARMED", "ARMED" ili "TRIGGERED"
const char *alarm_state_code(alarm_state_t state);
