#ifndef CALIB_H
#define CALIB_H

/* Number of read cycles (1-255) to find sensor baseline (avg. value) */
#define COLOR_SENSOR_CALIB_CYCLES 3
#define PROXIMITY_SENSOR_CALIB_CYCLES 15

/* Detection threshold in color */
#define COLOR_OBJECT_DETECTION_THRESHOLD 10

/* Overheight limit in millimetres from sensor to object top */
#define OVERHEIGHT_LIMIT 30

/* Overheight hysteresis (consecutive count that overheight condition is detected) */
#define OVERHEIGHT_HYSTERESIS 3

/* Color change hysteresis (consecutive count that color reading is different from baseline) */
#define OBJECT_COLOR_DETECTION_HYSTERESIS 6

/* Distance change hysteresis (consecutive count that distance reading is different from baseline) */
#define OBJECT_DISTANCE_DETECTION_HYSTERESIS 6

/* Maximum different between sensor readings when input remain unchanged */
#define PROXIMITY_SENSOR_NOISE 16

/* Distance values that make sense with the hardware set-up */
#define MINIMUM_DISTANCE 20
#define MAXIMUM_DISTANCE 1200

#endif /* CALIB_H */
