/*
 * Light.h
 *
 *  Created on: 6 mars 2016
 * author: 		Sebastien CAPOU (neskweek@gmail.com)
 * Source : 	https://github.com/neskweek/LightSaberOS
 */

#if not defined LIGHT_H_
#define LIGHT_H_

#include <Arduino.h>
#include <WS2812.h>
#include "Config.h"




// ====================================================================================
// ===              	    			LED FUNCTIONS		                		===
// ====================================================================================

#if defined LEDSTRINGS

void lightOn(uint8_t ledPins[], int8_t segment = -1);
void lightOff();

void lightIgnition(uint8_t ledPins[], uint16_t time, uint8_t type);
void lightRetract(uint8_t ledPins[], uint16_t time, uint8_t type);

void FoCOn (uint8_t pin);
void FoCOff (uint8_t pin);

void lightFlicker(uint8_t ledPins[], uint8_t type, uint8_t value = 0);

#endif
#if defined LUXEON

void lightOn(uint8_t ledPins[], uint8_t color[]);
void lightOff(uint8_t ledPins[]);

void lightIgnition(uint8_t ledPins[], uint8_t color[], uint16_t time);
void lightRetract(uint8_t ledPins[], uint8_t color[], uint16_t time);

void lightFlicker(uint8_t ledPins[], uint8_t color[], uint8_t value = 0);


#if defined MY_OWN_COLORS
void getColor(uint8_t color[], uint8_t colorID); //getColor
#endif
#if defined FIXED_RANGE_COLORS
void getColor(uint8_t color[], uint16_t colorID); //getColor
#endif
#endif

#if defined NEOPIXEL


void lightOn(cRGB color,int16_t pixel = -1);
void lightOff();

void lightIgnition(cRGB color, uint16_t time, uint8_t type);
void lightRetract( uint16_t time, uint8_t type);

void lightBlasterEffect( uint8_t pixel, uint8_t range);
void lightFlicker( uint8_t value = 0);

void getColor(uint8_t colorID); //getColor

#endif


#endif /* LIGHT_H_ */
