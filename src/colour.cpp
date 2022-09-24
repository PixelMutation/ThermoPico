#include "colour.h"

// hue is out of 360, saturation and value are out of 1
uint16_t hsvToRGB16(float h, float s, float v) {

    // wrap values outside of 360
	if (h>360) {
		h-=360;
	} else
	if (h<0) {
		h+=360;
	}

    s=constrain(s,0,1);
	v=constrain(v,0,1);

	//this is the algorithm to convert from RGB to HSV
	float r=0;
	float g=0;
	float b=0;

	float hf=h/60.0;

	int i=(int)floor(h/60.0);
	float f = h/60.0 - i;
	float pv = v * (1 - s);
	float qv = v * (1 - s*f);
	float tv = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:
		r = v;
		g = tv;
		b = pv;
		break;
	case 1: 
		r = qv;
		g = v;
		b = pv;
		break;
	case 2:
		r = pv;
		g = v;
		b = tv;
		break;
	case 3:
		r = pv;
		g = qv;
		b = v;
		break;
	case 4:
		r = tv;
		g = pv;
		b = v;
		break;
	case 5: 
		r = v;
		g = pv;
		b = qv;
		break;
	}

	//set each component to a integer value between 0 and 255
	uint8_t red=constrain((uint8_t)255*r,0,255); //5
	uint8_t green=constrain((uint8_t)255*g,0,255); //6
	uint8_t blue=constrain((uint8_t)255*b,0,255); //5

	return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
    // return gfx->color565(red,green,blue);
}


int32_t intmap(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


float tempPaletteMap(uint32_t index, int minTemp, int maxTemp, float min, float max, bool invert) {
	if(invert) {
		std::swap(max,min);
	}
	return (float)intmap(index-(40*TEMP_MULTIPLIER),minTemp*TEMP_MULTIPLIER,maxTemp*TEMP_MULTIPLIER,(long)(min*1000),(long)(max*1000))/1000;
}

void generatePalette(uint16_t * palette, paletteSettings config) {
	// constrain hsv channels to valid ranges
	config.hsvMax[HUE]=constrain(config.hsvMax[HUE],0,360);
	config.hsvMax[SAT]=constrain(config.hsvMax[SAT],0,1);
	config.hsvMax[VAL]=constrain(config.hsvMax[VAL],0,1);

	config.hsvMin[HUE]=constrain(config.hsvMin[HUE],0,360);
	config.hsvMin[SAT]=constrain(config.hsvMin[SAT],0,1);
	config.hsvMin[VAL]=constrain(config.hsvMin[VAL],0,1);

	// wrap hue to be circular. if max < min, it will wrap around.

	if (config.hsvMax[HUE]<config.hsvMin[HUE]) {
		config.hsvMax[HUE]+=360;
	}

	uint32_t scaleFactor[3];
	bool mapChannel[3]={true,true,true};
	for (uint8_t channel =0; channel < 3; channel++) {
		// if it has no range (min=max) then disable mapping.
		if (config.hsvMax[channel]<=config.hsvMin[channel]) {
			mapChannel[channel]=false;
		} 
		// otherwise generate a scale factor to map each channel to its range (similar to map() function)
		else { 
			scaleFactor[channel]=((uint32_t)config.hsvMax[channel] - (uint32_t)config.hsvMin[channel]) / (uint32_t)(config.maxTemp*TEMP_MULTIPLIER - config.minTemp*TEMP_MULTIPLIER) + (uint32_t)config.hsvMin[channel];
		}
	}
	(config.maxTemp - config.minTemp)+config.hsvMin;
	// for each possible temperature
	for (uint32_t i = 0 ; i < PALETTE_SIZE; i++) {
		// if it is above the configured range, set it to the overtemp colour
		if (i/TEMP_MULTIPLIER-TEMP_OFFSET>config.maxTemp) {
			// palette[i]=OVERTEMP;
		} else
		// if it is below the configured range, set it to the undertemp colour
		if (i/TEMP_MULTIPLIER-TEMP_OFFSET<config.minTemp) {
			// palette[i]=UNDERTEMP;
		}
		// otherwise, map the temperature onto the configured ranges for hue, saturation and value
		else {
			float hsv [3];
			// apply the mapping function, which reverses the range if desired.
			
			for (uint8_t channel =0; channel < 3; channel++) {
				// if max=min, the value won't change so just use max
				if (!mapChannel[channel]) {
					hsv[channel]=config.hsvMax[channel];
				} else
				{ // otherwise, map the temperature onto the hsv channel using the given range
					hsv[channel]=tempPaletteMap(i,config.minTemp,config.maxTemp,config.hsvMin[channel],config.hsvMax[channel],config.hsvInvert[channel]);
					// hsv[channel]=(float)((int)(i-(TEMP_OFFSET*TEMP_MULTIPLIER))-config.minTemp*TEMP_MULTIPLIER) * scaleFactor[channel];
				}
			}
			// convert the hsv colour into a 16 bit rgb colour and add it to the palette
			palette[i]=hsvToRGB16 (hsv[HUE],hsv[SAT],hsv[VAL]);
		}
	}
}

// temp must be passed as an integer scaled up by 100x
uint16_t tempToColor (int temperature, uint16_t * palette) {
	return	 palette[(uint32_t)temperature+(TEMP_OFFSET*TEMP_MULTIPLIER)]; // shift to be palette index
}