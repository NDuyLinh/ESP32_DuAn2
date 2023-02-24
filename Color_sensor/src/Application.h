#ifndef APPLICATION_H
#define APPLICATION_H

#include "Datatypes.h"

/* State of application
 *
 */
typedef enum
{
    APP_INIT = 0,
    APP_SENSOR_PREPARE,
    APP_WORKING
} t_AppState;

/* Function prototype of a callback
 * for object detection
 *
 * input: pointer to a buffer that receives object color
 * output: none
 */
typedef void (* t_appObjectDetectionCallback)(t_Color * objectColor);

/* Set a callback function to be triggered
 * when an object is detected
 *
 * input: pointer to callback function
 *       (to unregister, pass a nullptr)
 * output: none
 */
void App_setObjectDetectionCallback(t_appObjectDetectionCallback callback);

/* Function prototype of a callback
 * for object overheight exception
 *
 */
typedef void (* t_appObjectOverheightCallback)(void);

/* Set a callback function to be triggered
 * when an overheight condition is detected
 *
 * input: pointer to callback function
 *       (to unregister, pass a nullptr)
 * output: none
 */
void App_setObjectOverheightCallback(t_appObjectOverheightCallback callback);

/* Function prototype of a callback
 * for control the motor
 *
 */
typedef void (* t_appMotorCallback)(bool state);

/* Set a callback function used to grant
 * the control of motor to the application
 * so the app can stop the motor for better
 * color reading
 *
 * input: pointer to callback function
 *       (to unregister, pass a nullptr)
 * output: none
 */
void App_setMotorCallback(t_appMotorCallback callback);

/* Initialization procedure for application
 *
 * input: none
 * output: none
 */
void App_init(void);

/* Periodic task handler to perform
 * application job (object color detection)
 * and overheight exception handling
 *
 * input: none
 * output: none
 */
void App_task(void);

/* Get status of overheight condition
 *
 * input: none
 * output: overheight condition, true for overheight, false otherwise
 */
bool App_getOverheightCondition(void);

#endif /* APPLICATION_H */
