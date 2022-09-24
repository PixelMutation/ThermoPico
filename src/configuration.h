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

#define FILTERING 40

#define TEMP_RANGE 340
#define TEMP_MULTIPLIER 50 // gives 0.01C resolution (depending on palette type)
#define TEMP_OFFSET 40 // we store temperatures as unsigned integers so add 40
#define TEMP_OFFSET_MULTIPLIED (TEMP_OFFSET * TEMP_MULTIPLIER)

#define PALETTE_SIZE (TEMP_RANGE * TEMP_MULTIPLIER)

#define REFRESH MLX90640_32_HZ

#endif