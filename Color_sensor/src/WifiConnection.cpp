#include <WiFi.h>
#include "Authen.h"
#include "WifiConnection.h"

static t_WifiState Wifi_state = WIFI_INIT;
static t_wifiFailureCallback Wifi_failureCallback = nullptr;

void Wifi_init(void)
{
    Wifi_task();
}

void Wifi_task(void)
{
    switch (Wifi_state)
    {
        case WIFI_INIT:
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            Wifi_state = WIFI_CONNECTING;
            Serial.println("Wifi connecting");
            break;

        case WIFI_CONNECTING:
        {
            static uint8_t waits = 0;

            /* Connection established */
            if (WiFi.status() == WL_CONNECTED)
            {
                Wifi_state = WIFI_CONNECTED;
                waits = 0;
                Serial.println(" Wifi OK");
            }
            /* Connection rejected */
            else if (WiFi.status() == WL_CONNECT_FAILED)
            {
                Wifi_state = WIFI_INIT;
                waits = 0;
                Serial.println(" Wifi failed");
            }
            /* Wait state for connecting/authenticating process */
            else
            {
                waits++;

                Serial.print(">");
                if (!(waits % 10)) Serial.println(""); // Maximum 10 characters per line

                if (waits >= 30)
                {
                    /* Retry after 30 wait cycles */
                    waits = 0;
                    Wifi_state = WIFI_INIT;
                }
            }
            break;
        }

        case WIFI_CONNECTED:
            if ((WiFi.status() == WL_CONNECTION_LOST) || (WiFi.status() == WL_DISCONNECTED))
            {
                Wifi_state = WIFI_INIT;
                Serial.println("Wifi lost");

                if (Wifi_failureCallback != nullptr)
                    Wifi_failureCallback();
            }
            break;

        default:
        	break;
    }
}

void Wifi_setFailureCallback(t_wifiFailureCallback fpCallback)
{
    Wifi_failureCallback = fpCallback;
}

t_WifiState Wifi_getState(void)
{
    return Wifi_state;
}
