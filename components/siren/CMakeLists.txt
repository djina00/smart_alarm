idf_component_register(SRCS "siren.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio
                       PRIV_REQUIRES buzzer)
