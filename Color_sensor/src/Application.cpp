#include <Wire.h>
#include <Arduino.h> /* for String, Math */

#include "Lib/Adafruit_TCS34725/Adafruit_TCS34725.h"
#include "Lib/US-100/PingSerial.h"

#include "Application.h"
#include "Calib.h"

/* Sensor objects */
static Adafruit_TCS34725 colorSensor = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_24MS, TCS34725_GAIN_4X);
static PingSerial proximitySensor = PingSerial(Serial2, MINIMUM_DISTANCE, MAXIMUM_DISTANCE); /* Proximity sensor connected to ESP32 UART2 (GPIO16/17) */

/* Application static variables */
static t_AppState App_state = APP_INIT;
static bool App_overheightCondition = true; /* Initialize to true to avoid motor spurious start */
static t_Color App_colorSensorBaseline = {0, 0, 0, 0};
static uint16_t App_proximitySensorBaseline = 0;
static t_appObjectDetectionCallback App_objectDetectionCallback = nullptr;
static t_appObjectOverheightCallback App_objectOverheightCallback = nullptr;
static t_appMotorCallback App_motorCallback = nullptr;

/* Coupling between state machine
 * of proximity and color sensor
 */
typedef enum {
    OBJECT_NOT_DETECTED = 0,
    OBJECT_DETECTED_BY_DISTANCE,
    OBJECT_DETECTED_BY_COLOR
} t_objectDetectionState;

static t_objectDetectionState App_detectionState = OBJECT_NOT_DETECTED;

void App_init(void)
{
    if (!colorSensor.begin())
    {
        Serial.println("Color sensor failed");
        while (true) {} // Halt
    }
    Serial.println("Color sensor OK");

    /* Initialize and calibrate proximity sensor */
    proximitySensor.begin();
    proximitySensor.request_distance();

    App_state = APP_SENSOR_PREPARE;
}

static bool App_calibrateProximitySensor(void)
{
    static uint8_t cycles = 0;
    static uint16_t min = 0, max = 0;

    if (cycles >= PROXIMITY_SENSOR_CALIB_CYCLES)
    {
        /* Calibration completed */
        return true;
    }

    if (proximitySensor.data_available() & DISTANCE)
    {
        uint16_t distance = proximitySensor.get_distance();

        /* Initial values for first cycle */
        if (cycles == 0)
            max = min = distance;

        if (distance > max)
            max = distance;

        if (distance < min)
            min = distance;

        cycles++;
        proximitySensor.request_distance();
    }
    else /* Wait for data */
        return false;

    /* Finally get the avg. value between min and max */
    if (cycles == PROXIMITY_SENSOR_CALIB_CYCLES)
    {
        App_proximitySensorBaseline = ((min + max) / 2);
        return true;
    }

    /* Calibration in progress */
    return false;
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

static void App_setMotor(bool state)
{
    if (App_motorCallback != nullptr)
        App_motorCallback(state);
}

static void App_readColorSensor(void)
{
    static enum {
        COLOR_NO_OBJECT = 0,
        COLOR_NEW_OBJECT,
        COLOR_OBJECT_REGISTERED
    } objectColorDetectionState = COLOR_NO_OBJECT;

    static uint8_t objectColorDetectionCounter = 0;

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

    bool expectingObject = (App_detectionState == OBJECT_DETECTED_BY_DISTANCE);

    // Serial.printf("Color object detected/expecting: %d/%d\n", thereIsObject, expectingObject);

    switch (objectColorDetectionState)
    {
        case COLOR_NO_OBJECT:
            if (thereIsObject && expectingObject)
            {
                if (objectColorDetectionCounter < OBJECT_COLOR_DETECTION_HYSTERESIS)
                    objectColorDetectionCounter++;
                else
                    objectColorDetectionState = COLOR_NEW_OBJECT;
            }
            else
                objectColorDetectionCounter = 0;
            break;

        case COLOR_NEW_OBJECT:
        {
            // Serial.println("Registering new object color");

            App_setMotor(false);

            float rx, gx, bx;
            colorSensor.setIntegrationTime(TCS34725_INTEGRATIONTIME_199MS);
            colorSensor.getRGB(&rx, &gx, &bx);
            colorSensor.setIntegrationTime(TCS34725_INTEGRATIONTIME_24MS);

            if (App_objectDetectionCallback != nullptr)
            {
                t_Color newObjColor = {(uint8_t)rx, (uint8_t)gx, (uint8_t)bx, 0};
                App_objectDetectionCallback(&newObjColor);
            }

            App_setMotor(true);

            objectColorDetectionState = COLOR_OBJECT_REGISTERED;
            break;
        }

        case COLOR_OBJECT_REGISTERED:
            if (!thereIsObject)
            {
                if (objectColorDetectionCounter > 0)
                    objectColorDetectionCounter--;
                else
                {
                    objectColorDetectionState = COLOR_NO_OBJECT;
                    App_detectionState = OBJECT_DETECTED_BY_COLOR;
                }
            }
            else
                objectColorDetectionCounter = OBJECT_COLOR_DETECTION_HYSTERESIS;
            break;

        default:
            break;
    }
}

static void App_readProximitySensor(void)
{
    static enum {
        PROXIMITY_INVALID = 0,
        PROXIMITY_IN_RANGE,
        PROXIMITY_PENDING_OVERHEIGHT,
        PROXIMITY_OVERHEIGHT
    } proximityState = PROXIMITY_INVALID;

    static enum {
        PROXIMITY_IN_RANGE_NO_OBJECT = 0,
        PROXIMITY_IN_RANGE_NEW_OBJECT
    } objectDistanceDetectionState = PROXIMITY_IN_RANGE_NO_OBJECT;

    static uint8_t overheightDetectionCounter = 0;
    static uint8_t distanceDetectionCounter = 0;

    uint16_t distance = 0;

    if (proximitySensor.data_available() & DISTANCE)
        distance = proximitySensor.get_distance();
    else /* Wait for sensor data */
        return;

    // Serial.printf("Distance: %d\n", distance);

    bool distanceValid = ((distance >= MINIMUM_DISTANCE) && (distance <= MAXIMUM_DISTANCE));
    bool distanceInRange = (distance >= OVERHEIGHT_LIMIT);
    bool objectDetected = (distance < (App_proximitySensorBaseline - PROXIMITY_SENSOR_NOISE));

    // Serial.printf("Distance baseline/actual: %d/%d\n", App_proximitySensorBaseline, distance);

    switch (proximityState)
    {
        case PROXIMITY_OVERHEIGHT:
        case PROXIMITY_INVALID:
            App_overheightCondition = true;

            /* Exit path */
            if (distanceValid)
            {
                /* Wait state before transition to 'in range' */
                if (distanceInRange)
                {
                    if (overheightDetectionCounter > 0)
                        overheightDetectionCounter--;
                    else
                        proximityState = PROXIMITY_IN_RANGE;
                }
                else
                    overheightDetectionCounter = OVERHEIGHT_HYSTERESIS;
            }
            /* Require some stable readings before leaving */
            else
                overheightDetectionCounter = OVERHEIGHT_HYSTERESIS;
            break;

        case PROXIMITY_IN_RANGE:
            App_overheightCondition = false;
            overheightDetectionCounter = 0;

            /* Child state machine */
            switch (objectDistanceDetectionState)
            {
                case PROXIMITY_IN_RANGE_NO_OBJECT:
                    App_detectionState = OBJECT_NOT_DETECTED;

                    if (objectDetected)
                    {
                        if (distanceDetectionCounter < OBJECT_DISTANCE_DETECTION_HYSTERESIS)
                            distanceDetectionCounter++;
                        else
                        {
                            App_detectionState = OBJECT_DETECTED_BY_DISTANCE;
                            objectDistanceDetectionState = PROXIMITY_IN_RANGE_NEW_OBJECT;
                        }
                    }
                    else
                        distanceDetectionCounter = 0;
                    break;

                case PROXIMITY_IN_RANGE_NEW_OBJECT:
                    distanceDetectionCounter = 0;

                    if (App_detectionState == OBJECT_DETECTED_BY_COLOR)
                        objectDistanceDetectionState = PROXIMITY_IN_RANGE_NO_OBJECT;
                    break;
            }

            /* Exit path, priority */
            if (!distanceValid)
            {
                overheightDetectionCounter = OVERHEIGHT_HYSTERESIS;
                proximityState = PROXIMITY_INVALID;
                break;
            }

            /* Exit path */
            if (!distanceInRange)
                proximityState = PROXIMITY_PENDING_OVERHEIGHT;
            break;

        case PROXIMITY_PENDING_OVERHEIGHT:
            /* Exit path, priority */
            if (!distanceValid)
            {
                overheightDetectionCounter = OVERHEIGHT_HYSTERESIS;
                proximityState = PROXIMITY_INVALID;
                break;
            }

            /* Wait state before transition to 'overheight' */
            if (!distanceInRange)
            {
                if (overheightDetectionCounter < OVERHEIGHT_HYSTERESIS)
                    overheightDetectionCounter++;

                /* Exit path */
                else
                {
                    proximityState = PROXIMITY_OVERHEIGHT;
                    if (App_objectOverheightCallback != nullptr)
                        App_objectOverheightCallback();
                }
            }
            else /* Fall back to 'in range' if the reading was only a spike */
            {
                proximityState = PROXIMITY_IN_RANGE;
            }

            break;

        default:
            break;
    }

    // Serial.printf("new state: %d\n", proximityState);

    proximitySensor.request_distance();
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
            if (!App_calibrateProximitySensor())
            {
                break;
            }

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

void App_setMotorCallback(t_appMotorCallback callback)
{
    App_motorCallback = callback;
}

bool App_getOverheightCondition(void)
{
    return App_overheightCondition;
}
