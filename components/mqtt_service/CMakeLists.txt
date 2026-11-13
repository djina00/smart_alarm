idf_component_register(SRCS "mqtt_service.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES mqtt json esp_event esp_netif)
