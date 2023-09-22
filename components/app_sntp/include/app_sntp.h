typedef void (*on_sntp_cb)(void);

/**
 * Initialize SNTP module
 * @param cb callback function, executed after getting responce from  NTP server
 */
void sntp_start(on_sntp_cb cb);

/**
 * Sync time via SNTP
 * @return nothing
 */
void sync_time(void);
