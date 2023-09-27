idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_wifi app_sntp driver button_events app_ble
    PRIV_REQUIRES nvs_flash freertos)
