#ifndef COLOUR_H
#define COLOUR_H

#include <Arduino.h>

#include "configuration.h"


enum hsvChannel {
	HUE,SAT,VAL
};
// Palettes are generated given a range of temperatures to map to the ranges of hue, saturation and value
// Separate palettes can be tied to different temperature ranges, such that both a high and low temperature object
// can be displayed with adequate resolution
struct paletteSettings {
	int minTemp=-40;
	int maxTemp=300;

	float hsvMax[3]= {360,1,1};
	float hsvMin[3]= {0,1,1};

	bool hsvInvert[3]={false,false,false};
};

int32_t intmap(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);
float tempPaletteMap(uint32_t index, int minTemp, int maxTemp, float min, float max, bool invert=false);
uint16_t hsvToRGB16(float h, float s=1, float v=1);
void generatePalette(uint16_t * palette, paletteSettings config);
uint16_t tempToColor (int temperature, uint16_t * palette);

enum colourMode {COLOUR_16, COLOUR_18};

class palette {
private:
    uint16_t colourPalette [PALETTE_SIZE]={0};
    std::vector<paletteSettings> palettes;
public:
    palette();
    void addPalette(paletteSettings); // overwrite the palette in the given temperature region with the given settings
    void removePalette(); //TODO
    uint16_t tempToColor16(); // 565 colour
    uint32_t tempToColor18(); // 666 colour, top 14 bits are empty!
};






















#endif
