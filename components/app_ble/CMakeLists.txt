set(REQ "bt")

if(!CONFIG_APP_BLE)
    set(SOURCE "")
    set(REQ "")
endif()

if(CONFIG_APP_BLE AND CONFIG_APP_BLE_DRIVER_NIMBLE)
    set(SOURCE "app_ble_nimble.c")
endif()

if(CONFIG_APP_BLE AND CONFIG_APP_BLE_DRIVER_BLUEDROID)
    set(SOURCE "app_ble_bluedroid.c")
endif()

idf_component_register(SRCS ${SOURCE}
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash ${REQ})
