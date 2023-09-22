#pragma once
// WIFI:T:WPA;P:93869164676756534213;S:MagentaWLAN-534A;H:false;
#define WIFI_SSID     CONFIG_SSID
//"MagentaWLAN-534A"
// CONFIG_SSID
#define WIFI_PASSWORD CONFIG_PASSWORD
//"93869164676756534213"
// CONFIG_PASSWORD

typedef void (*on_ip_cb)(void);

/**
 * Initialize WiFi module
 * @param ip_cb call back function, executed after connection established
 */
void wifi_init(on_ip_cb ip_cb);

/**
 * Connect to WiFi network
 */
void wifi_connect(void);