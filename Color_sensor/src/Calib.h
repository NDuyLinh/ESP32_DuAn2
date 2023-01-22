#ifndef CALIB_H
#define CALIB_H

/* Number of read cycles (1-255) to find sensor baseline (avg. value) */
#define COLOR_SENSOR_CALIB_CYCLES 3

/* Detection threshold */
#define COLOR_OBJECT_DETECTION_THRESHOLD 5

/* Overheight limit = 60mm from sensor to object top */
#define OVERHEIGHT_LIMIT 60

/* Overheight hysteresis (consecutive count that overheight condition is detected */
#define OVERHEIGHT_HYSTERESIS 3

/* Maximum distance that makes sense with the hardware set-up */
#define MAXIMUM_DISTANCE 2000

#endif /* CALIB_H */
