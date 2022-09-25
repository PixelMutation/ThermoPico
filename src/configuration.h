#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <Adafruit_MLX90640.h>

#define MIN_TEMP 25
#define MAX_TEMP 38

#define OVERTEMP BLACK
#define UNDERTEMP WHITE

#define MAX_SPOT RED
#define MIN_SPOT CYAN
#define CENTRE_SPOT GREEN

#define FILTERING 100 // 0-100. lower increases filtering which reduces noise but adds latency
#define RESOLUTION 1 // 1 is 0.01C, 100 is 1C

#define TEMP_RANGE 340 // total range of the sensor (-40 to 300C)
#define TEMP_MULTIPLIER 100 // Temperatures are stored as unsigned integers to reduce float processing. A value of 100 means temperature is stored to the nearest 0.01C
#define TEMP_OFFSET 40 // we store temperatures as unsigned integers, since the minimum is -40 we must add 40
#define TEMP_OFFSET_MULTIPLIED (TEMP_OFFSET * TEMP_MULTIPLIER)

#define PALETTE_SIZE (TEMP_RANGE * TEMP_MULTIPLIER)

#define REFRESH MLX90640_64_HZ // 32Hz is stable, 64Hz not yet. Lower values reduce noise but obviously add lag.

#endif