/*
 * Buttons.c
 *
 *  Created on: 21 Octber 2016
 * author: 		Sebastien CAPOU (neskweek@gmail.com) and Andras Kun (kun.andras@yahoo.de)
 * Source : 	https://github.com/neskweek/LightSaberOS
 */

#include "Buttons.h"
#include "Config.h"
#include "SoundFont.h"
#include "Light.h"


extern SoundFont soundFont;
enum SaberStateEnum {S_STANDBY, S_SABERON, S_CONFIG, S_SLEEP, S_JUKEBOX};
enum ActionModeSubStatesEnum {AS_HUM, AS_IGNITION, AS_RETRACTION, AS_BLADELOCKUP, AS_PREBLADELOCKUP, AS_BLASTERDEFLECTMOTION, AS_BLASTERDEFLECTPRESS, AS_CLASH, AS_SWING, AS_SPIN, AS_FORCE};
enum ConfigModeSubStatesEnum {CS_VOLUME, CS_SOUNDFONT, CS_MAINCOLOR, CS_CLASHCOLOR, CS_BLASTCOLOR, CS_FLICKERTYPE, CS_IGNITIONTYPE, CS_RETRACTTYPE, CS_SLEEPINIT, CS_BATTERYLEVEL};
extern SaberStateEnum SaberState;
extern SaberStateEnum PrevSaberState;
extern ActionModeSubStatesEnum ActionModeSubStates;
extern ConfigModeSubStatesEnum ConfigModeSubStates;
extern unsigned long sndSuppress;
extern bool hum_playing;
extern int8_t modification;
extern bool play;
extern int16_t value;

//extern bool blasterBlocks;
extern bool lockuponclash;
extern int8_t blink;
extern bool changeMenu;
extern uint8_t menu;
extern bool enterMenu;

#if defined PIXELBLADE
extern cRGB color;
extern cRGB currentColor;
#endif
extern uint8_t blaster;
extern void HumRelaunch();
extern void SinglePlay_Sound(uint8_t track);
extern void LoopPlay_Sound(uint8_t track);
extern void Pause_Sound();
extern void Resume_Sound();
extern void Set_Loop_Playback();
extern void Set_Volume();
extern void confParseValue(uint16_t variable, uint16_t min, uint16_t max,
    short int multiplier);
#ifndef COLORS
extern uint8_t GravityVector();
#endif

#if defined PIXELBLADE
extern struct StoreStruct {
  // This is for mere detection if they are our settings
  char version[5];
  // The settings
  uint8_t volume;// 0 to 31
  uint8_t soundFont;// as many as Sound font you have defined in Soundfont.h Max:253
  struct Profile {
  #ifdef COLORS
    uint8_t mainColor;  //colorID
    uint8_t clashColor;//colorID
    uint8_t blasterboltColor;//colorID
  #else
    cRGB mainColor;
    cRGB clashColor;
    cRGB blasterboltColor;
  #endif
  }sndProfile[SOUNDFONT_QUANTITY + 2];
}storage;
#endif // PIXELBLADE
// ====================================================================================
// ===               			BUTTONS CALLBACK FUNCTIONS                 			===
// ====================================================================================
void red() {
  analogWrite(RED_ACCENT_LED,0);
analogWrite(GREEN_ACCENT_LED,255);
analogWrite(BLUE_ACCENT_LED,255);
}

void green() {
  analogWrite(RED_ACCENT_LED,255);
analogWrite(GREEN_ACCENT_LED,0);
analogWrite(BLUE_ACCENT_LED,255);
}

void blue() {
  analogWrite(RED_ACCENT_LED,255);
analogWrite(GREEN_ACCENT_LED,255);
analogWrite(BLUE_ACCENT_LED,0);
}

void mainClick() {
  green();
#if defined LS_BUTTON_DEBUG
	Serial.println(F("Main button click."));
#endif
	if (SaberState==S_SABERON) {
    if (lockuponclash) {
      lockuponclash=false;
      HumRelaunch();
      ActionModeSubStates=AS_HUM;
      #if defined LS_BUTTON_DEBUG
            Serial.println(F("End clash triggered lockup (either pre or active phase)"));
      #endif
    }
    else {
      lockuponclash=true;
#if defined LS_BUTTON_DEBUG
      Serial.println(F("Start clash triggered lockup (either pre or active phase)"));
#endif
    }
	}
	else if (SaberState==S_CONFIG) {
    SinglePlay_Sound(1);
    delay(50);
    if (ConfigModeSubStates == CS_VOLUME) {
      confParseValue(storage.volume, 0, 30, 1);
      storage.volume = value;
      BladeMeter(value*100/30);
      Set_Volume();
      #if defined LS_INFO
              Serial.println(storage.volume);
      #endif
    }
    else if (ConfigModeSubStates == CS_SOUNDFONT) {
      play = false;
      confParseValue(storage.soundFont, 2, SOUNDFONT_QUANTITY + 1, 1);
      storage.soundFont = value;
      soundFont.setID(value);
      SinglePlay_Sound(soundFont.getMenu());
          #if defined PIXELBLADE
            getColor(storage.sndProfile[storage.soundFont].mainColor);
            lightOn(currentColor);
          #endif  // PIXELBLADE

      delay(150);
      #if defined LS_INFO
              Serial.println(soundFont.getID());
      #endif
    }
#ifdef COLORS
    else if (ConfigModeSubStates == CS_MAINCOLOR) {
      confParseValue(storage.sndProfile[storage.soundFont].mainColor, 0, COLORS - 1, 1);
      storage.sndProfile[storage.soundFont].mainColor =value;
      #ifdef PIXELBLADE
        getColor(storage.sndProfile[storage.soundFont].mainColor);
        lightOn(currentColor);
      #endif  // PIXELBLADE
    }
    else if (ConfigModeSubStates == CS_CLASHCOLOR) {
      confParseValue(storage.sndProfile[storage.soundFont].clashColor, 0, COLORS - 1, 1);
      storage.sndProfile[storage.soundFont].clashColor =value;
      #ifdef PIXELBLADE
        getColor(storage.sndProfile[storage.soundFont].clashColor);
        lightOn(currentColor);
      #endif  // PIXELBLADE
    }
    else if (ConfigModeSubStates == CS_BLASTCOLOR) {
      confParseValue(storage.sndProfile[storage.soundFont].blasterboltColor, 0, COLORS - 1, 1);
      storage.sndProfile[storage.soundFont].blasterboltColor =value;
      #ifdef PIXELBLADE
        getColor(storage.sndProfile[storage.soundFont].blasterboltColor);
        lightOn(currentColor);
      #endif  // PIXELBLADE
    }
    //modification=0;  // reset config mode change indicator
#else // not COLORS
    #if defined(PIXELBLADE)
    else if (ConfigModeSubStates == CS_MAINCOLOR) {
      ColorMixing(storage.sndProfile[storage.soundFont].mainColor,modification, MAX_BRIGHTNESS, true);
      storage.sndProfile[storage.soundFont].mainColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].mainColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].mainColor.b=currentColor.b;
      #ifdef PIXELBLADE
        lightOn(currentColor, NUMPIXELS/2, NUMPIXELS-6);
      #endif  // PIXELBLADE
    }
    else if (ConfigModeSubStates == CS_CLASHCOLOR) {
      ColorMixing(storage.sndProfile[storage.soundFont].clashColor,modification, MAX_BRIGHTNESS, true);
      storage.sndProfile[storage.soundFont].clashColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].clashColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].clashColor.b=currentColor.b;
      #ifdef PIXELBLADE
        lightOn(currentColor, 1, NUMPIXELS/2-1);
      #endif  // PIXELBLADE
    }
    else if (ConfigModeSubStates == CS_BLASTCOLOR) {
      ColorMixing(storage.sndProfile[storage.soundFont].blasterboltColor,modification, MAX_BRIGHTNESS, true);
      storage.sndProfile[storage.soundFont].blasterboltColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].blasterboltColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].blasterboltColor.b=currentColor.b;
      #ifdef PIXELBLADE
        lightOn(currentColor, NUMPIXELS*3/4-5, NUMPIXELS*3/4);
      #endif  // PIXELBLADE
    }
    #endif // PIXELBLADE
#endif // COLORS/not COLORS
	}
	else if (SaberState==S_STANDBY) {
		// LightSaber poweron
   SaberState=S_SABERON;
   PrevSaberState=S_STANDBY;
   ActionModeSubStates=AS_IGNITION;
		//actionMode = true;
	}

} // mainClick

void mainDoubleClick() {
  blue();
#if defined LS_BUTTON_DEBUG
	Serial.println(F("Main button double click."));
#endif
#ifdef SINGLEBUTTON
	if (SaberState==S_SABERON) {
		//ACTION TO DEFINE
    #if defined LS_BUTTON_DEBUG
      Serial.println(F("Start motion triggered blaster bolt deflect"));
    #endif
    if (ActionModeSubStates!=AS_BLASTERDEFLECTMOTION) { // start motion triggered blaster deflect
      ActionModeSubStates=AS_BLASTERDEFLECTMOTION;
      #if defined LS_BUTTON_DEBUG
            Serial.println(F("Start motion triggered blaster bolt deflect"));
      #endif
    }
    else { // stop motion triggered blaster deflect
      #if defined LS_BUTTON_DEBUG
            Serial.println(F("End motion triggered blaster bolt deflect"));
      #endif
      HumRelaunch();
      ActionModeSubStates=AS_HUM;
      #ifdef ACCENT_LED
      accentLEDControl(AL_ON);
      #else if MULTICOLOR_ACCENT_LED
      //accentLEDControl(AL_ON,currentColor);
      #endif
    }
} else if (SaberState==S_CONFIG) {
// Change Menu
    switch(ConfigModeSubStates) {
      case CS_BATTERYLEVEL:
        lightOff();
        ConfigModeSubStates=CS_SOUNDFONT;
        SinglePlay_Sound(5);
        delay(600);
        SinglePlay_Sound(soundFont.getMenu());
          #if defined PIXELBLADE
            getColor(storage.sndProfile[storage.soundFont].mainColor);
              lightOn(currentColor);
          #endif  // PIXELBLADE
        delay(500);
        break;
      case CS_SOUNDFONT:
          ConfigModeSubStates=CS_MAINCOLOR;
          SinglePlay_Sound(6);
          delay(500);
          #if defined LS_FSM
            Serial.print(F("Main color"));
          #endif
          #if defined PIXELBLADE
            getColor(storage.sndProfile[storage.soundFont].mainColor);
            #ifdef COLORS
              lightOn(currentColor);
            #else  // not COLORS
              lightOn(currentColor, NUMPIXELS/2, NUMPIXELS-6);
            #endif
          #endif  // PIXELBLADE
        break;

      case CS_VOLUME:
          lightOff();
          ConfigModeSubStates=CS_SOUNDFONT;
          SinglePlay_Sound(5);
          delay(600);
          SinglePlay_Sound(soundFont.getMenu());
            #if defined PIXELBLADE
              getColor(storage.sndProfile[storage.soundFont].mainColor);
                lightOn(currentColor);
            #endif  // PIXELBLADE
          delay(500);
        break;
      case CS_MAINCOLOR:
        ConfigModeSubStates=CS_CLASHCOLOR;
        SinglePlay_Sound(7);
        delay(500);
        #if defined LS_FSM
          Serial.print(F("Clash color"));
        #endif
        #if defined PIXELBLADE
          getColor(storage.sndProfile[storage.soundFont].clashColor);
            #ifdef COLORS
              lightOn(currentColor);
            #else  // not COLORS
              lightOn(currentColor, 1, NUMPIXELS/2-1);
            #endif
         #endif  // PIXELBLADE
        break;
      case CS_CLASHCOLOR:
        ConfigModeSubStates=CS_BLASTCOLOR;
        SinglePlay_Sound(8);
        delay(500);
        #if defined LS_FSM
          Serial.print(F("Blaster color"));
        #endif
        #if defined PIXELBLADE
          getColor(storage.sndProfile[storage.soundFont].blasterboltColor);
            #ifdef COLORS
              lightOn(currentColor);
            #else  // not COLORS
              lightOn(currentColor, NUMPIXELS*3/4-5, NUMPIXELS*3/4);
            #endif
         #endif  // PIXELBLADE
        break;
      case CS_BLASTCOLOR:
        #if defined LS_FSM
          Serial.print(F("Volume"));
        #endif
        ConfigModeSubStates=CS_VOLUME;
        BladeMeter(storage.volume*100/30);
        SinglePlay_Sound(4);
        delay(500);
        break;
      }
  }
#endif  // SINGLEBUTTON
} // mainDoubleClick

void mainLongPressStart() {
  red();
#if defined LS_BUTTON_DEBUG
	Serial.println(F("Main button longPress start"));
#endif
	if (SaberState==S_SABERON) {
    // LightSaber switch-off
    ActionModeSubStates=AS_RETRACTION;
    SaberState=S_STANDBY;
    PrevSaberState=S_SABERON;
	} else if (SaberState==S_CONFIG) {
//Leaving Config Mode
  if (ConfigModeSubStates!=CS_MAINCOLOR and ConfigModeSubStates!=CS_CLASHCOLOR and ConfigModeSubStates!=CS_BLASTCOLOR) {
    changeMenu = false;
    SaberState=S_STANDBY;
    PrevSaberState=S_CONFIG;
  }
  }

#ifdef SINGLEBUTTON
  else if (SaberState==S_STANDBY) {
    //Entering Config Mode
    SaberState=S_CONFIG;
    PrevSaberState=S_STANDBY;
	}
#endif
} // mainLongPressStart

void mainLongPress() {
#if defined LS_BUTTON_DEBUG
	Serial.println(F("Main button longPress..."));
#endif
	if (SaberState==S_SABERON) {
	} else if (SaberState==S_CONFIG) {
    #if defined(PIXELBLADE)
    #ifndef COLORS
    //if (ConfigModeSubStates==CS_MAINCOLOR or ConfigModeSubStates==CS_CLASHCOLOR or ConfigModeSubStates==CS_BLASTCOLOR) {
    //  modification=GravityVector();
    //}
    if (ConfigModeSubStates==CS_MAINCOLOR) {
      //confParseValue(storage.sndProfile[storage.soundFont].mainColor, 0, 100 - 1, 1);
      ColorMixing(storage.sndProfile[storage.soundFont].mainColor,modification,false);
      storage.sndProfile[storage.soundFont].mainColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].mainColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].mainColor.b=currentColor.b;
          #ifdef PIXELBLADE
            lightOn(currentColor, NUMPIXELS/2, NUMPIXELS-6);
          #endif
      delay(50);

      }
    else if (ConfigModeSubStates==CS_CLASHCOLOR) {
      ColorMixing(storage.sndProfile[storage.soundFont].clashColor,modification,false);
      storage.sndProfile[storage.soundFont].clashColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].clashColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].clashColor.b=currentColor.b;
          #ifdef PIXELBLADE
            lightOn(currentColor, 1, NUMPIXELS/2-1);
          #endif
      delay(50);
    }
    else if (ConfigModeSubStates==CS_BLASTCOLOR) {
      ColorMixing(storage.sndProfile[storage.soundFont].blasterboltColor,modification,false);
      storage.sndProfile[storage.soundFont].blasterboltColor.r=currentColor.r;
      storage.sndProfile[storage.soundFont].blasterboltColor.g=currentColor.g;
      storage.sndProfile[storage.soundFont].blasterboltColor.b=currentColor.b;
          #ifdef PIXELBLADE
            lightOn(currentColor, NUMPIXELS*3/4-5, NUMPIXELS*3/4);
          #endif
      delay(50);
    }
  #endif // not COLORS
  #endif // PIXELBLADE
	}
	else if (SaberState==S_STANDBY) {
		/*
		 * ACTION TO DEFINE
		 */
	}
} // mainLongPress

void mainLongPressStop() {
#if defined LS_BUTTON_DEBUG
	Serial.println(F("Main button longPress stop"));
#endif
} // mainLongPressStop
