idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_wifi app_sntp driver
    PRIV_REQUIRES nvs_flash freertos)
