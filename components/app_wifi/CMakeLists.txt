if(CONFIG_APP_WIFI)
    set(SOURCE "app_wifi.c")
    set(REQ "esp_wifi")
else()
    set(SOURCE "")
    set(REQ "")
endif()

idf_component_register(SRCS ${SOURCE}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash ${REQ})
