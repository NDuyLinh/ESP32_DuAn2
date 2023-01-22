#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Arduino.h> /* for String, Math */
#include "Lib/US-100/PingSerial.h"

#include "Application.h"
#include "Calib.h"

static Adafruit_TCS34725 colorSensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_120MS, TCS34725_GAIN_4X);
static PingSerial proximitySensor = PingSerial(Serial2, 30, 1200); /* Proximity sensor connected to ESP32 UART2 (GPIO16/17) */
static t_AppState App_state = APP_INIT;
static bool App_overheightCondition = false;
static t_Color App_colorSensorBaseline = {0, 0, 0, 0};
static t_appObjectDetectionCallback App_objectDetectionCallback = nullptr;
static t_appObjectOverheightCallback App_objectOverheightCallback = nullptr;

void App_init(void)
{
    if (!colorSensor.begin())
    {
        Serial.println("Color sensor failed");
        while (true) {} // Halt
    }
    Serial.println("Color sensor OK");

    /* Initialize proximity sensor and request */
    proximitySensor.begin();
    proximitySensor.request_distance();


    App_state = APP_SENSOR_PREPARE;
}

static bool App_calibrateColorSensor(void)
{
    static uint8_t cycles = 0;
    cycles++;

    if (cycles > COLOR_SENSOR_CALIB_CYCLES)
    {
        /* Calibration completed */
        return true;
    }

    float r, g, b;
    colorSensor.getRGB(&r, &g, &b);

    /* Calculate sum of all cycles */
    static uint16_t red = 0, green = 0, blue = 0;
    red   += (uint8_t)r;
    green += (uint8_t)g;
    blue  += (uint8_t)b;

    /* Finally divide the sum by number of cycles */
    if (cycles == COLOR_SENSOR_CALIB_CYCLES)
    {
        App_colorSensorBaseline.red = (uint8_t)(red / COLOR_SENSOR_CALIB_CYCLES);
        App_colorSensorBaseline.green = (uint8_t)(green / COLOR_SENSOR_CALIB_CYCLES);
        App_colorSensorBaseline.blue = (uint8_t)(blue / COLOR_SENSOR_CALIB_CYCLES);
    }

    /* Calibration in progress */
    return false;
}

static void App_readColorSensor(void)
{
    float r, g, b;
    colorSensor.getRGB(&r, &g, &b);

    uint8_t red, green, blue;
    red   = (uint8_t)r;
    green = (uint8_t)g;
    blue  = (uint8_t)b;

    /* Calculate color difference of sensor reading against baseline */
    uint8_t delta_red, delta_green, delta_blue;
    delta_red = abs(red - App_colorSensorBaseline.red);
    delta_green = abs(green - App_colorSensorBaseline.green);
    delta_blue = abs(blue - App_colorSensorBaseline.blue);

    /* Check if an object with color different from baseline is detected */
    bool thereIsObject = ((delta_red > COLOR_OBJECT_DETECTION_THRESHOLD) ||
            (delta_green > COLOR_OBJECT_DETECTION_THRESHOLD) ||
            (delta_blue > COLOR_OBJECT_DETECTION_THRESHOLD));

    /* Cache value for detection status */
    static bool thereWasObject = false;

    /* Process each new object once */
    if (thereIsObject && (!thereWasObject))
    {
        /* Trigger callback for data pushing etc... */
        if (App_objectDetectionCallback != nullptr)
        {
            t_Color newObjColor = {red, green, blue, 0};
            App_objectDetectionCallback(&newObjColor);
        }
    }

    /* Store the status to cache to avoid duplicate processing */
    thereWasObject = thereIsObject;
}

static void App_readProximitySensor(void)
{
    static bool wasOverheight = false;
    static uint8_t overheightDetectionCounter = 0;

    /* Wait until sensor data is available */
    if (proximitySensor.data_available() & DISTANCE)
    {
        uint16_t distance = proximitySensor.get_distance();

        /* Validate sensor reading and reject non-sense values */
        if ((distance > MAXIMUM_DISTANCE) || (distance == 0))
        {
            proximitySensor.request_distance();
            return;
        }

        bool isOverheight = false;

        /* Implement sensor debounce */
        if (distance <= OVERHEIGHT_LIMIT)
        {
            /* Count consecutive times that overheight condition is detected */
            if (overheightDetectionCounter <= OVERHEIGHT_HYSTERESIS)
                overheightDetectionCounter++;
            else
                isOverheight = true;
        }
        else /* Reset counter whenever the condition fails */
        {
            App_overheightCondition = false;
            overheightDetectionCounter = 0;
        }

        /* If overheight condition appears to be stable enough then notify */
        if (isOverheight && (!wasOverheight))
        {
            App_overheightCondition = true;

            if (App_objectOverheightCallback != nullptr)
                App_objectOverheightCallback();

            Serial.println("Overheight detected");
        }

        /* Store status to cache to avoid duplicate notification */
        wasOverheight = isOverheight;

        /* Request new data */
        proximitySensor.request_distance();
    }
}

void App_task(void)
{
    switch (App_state)
    {
        case APP_INIT:
            App_init();
            break;

        case APP_SENSOR_PREPARE:
            /* Wait state for sensors calibration */
            if (App_calibrateColorSensor())
            {
                App_state = APP_WORKING;
                Serial.printf("Color baseline: (%d,%d,%d)\n", App_colorSensorBaseline.red, App_colorSensorBaseline.green, App_colorSensorBaseline.blue);
            }
            break;

        case APP_WORKING:
            App_readProximitySensor();
            App_readColorSensor();
            break;

        default:
            break;
    }
}

void App_setObjectDetectionCallback(t_appObjectDetectionCallback callback)
{
    App_objectDetectionCallback = callback;
}

void App_setObjectOverheightCallback(t_appObjectOverheightCallback callback)
{
    App_objectOverheightCallback = callback;
}

bool App_getOverheightCondition(void)
{
    return App_overheightCondition;
}
