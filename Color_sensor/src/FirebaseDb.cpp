#include <Arduino.h>
#include "Time.h"

#include "Lib/Firebase_ESP32_Client/src/FirebaseESP32.h"
// Provide the token generation process info.
#include "Lib/Firebase_ESP32_Client/src/addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "Lib/Firebase_ESP32_Client/src/addons/RTDBHelper.h"

#include "FirebaseDb.h"
#include "Authen.h"

// Firebase Data object
static FirebaseData fbdo;
static FirebaseAuth auth;
static FirebaseConfig config;
static FirebaseJson json;

// Variable to save USER UID
static String uid;

// Database child nodes
static const String colorPath = "/color";
static const String timePath = "/timestamp";
static const String userPath = "/user";
static const String statusPath = "/status";
static const String databasePath = "/Data";

// Parent Node (to be updated before pushing)
static String parentPath;

static t_FirebaseState Firebase_state = FIREBASE_INIT;

static void Database_configLocalTime(void)
{
    const char * ntpServer = "pool.ntp.org";

    /* GMT+7 offset in seconds */
    const long GMT7_sec = 7 * 60 * 60;

    configTime(GMT7_sec, 0, ntpServer);
    Serial.println("Time sync, GMT+7 (Hanoi)");
}

static void Database_getLocalTime(struct tm * localTime)
{
    struct tm * info;

    /* Dummy object used in case function only needs to show local time */
    struct tm dummy;

    if (localTime == nullptr) /* Show only, no return */
        info = &dummy;
    else /* Return local time to a desired address */
        info = localTime;

    (void)getLocalTime(info);

    if (localTime == nullptr) /* Display result */
    {
        int16_t yy = (info->tm_year) + 1900;
        Serial.printf("Local date: %d/%d/%d\n", info->tm_mday, (info->tm_mon) + 1, yy);
        Serial.printf("Local time: %d:%d:%d\n", info->tm_hour, info->tm_min, info->tm_sec);
    }
}

static void Database_init(void)
{
    if (Firebase_state != FIREBASE_INIT) return;

    Database_configLocalTime();

    // Assign the api key (required)
    config.api_key = API_KEY;

    // Assign the user sign in credentials
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // Assign the RTDB URL (required)
    config.database_url = DATABASE_URL;

    // Disable wifi reconnecting, handle manually
    Firebase.reconnectWiFi(false);

    fbdo.setResponseSize(4096);

    // Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback;

    // Assign the maximum retry of token generation
    config.max_token_generation_retry = 5;

    Firebase.begin(&config, &auth);
    Firebase_state = FIREBASE_CONNECTING;
}

void Database_task(void)
{
    switch (Firebase_state)
    {
        case FIREBASE_INIT:
            Database_init();
            break;

        case FIREBASE_CONNECTING:
            if (Firebase.ready())
            {
                Firebase_state = FIREBASE_CONNECTED;
                Serial.println("Db authen OK");
                Database_getLocalTime(nullptr); /* Show local time */
            }
            break;

        case FIREBASE_CONNECTED:
        {
            /* Periodically call Firebase.ready() to handle authentication */
            bool db_ready = Firebase.ready();

            /* Check if token is still valid */
            db_ready &= !Firebase.isTokenExpired(); /* Bit-wise AND applicable here as both functions return boolean type */

            if (!db_ready)
            {
                Firebase_state = FIREBASE_CONNECTING;
                Serial.println("Db reconnecting");
            }
            break;
        }

        default:
            break;
    }
}

void Database_refreshConnection(void)
{
    /* Force token expiration */
    Firebase.refreshToken(&config);
    Serial.println("Db token expired");

    /* Switch connection state */
    Firebase_state = FIREBASE_CONNECTING;
}

t_FirebaseState Database_getState(void)
{
    return Firebase_state;
}

bool Database_pushObjectColor(t_Color * objectColor)
{
    String objectColorValue = String(objectColor->red) + "," + String(objectColor->green) + "," + String(objectColor->blue);

    struct tm timeStamp;
    Database_getLocalTime(&timeStamp);

    String hh = (timeStamp.tm_hour < 10 ? "0" : "") + String(timeStamp.tm_hour);
    String mm = (timeStamp.tm_min < 10 ? "0" : "") + String(timeStamp.tm_min);
    String ss = (timeStamp.tm_sec < 10 ? "0" : "") + String(timeStamp.tm_sec);

    String objectDetectionTime = String(timeStamp.tm_mday) + "/" + String((timeStamp.tm_mon) + 1) + "/" + String((timeStamp.tm_year) + 1900) +
            " " + hh + ":" + mm + ":" + ss;

    /* Get epoch time */
    time_t now;
    time(&now);

    Serial.printf("Db push: (%d,%d,%d)\n",objectColor->red, objectColor->green, objectColor->blue);

    parentPath = databasePath + "/" + String(now);

    json.set(userPath.c_str(), String(""));
    json.set(statusPath.c_str(), String("0"));
    json.set(colorPath.c_str(), objectColorValue);
    json.set(timePath.c_str(), objectDetectionTime);

    return Firebase.RTDB.setJSON(&fbdo, parentPath, &json);
}
