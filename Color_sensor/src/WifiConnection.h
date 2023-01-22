#ifndef WIFICONNECTION_H
#define WIFICONNECTION_H

/* Connection state for WiFi
 *
 */
typedef enum
{
    WIFI_INIT = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED
} t_WifiState;

/* Function prototype of a callback
 * for WiFi failure event
 */
typedef void (* t_wifiFailureCallback)(void);

/* Initialization procedure for WiFi
 *
 * input: none
 * output: none
 */
void Wifi_init(void);

/* Periodic task handler to manage
 * connection, failure detection
 * and reconnecting of WiFi
 *
 * input: none
 * output: none
 */
void Wifi_task(void);

/* Set a callback function to be triggered
 * when WiFi failure is detected
 *
 * input: pointer to callback function
 *       (to unregister, pass a nullptr)
 * output: none
 */
void Wifi_setFailureCallback(t_wifiFailureCallback fpCallback);

/* Get status of WiFi connection
 *
 * input: none
 * output: connection state
 */
t_WifiState Wifi_getState(void);

#endif /* WIFICONNECTION_H */
