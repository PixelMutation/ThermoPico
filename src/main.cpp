#include <Ewma.h>
#include <EwmaT.h>


#include <Adafruit_MLX90640.h>

#include <Arduino_DataBus.h>
#include <Arduino_G.h>
#include <Arduino_GFX.h>
#include <Arduino_GFX_Library.h>
#include <Arduino_TFT.h>
#include <Arduino_TFT_18bit.h>
#include <gfxfont.h>

#include <SPI.h>

#include "pico/util/queue.h"
#include "pico/multicore.h"

#include "colour.h"
#include "configuration.h"


#define TFT_CS				1
#define TFT_DC				 4
#define BACKLIGHT				5


#define TFT_MOSI 3	// Data out
#define TFT_SCLK 2	// Clock out

Arduino_DataBus *bus;
Arduino_GFX *gfx;


// OPTION 2 lets you interface the display using ANY TWO or THREE PINS,
// tradeoff being that performance is not as fast as hardware SPI above.
Arduino_RPiPicoSPI * spibus;
Arduino_ST7789 * display; //TODO change library to use 18 bit mode, upscale from 16 unless directly set. gives better image.
Adafruit_MLX90640 * mlx;

uint16_t colorPalette [PALETTE_SIZE];

queue_t frameQueue;

struct frameContainer {
	int32_t frame[32*24];
};

bool frameReady=false;
bool screenReady=false;

void camLoop() {
	frameReady=false;
	static int32_t prevFrame	[32*24];
	const int32_t alpha = 90;
	const int32_t alphaScale = 100;

	// static EwmaT <int32_t> filter[32*24] = {EwmaT<int32_t>(70,100)};	// EWMA filter which can be applied to the pixels to reduce high frequency noise. set out of 100, with lower values increasing smoothing
	static frameContainer bufferIn; // the most recent frame from the camera, which is then loaded into the queue
	static float floatFrame[32*24]; // contains the image in float format
	static uint16_t rawFrame[32*24];
	// read image and load into float buffer

	// mlx->getFrame(floatFrame);
	// Serial.print("Get half frame begin");
	mlx->getHalfFrame(floatFrame);
	// mlx->getHalfImage(floatFrame);

	// mlx->MLX90640_GetFrameData(0x33,rawFrame);

	// convert image to int to make processing faster
	for (uint i=0; i < 32*24; i++) {
		bufferIn.frame[i]=(int32_t)floatFrame[i]*TEMP_MULTIPLIER;
		// bufferIn.frame[i]=(int32_t)rawFrame[i]*TEMP_MULTIPLIER;
		// bufferIn.frame[i]=filter[i].filter((int)floatFrame[i]*TEMP_MULTIPLIER)); // frame passed through a filter
		// bufferIn.frame[i] = (alpha * ((int32_t)floatFrame[i]*TEMP_MULTIPLIER) + (alphaScale - alpha) * prevFrame[i]) / alphaScale;
		// prevFrame[i]=bufferIn.frame[i];
	}
	// send into the queue. if the queue is full, empty it
	static frameContainer binnedFrame; //
	if (queue_is_full(&frameQueue)) {
		queue_remove_blocking(&frameQueue, &binnedFrame);
	}
	queue_try_add(&frameQueue, &bufferIn);
}

void camInit() {
	Wire1.setSDA(26);
	Wire1.setSCL(27);
	Wire1.setClock(1000000);
	Wire1.begin();

	mlx = new Adafruit_MLX90640;

	mlx->begin(0x33, &Wire1);
	Wire1.setClock(2000000);
	mlx->setMode(MLX90640_CHESS);
	mlx->setResolution(MLX90640_ADC_19BIT);
	mlx->setRefreshRate(REFRESH);
}


void screenInit() {
	SPI.setSCK(TFT_SCLK);
	SPI.setTX(TFT_MOSI);


	spibus= new Arduino_RPiPicoSPI(TFT_DC , TFT_CS , TFT_SCLK , TFT_MOSI , 7 , spi0 );
	bus=spibus;
	display = new Arduino_ST7789(bus, 7, 3, true );
	gfx = display;


	gfx->begin(40000000);
	gfx->setRotation(1);
	clock_configure(clk_peri,0,CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,clock_get_hz(clk_sys),clock_get_hz(clk_sys));
	
	// for (uint8_t i =0; i < 10; i++) {
	// 	Serial.println(clock_get_hz(clk_sys));
	// 	Serial.println(clock_get_hz(clk_peri));
	// 	Serial.println(spi_set_baudrate(spi0,100000000));
	// 	delay(1000);
	// }
	spi_set_baudrate(spi0,100000000);

	// display->setRotation(3);

	pinMode(BACKLIGHT,OUTPUT);
	analogWrite(BACKLIGHT,100);

	paletteSettings config;

	config.hsvMin[HUE]=0;
	config.hsvMin[SAT]=0.5;
	config.hsvMin[VAL]=1;

	config.hsvMax[HUE]=360;
	config.hsvMax[SAT]=1;
	config.hsvMax[VAL]=1;

	config.hsvInvert[HUE]=false;

	config.minTemp=15;
	config.maxTemp=37;

	generatePalette(colorPalette,config);

	for (uint index=0; index < 240; index ++) {
		gfx->drawLine(index,188,index,198,colorPalette[uint((float)index/(0.7/TEMP_MULTIPLIER))]);
	}
	uint8_t prevXpos=0;
	for (int64_t index=MIN_TEMP*TEMP_MULTIPLIER; index<MAX_TEMP*TEMP_MULTIPLIER; index++) {
		int xpos = map(index,MIN_TEMP*TEMP_MULTIPLIER,MAX_TEMP*TEMP_MULTIPLIER,0,240);
		if (xpos!=prevXpos) {
			gfx->drawLine(xpos,199,xpos,208,colorPalette[(uint)index+(40*TEMP_MULTIPLIER)]);
		}
		prevXpos=xpos;
	}
	frameReady=true;
}


void screenLoop() {
	screenReady=false;
	
	static uint8_t frameCount=0;
	static long startTime=0;
	static float fps=0;

	static int32_t prevFrame	[32*24];
	const int32_t alpha = FILTERING;
	const int32_t alphaScale = 100;

	static frameContainer bufferOut; // the most recent frame taken from the queue
	queue_try_remove(&frameQueue,&bufferOut); // read latest image from FIFO

	if (millis()-startTime>1000) {
		startTime=millis();
		fps=(float)frameCount/1;
		frameCount=0;
	}
	frameCount++;
	Serial.print("fps=");Serial.println(fps);

	int colorTemp;
	int32_t max=-TEMP_OFFSET*TEMP_MULTIPLIER;
	int32_t min=TEMP_RANGE-TEMP_OFFSET*TEMP_MULTIPLIER;

	uint8_t maxW, maxH;
	uint8_t minW, minH;

	uint8_t maxX, maxY;
	uint8_t minX, minY;

	int32_t centre=bufferOut.frame[12*32 + 16];

	uint16_t bitmap[32*24]={0};
	uint16_t scaledBitmap[240*90]={0};


	// Generate bitmap using color palette


	for (uint8_t h=0; h<24; h++) {
		for (uint8_t w=0; w<32; w++) {
			int32_t temp = bufferOut.frame[24*32-(h*32 + w)];
			// use floor division to 'round'  to resolution value, reduces noise from flickering
			temp/=RESOLUTION;
			temp*=RESOLUTION;
			temp = (alpha * (temp) + (alphaScale - alpha) * prevFrame[h*32 + w]) / alphaScale; // apply EWMA filter
			prevFrame[h*32 + w]=temp; // update previous value for next use of EWMA filter
			
			// temp = filter[h*32 + w]->filter(temp);
			bitmap[h*32 + w] = tempToColor(temp,colorPalette);
			// bitmap[h*32 + w] =bufferOut.frame[h*32 + w]/TEMP_MULTIPLIER;
			if (temp>max) {
				max=temp;
				maxW=w;
				maxH=h;
			} else
			if (temp<min) {
				min=temp;
				minW=w;
				minH=h;
			}
		}
	}


	for (uint8_t h=0; h<90; h++) {
		for (uint8_t w=0; w<240; w++) {
			scaledBitmap[h*240 + w]=bitmap[(h/8)*32 + w/8];
			// scaledBitmap[h*240 + w]=(uint16_t)(bufferOut.frame[(h/8)*32 + w/8]/TEMP_MULTIPLIER);
		}
	}
	display->startWrite();
	display->writeAddrWindow(0, 0, 240, 90);
	display->endWrite();
	// display->draw24bitRGBBitmap()

	spibus->beginWrite();
	spibus->writePixelsFast(scaledBitmap,240*90);
	spibus->endWrite();

	for (uint8_t h=90; h<180; h++) {
		for (uint8_t w=0; w<240; w++) {
			scaledBitmap[(h-90)*240 + w]=bitmap[(h/8)*32 + w/8];
			// scaledBitmap[h*240 + w]=(uint16_t)(bufferOut.frame[(h/8)*32 + w/8]/TEMP_MULTIPLIER);
		}
	}
	display->startWrite();
	display->writeAddrWindow(0, 90, 240, 180);
	display->endWrite();
	// display->draw24bitRGBBitmap()

	spibus->beginWrite();
	spibus->writePixelsFast(scaledBitmap,240*90);
	spibus->endWrite();

	// for (uint8_t h=0; h<90; h++) {
	//	 for (uint8_t w=0; w<120; w++) {
	//		 scaledBitmap[h*120 + w]=bitmap[(h/4)*32 + w/4];
	//	 }
	// }

// for (uint i =0; i < 32*24; i++) {
// bitmap[i]=(uint16_t)bufferOut.frame[i]/TEMP_MULTIPLIER;
// }


	// draw the image
	// gfx->draw16bitRGBBitmap(0,0,scaledBitmap,240,180);
	// gfx->draw16bitRGBBitmap(0,0,scaledBitmap,120,90);

	display->startWrite();
	display->writeAddrWindow(0, 0, 240, 180);
	display->endWrite();
	// display->draw24bitRGBBitmap()

	spibus->beginWrite();
	spibus->writePixelsFast(scaledBitmap,200*180);
	spibus->endWrite();

// uint16_t bottom[240*90];
// for (uint i =0; i < 240*90; i++) {
//	 bottom[i]=scaledBitmap[i+240*90];
// }
//	 display->startWrite();
//	 display->writeAddrWindow(0, 90, 240, 180);
//	 display->endWrite();
//	 spibus->beginWrite();
//	 spibus->writePixelsFast(bottom,240*90);
//	 spibus->endWrite();

	// gfx->draw16bitRGBBitmap(0,0,bitmap,32,24);

	
	gfx->setCursor(0,0);
	gfx->print(fps);gfx->print("fps");

// clear the bottom part of the image

	gfx->fillRect(0,180,239,10,BLACK);
	gfx->fillRect(0,209,239,30,BLACK);

	maxX=maxW*7.5;
	maxY=maxH*7.5;
	minX=minW*7.5;
	minY=minH*7.5;
	// highlight the max min and centre points
	gfx->fillRect(maxX,maxY,3,3,MAX_SPOT);
	gfx->fillRect(minX,minY,3,3,MIN_SPOT);
	gfx->fillRect(120,90,3,3,CENTRE_SPOT);
	// show the max min and centre points on the scale

	uint8_t maxScaleX = (max/TEMP_MULTIPLIER+TEMP_OFFSET)*0.7;
	uint8_t minScaleX = (min/TEMP_MULTIPLIER+TEMP_OFFSET)*0.7;
	uint8_t centreScaleX = (centre/TEMP_MULTIPLIER+40)*0.7;

	gfx->drawLine(maxScaleX,180,maxScaleX,189,MAX_SPOT);
	gfx->drawLine(minScaleX,180,minScaleX,189,MIN_SPOT);
	gfx->drawLine(centreScaleX,180,centreScaleX,189,CENTRE_SPOT);

	gfx->drawLine(maxX,211,maxX,219,MAX_SPOT);
	gfx->drawLine(minX,211,minX,219,MIN_SPOT);
	gfx->drawLine(centre,211,centre,219,CENTRE_SPOT);


	gfx->setCursor(0,225);
	gfx->setTextColor(MAX_SPOT);
	gfx->print("Max"); gfx->setTextSize(2); gfx->print((float)max/TEMP_MULTIPLIER); gfx->setTextSize(1);
	gfx->setTextColor(CENTRE_SPOT);
	gfx->print("Mid"); gfx->setTextSize(2); gfx->print((float)centre/TEMP_MULTIPLIER); gfx->setTextSize(1);
	gfx->setTextColor(MIN_SPOT);
	gfx->print("Min"); gfx->setTextSize(2); gfx->print((float)min/TEMP_MULTIPLIER); gfx->setTextSize(1);



	gfx->setTextColor(WHITE);
	gfx->setTextSize(1);
	for (uint xpos=0; xpos<240; xpos+=30) {
		gfx->setCursor(xpos,180);
		gfx->print(map(xpos,0,240,-40,300));
	}
	for (uint xpos=0; xpos<240; xpos+=30) {
		gfx->setCursor(xpos,209);
		gfx->print(map(xpos,0,240,MIN_TEMP,MAX_TEMP));
	}
	gfx->setTextSize(1);

	screenReady=true;
}

bool core0interrupt(struct repeating_timer *t) {
	screenLoop();
	return true;
}

bool core1interrupt(struct repeating_timer *t) {
	camLoop();
	return true;
}

void core1_entry() {
	camInit();

	alarm_pool_t *core1 ;
	core1 = alarm_pool_create(2, 16) ;

	struct repeating_timer core1Timer;

	alarm_pool_add_repeating_timer_ms(core1, 50, core1interrupt, NULL, &core1Timer);
	// while (true) {
	//	 // core1_loop
	// }
}
void core0_entry() {
	screenInit();

	struct repeating_timer core0Timer;

	add_repeating_timer_ms(50,	core0interrupt, NULL, &core0Timer);

	while (true) {
		//core0_loop
	}
}

void setup() {
	// pinMode(20,OUTPUT);
	// pinMode(19,OUTPUT);
	Serial.begin(9600);
	// spinlock_num_count = spin_lock_claim_unused(true) ;
	// spinlock_count = spin_lock_init(spinlock_num_count) ;

	// this threadsafe FIFO passes images between cores. there are 3 available spaces to allow the cores to be out of sync.
	queue_init(&frameQueue, sizeof(frameContainer), 1);

	// multicore_launch_core1(screen);
	// camera();

	// multicore_launch_core1(camera);
	// screen();

	screenInit();
	camInit();


	// multicore_launch_core1(core1_entry);
	// core0_entry();


	// multicore_launch_core1(camInit);

	// multicore_launch_core1(screenInit);
}

long prevLooptime=0;
long prevLooptime1=0;
uint mintemp=34;
uint maxtemp=35;
void loop() {
	if (millis()-prevLooptime>15) {
		prevLooptime=millis();
		// the screen updates faster than the camera, so we manually restart that core each loop. may be better to use interrupts!
	// if (millis()-prevLooptime1>1000) {
	// 	prevLooptime1=millis();
	// 	mintemp--;
	// 	maxtemp++;
	// 	generateHuePalette(colorPalette,mintemp,maxtemp,0,360,false);
	// }

		multicore_reset_core1();	//needed
		multicore_launch_core1(screenLoop);
		// multicore_launch_core1(camLoop);
		// screenLoop();
		camLoop();
	}
	


}
