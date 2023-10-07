if(CONFIG_APP_BLE_DRIVER_NIMBLE)
    set(SOURCE "app_ble_nimble.c")
else()
    set(SOURCE "app_ble_bluedroid.c")
endif()

idf_component_register(SRCS ${SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash bt)
