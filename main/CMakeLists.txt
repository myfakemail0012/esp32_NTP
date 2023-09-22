idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_wifi app_sntp
    PRIV_REQUIRES nvs_flash)
