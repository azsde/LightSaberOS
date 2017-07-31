/*
 * Light.h
 *
 *  Created on: 21 Octber 2016
 * author: 		Sebastien CAPOU (neskweek@gmail.com) and Andras Kun (kun.andras@yahoo.de)
 * Source : 	https://github.com/neskweek/LightSaberOS
 */

#if not defined LIGHT_H_
#define LIGHT_H_

#include <Arduino.h>
//#include "FastLED.h"

#include "Config.h"


enum AccentLedAction_En {AL_PULSE, AL_ON, AL_OFF};

#if defined ACCENT_LED
#if defined SOFT_ACCENT

struct softPWM {
  uint8_t dutyCycle; // in percent
  bool revertCycle;
  uint8_t state;
  uint16_t tick;
} pwmPin = { 100, false, LOW, 0 };
#endif
#endif

// ====================================================================================
// ===              	    			LED FUNCTIONS		                		===
// ====================================================================================

void BladeMeter (int meterLevel);

#if defined PIXELBLADE

//void pixelblade_KillKey_Enable();
//void pixelblade_KillKey_Disable();

void lightOn(cRGB color, int8_t StartPixel=-1, int8_t StopPixel=-1);
void lightOff();

void lightIgnition(cRGB color, uint16_t time, uint8_t type);
void lightRetract( uint16_t time, uint8_t type);

#ifdef COLORS
void lightBlasterEffect( uint8_t pixel, uint8_t range, uint8_t SndFnt_MainColor);
#else
void lightBlasterEffect( uint8_t pixel, uint8_t range, cRGB SndFnt_MainColor);
#endif
void lightFlicker( uint8_t value = 0,uint8_t AState=0);

#ifdef COLORS
void getColor(uint8_t color); //getColor
#else
void getColor(cRGB color); //getColor
void ColorMixing(cRGB colorID, int8_t mod, uint8_t maxBrightness=MAX_BRIGHTNESS, bool Saturate=false);
#endif
void RampPixels(uint16_t RampDuration, bool DirectionUpDown);

#ifdef FIREBLADE
void FireBlade();
cRGB HeatColor( uint8_t temperature);
uint8_t scale8_video( uint8_t i, uint8_t scale);
#endif

#endif
#ifndef MULTICOLOR_ACCENT_LED
void accentLEDControl(AccentLedAction_En AccentLedAction);
#else
void accentLEDControl(AccentLedAction_En AccentLedAction, cRGB color);
#endif
void PWM();
#endif /* LIGHT_H_ */


