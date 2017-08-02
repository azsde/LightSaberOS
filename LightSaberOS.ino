/*
   LightSaberOS V1.5.2jb

   Modified from LSOS 1.5 2017 March 3

   released on: 21 Octber 2016
   author: 		Sebastien CAPOU (neskweek@gmail.com) and Andras Kun (kun.andras@yahoo.de)
   Source : 	https://github.com/neskweek/LightSaberOS
   Description:	Operating System for Arduino based LightSaber

   This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
   To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/.
*/
/***************************************************************************************************
* DFPLAYER variables
*/

#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
SoftwareSerial mySoftwareSerial(7, 8); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

#include <SoftwareSerial.h> // interestingly the DFPlayer lib refuses

#include <Arduino.h>
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <EEPROMex.h>
#include <OneButton.h>
#include <LinkedList.h>


#include "Buttons.h"
#include "Config.h"
#include "ConfigMenu.h"
#include "Light.h"
#include "SoundFont.h"

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include <Wire.h>
#endif

SoundFont soundFont;
unsigned long sndSuppress = millis();
unsigned long sndSuppress2 = millis();
bool hum_playing = false; // variable to store whether hum is being played


/***************************************************************************************************
 * Saber Finite State Machine Custom Type and State Variable
 */
enum SaberStateEnum {S_STANDBY, S_SABERON, S_CONFIG, S_SLEEP, S_JUKEBOX};
SaberStateEnum SaberState;
SaberStateEnum PrevSaberState;

enum ActionModeSubStatesEnum {AS_HUM, AS_IGNITION, AS_RETRACTION, AS_BLADELOCKUP, AS_PREBLADELOCKUP, AS_BLASTERDEFLECTMOTION, AS_BLASTERDEFLECTPRESS, AS_CLASH, AS_SWING, AS_SPIN, AS_FORCE};
ActionModeSubStatesEnum ActionModeSubStates;

enum ConfigModeSubStatesEnum {CS_VOLUME, CS_SOUNDFONT, CS_MAINCOLOR, CS_CLASHCOLOR, CS_BLASTCOLOR, CS_FLICKERTYPE, CS_IGNITIONTYPE, CS_RETRACTTYPE, CS_SLEEPINIT, CS_BATTERYLEVEL};
ConfigModeSubStatesEnum ConfigModeSubStates;
/***************************************************************************************************
 * Motion detection Variables
*/
MPU6050 mpu;
// MPU control/status vars
volatile bool mpuInterrupt = false; // indicates whether MPU interrupt pin has gone high
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus; // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint8_t fifoBuffer[64]; // FIFO storage buffer
uint16_t mpuFifoCount;     // count of all bytes currently in FIFO
// orientation/motion vars
Quaternion curRotation;           // [w, x, y, z]         quaternion container
Quaternion prevRotation;           // [w, x, y, z]         quaternion container
static Quaternion prevOrientation;  // [w, x, y, z]         quaternion container
static Quaternion curOrientation; // [w, x, y, z]         quaternion container
VectorInt16 curAccel;
VectorInt16 prevAccel;
VectorInt16 curDeltAccel;
VectorInt16 prevDeltAccel;

/***************************************************************************************************
 * LED String variables
 */

#if defined PIXELBLADE
// Define the array of leds
WS2812 pixels(NUMPIXELS);
cRGB color;
cRGB currentColor;
uint8_t blasterPixel;
bool extraInitDone = false;
#endif

uint8_t blaster = 0;
//bool blasterBlocks = false;
uint8_t clash = 0;
bool lockuponclash = false;
uint8_t blink = 0;
uint8_t randomBlink = 0;
/***************************************************************************************************
 * Buttons variables
 */
OneButton mainButton(SINGLEBUTTON, true);

// replaced by Saber State Machine Variables
//bool actionMode = false; // Play with your saber
//bool configMode = false; // Configure your saber
//static bool ignition = false;
//static bool browsing = false;

/***************************************************************************************************
 * ConfigMode Variables
 */
int8_t modification = 0;
int16_t value = 0;
uint8_t menu = 0;
bool enterMenu = false;
bool changeMenu = false;
bool play = false;
unsigned int configAdress = 0;
volatile uint8_t portbhistory = 0xFF;     // default is high because the pull-up

#if defined PIXELBLADE
struct StoreStruct {
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

#endif

/***************************************************************************************************
 * Function Prototypes
 * The following prototypes are not correctly generated by Arduino IDE 1.6.5-r5 or previous
 */
inline void printQuaternion(Quaternion quaternion, long multiplier);
inline void printAcceleration(VectorInt16 aaWorld);

// ====================================================================================
// ===        	       	   			SETUP ROUTINE  	 	                			===
// ====================================================================================
void setup() {
	// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
	Wire.begin();
	TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz). Comment this line if having compilation difficulties with TWBR.
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
			Fastwire::setup(400, true);
#endif
	// Serial line for debug
	Serial.begin(115200);


	/***** LOAD CONFIG *****/
	// Get config from EEPROM if there is one
	// or initialise value with default ones set in StoreStruct
	EEPROM.setMemPool(MEMORYBASE, EEPROMSizeATmega328); //Set memorypool base to 32, assume Arduino Uno board
	configAdress = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object

	if (!loadConfig()) {
		for (uint8_t i = 0; i <= 2; i++)
			storage.version[i] = CONFIG_VERSION[i];
		storage.soundFont = SOUNDFONT;
		storage.volume = VOL;

#if defined PIXELBLADE
    for (uint8_t i=2; i<SOUNDFONT_QUANTITY+2;i++){
      #ifdef COLORS
      storage.sndProfile[i].mainColor=1;
      storage.sndProfile[i].clashColor=1;
      storage.sndProfile[i].blasterboltColor=1;
      #else
      storage.sndProfile[i].mainColor.r=20;
      storage.sndProfile[i].mainColor.g=20;
      storage.sndProfile[i].mainColor.b=20;
      storage.sndProfile[i].clashColor.r=20;
      storage.sndProfile[i].clashColor.g=20;
      storage.sndProfile[i].clashColor.b=20;
      storage.sndProfile[i].blasterboltColor.r=20;
      storage.sndProfile[i].blasterboltColor.g=20;
      storage.sndProfile[i].blasterboltColor.b=20;
      #endif
    }
#endif
    saveConfig();
#if defined LS_INFO
    Serial.println(F("DEFAULT VALUE"));
#endif
  }
#if defined LS_INFO
  else {
    Serial.println(F("EEPROM LOADED"));
  }
#endif
  soundFont.setID(storage.soundFont);

  /***** LOAD CONFIG *****/

  /***** MP6050 MOTION DETECTOR INITIALISATION  *****/

  // initialize device
#if defined LS_INFO
  Serial.println(F("Initializing I2C devices..."));
#endif
  mpu.initialize();

  // verify connection
#if defined LS_INFO
  Serial.println(F("Testing device connections..."));
  Serial.println(
    mpu.testConnection() ?
    F("MPU6050 connection successful") :
    F("MPU6050 connection failed"));

  // load and configure the DMP
  Serial.println(F("Initializing DMP..."));
#endif
  devStatus = mpu.dmpInitialize_light();  // this is a ligter version of the above

  /*
     Those offsets are specific to each MPU6050 device.
     they are found via calibration process.
     See this script http://www.i2cdevlib.com/forums/index.php?app=core&module=attach&section=attach&attach_id=27
  */
#ifdef MPUCALOFFSETEEPROM
  // retreive MPU6050 calibrated offset values from EEPROM
  EEPROM.setMemPool(MEMORYBASEMPUCALIBOFFSET, EEPROMSizeATmega328);
  int addressInt = MEMORYBASEMPUCALIBOFFSET;
  mpu.setXAccelOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  int16_t output;
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
  addressInt = addressInt + 2; //EEPROM.getAddress(sizeof(int));
  mpu.setYAccelOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
  addressInt = addressInt + 2; //EEPROM.getAddress(sizeof(int));
  mpu.setZAccelOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
  addressInt = addressInt + 2; //EEPROM.getAddress(sizeof(int));
  mpu.setXGyroOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
  addressInt = addressInt + 2; //EEPROM.getAddress(sizeof(int));
  mpu.setYGyroOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
  addressInt = addressInt + 2; //EEPROM.getAddress(sizeof(int));
  mpu.setZGyroOffset(EEPROM.readInt(addressInt));
#ifdef LS_INFO
  output = EEPROM.readInt(addressInt);
  Serial.print("address: "); Serial.println(addressInt); Serial.print("output: "); Serial.println(output); Serial.println("");
#endif
#else // assign calibrated offset values here:
  /* UNIT1 */
  mpu.setXAccelOffset(46);
  mpu.setYAccelOffset(-4942);
  mpu.setZAccelOffset(4721);
  mpu.setXGyroOffset(23);
  mpu.setYGyroOffset(-11);
  mpu.setZGyroOffset(44);
#endif



  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
    // turn on the DMP, now that it's ready
#if defined LS_INFO
    Serial.println(F("Enabling DMP..."));
#endif
    mpu.setDMPEnabled(true);

    // enable Arduino interrupt detection
#if defined LS_INFO
    Serial.println(
      F(
        "Enabling interrupt detection (Arduino external interrupt 0)..."));
#endif
    //		attachInterrupt(0, dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use it
#if defined LS_INFO
    Serial.println(F("DMP ready! Waiting for first interrupt..."));
#endif
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
#if defined LS_INFO
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
#endif
  }

  // configure the motion interrupt for clash recognition
  // INT_PIN_CFG register
  // in the working code of MPU6050_DMP all bits of the INT_PIN_CFG are false (0)
  mpu.setDLPFMode(3);
  mpu.setDHPFMode(0);
  //mpu.setFullScaleAccelRange(3);
  mpu.setIntMotionEnabled(true); // INT_ENABLE register enable interrupt source  motion detection
  mpu.setIntZeroMotionEnabled(false);
  mpu.setIntFIFOBufferOverflowEnabled(false);
  mpu.setIntI2CMasterEnabled(false);
  mpu.setIntDataReadyEnabled(false);
  //  mpu.setMotionDetectionThreshold(10); // 1mg/LSB
  mpu.setMotionDetectionThreshold(CLASH_THRESHOLD); // 1mg/LSB
  mpu.setMotionDetectionDuration(2); // number of consecutive samples above threshold to trigger int
  mpuIntStatus = mpu.getIntStatus();

  /***** MP6050 MOTION DETECTOR INITIALISATION  *****/

#if defined PIXELBLADE
  currentColor.r = 0;
  currentColor.g = 0;
  currentColor.b = 0;
  lightOn(currentColor);
  delay(300);
  lightOff();
  getColor(storage.sndProfile[storage.soundFont].mainColor);
#endif

#if defined ACCENT_LED
  pinMode(ACCENT_LED, OUTPUT);
#else if MULTICOLOR_ACCENT_LED
  pinMode(ACCENT_LED_COMMON_ANODE, OUTPUT);
#endif

  //Randomize randomness (no really that's what it does)
  randomSeed(analogRead(2));

  /***** LED SEGMENT INITIALISATION  *****/

  /***** BUTTONS INITIALISATION  *****/

  // link the Main button functions.
  mainButton.setClickTicks(CLICK);
  mainButton.setPressTicks(PRESS_CONFIG);
  mainButton.attachClick(mainClick);
  mainButton.attachDoubleClick(mainDoubleClick);
  mainButton.attachLongPressStart(mainLongPressStart);
  mainButton.attachLongPressStop(mainLongPressStop);
  mainButton.attachDuringLongPress(mainLongPress);

  /***** DF PLAYER INITIALISATION  *****/
  InitDFPlayer();

  delay(1500);
  pinMode(SPK1, INPUT);
  pinMode(SPK2, INPUT);
  SinglePlay_Sound(11);
  delay(1500);


  /****** INIT SABER STATE VARIABLE *****/
  SaberState = S_STANDBY;
  PrevSaberState = S_SLEEP;
  ActionModeSubStates = AS_HUM;

  digitalWrite(ACCENT_LED_COMMON_ANODE,HIGH);
}

// ====================================================================================
// ===               	   			LOOP ROUTINE  	 	                			===
// ====================================================================================
void loop() {

   mainButton.tick();

   // For some reason if I put this in the setup routine, it messes up everything...
   if (extraInitDone == false) {
    initLedStrip();
    initCrystalLED();
    extraInitDone = true;
   }

  // if MPU6050 DMP programming failed, don't try to do anything : EPIC FAIL !
  if (!dmpReady) {
    Serial.println(F("uh oh"));
    return;
  }

  /*//////////////////////////////////////////////////////////////////////////////////////////////////////////
     ACTION MODE HANDLER
  */ /////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (SaberState == S_SABERON) {

      // In case we want to time the loop
      //Serial.print(F("Action Mode"));
      //Serial.print(F(" time="));
      //Serial.println(millis());

    if (ActionModeSubStates != AS_HUM) { // needed for hum relauch only in case it's not already being played
      hum_playing = false;
    }
    else { // AS_HUM
      if ((millis() - sndSuppress > HUM_RELAUNCH and not hum_playing)) {
        HumRelaunch();
      }
    }

    if (ActionModeSubStates == AS_IGNITION) {
      /*
          This is the very first loop after Action Mode has been turned on
      */
      //attachInterrupt(0, dmpDataReady, RISING);
      // Reduce lockup trigger time for faster lockup response

/*#if defined PIXELBLADE
      pixelblade_KillKey_Disable();
#endif*/
#if defined LS_INFO
      Serial.println(F("START ACTION"));
#endif

      //Play powerons wavs
      SinglePlay_Sound(soundFont.getPowerOn());
      // Light up the ledstrings
#if defined PIXELBLADE
      /*for (uint8_t i = 0; i <= 5; i++) {
        digitalWrite(ledPins[i], HIGH);
      }*/
      lightIgnition(currentColor, soundFont.getPowerOnTime(), 0);
      //deployBlade(currentColor);

#endif
      sndSuppress = millis();
      sndSuppress2 = millis();

      // Get the initial position of the motion detector
      motionEngine();
      ActionModeSubStates = AS_HUM;
      //ignition = true;

#if defined ACCENT_LED
  // turns accent LED On
  accentLEDControl(AL_ON);
  //digitalWrite(ACCENT_LED, HIGH);
#else if MULTICOLOR_ACCENT_LED
  accentLEDControl(AL_ON, currentColor);
#endif
    }

    // ************************* blade movement detection ************************************
    //Let's get our values !
    motionEngine();

    /*
       CLASH DETECTION :
       A clash is a violent deceleration when 2 blades hit each other
       For a realistic clash detection it's imperative to detect
       such a deceleration instantenously, which is only feasible
       using the motion interrupt feature of the MPU6050.
    */
    if (mpuIntStatus > 60 and mpuIntStatus < 70 and ActionModeSubStates != AS_BLADELOCKUP) {

#if defined LS_CLASH_DEBUG
      Serial.print(F("CLASH\tmpuIntStatus="));
      Serial.println(mpuIntStatus);
#endif
      if (lockuponclash) {
        //if (ActionModeSubStates==AS_PREBLADELOCKUP or lockuponclash) {
        //Lockup Start
        ActionModeSubStates = AS_BLADELOCKUP;
        if (soundFont.getLockup()) {
          LoopPlay_Sound(soundFont.getLockup());
          //sndSuppress = millis();
          //while (millis() - sndSuppress < 50) {
          //}
          //Set_Loop_Playback();
          //sndSuppress = millis();
          //while (millis() - sndSuppress < 50) {
          //}
        }
      }
      else { // ordinary clash
        if (millis() - sndSuppress >= CLASH_SUPRESS) {
          //blink = 0;
          //clash = CLASH_FLASH_TIME;
          SinglePlay_Sound(soundFont.getClash());
          sndSuppress = millis();
          sndSuppress2 = millis();
          /*
             THIS IS A CLASH  !
          */
          ActionModeSubStates = AS_CLASH;

#if defined PIXELBLADE
#ifdef FIREBLADE  // simply flash white
          getColor(14);
          lightOn(currentColor);
#else
          getColor(storage.sndProfile[storage.soundFont].clashColor);
          lightOn(currentColor);
#endif
#endif
          delay(CLASH_FX_DURATION);  // clash duration

        }
      }
    }
    /*
       SIMPLE BLADE MOVEMENT DETECTION FOR MOTION  TRIGGERED BLASTER FEDLECT
       We detect swings as hilt's orientation change
       since IMUs sucks at determining relative position in space
    */
    // movement of the hilt while blaster move deflect is activated can trigger a blaster deflect
    else if ((ActionModeSubStates == AS_BLASTERDEFLECTPRESS or (ActionModeSubStates == AS_BLASTERDEFLECTMOTION and (abs(curDeltAccel.y) > soundFont.getSwingThreshold() // and it has suffisent power on a certain axis
              or abs(curDeltAccel.z) > soundFont.getSwingThreshold()
              or abs(curDeltAccel.x) > soundFont.getSwingThreshold()))) and (millis() - sndSuppress >= BLASTERBLOCK_SUPRESS)) {

      if (soundFont.getBlaster()) {
        SinglePlay_Sound(soundFont.getBlaster());

#if defined PIXELBLADE
#ifdef FIREBLADE
        getColor(14);
        lightOn(currentColor);
#else
        lightOn(currentColor);
        blasterPixel = random(NUMPIXELS / 4, NUMPIXELS - 3); //momentary shut off one led segment
        blink = 0;
        getColor(storage.sndProfile[storage.soundFont].blasterboltColor);
        //            lightBlasterEffect(blasterPixel, 3, storage.sndProfile[storage.soundFont].mainColor);
        lightBlasterEffect(blasterPixel, map(NUMPIXELS, 0, 120, 1, 3), storage.sndProfile[storage.soundFont].blasterboltColor);

#endif
#endif
        delay(BLASTER_FX_DURATION);  // blaster bolt deflect duration
        blaster = BLASTER_FLASH_TIME;
        // Some Soundfont may not have Blaster sounds
        if (millis() - sndSuppress > 50) {
          //SinglePlay_Sound(soundFont.getBlaster());
          sndSuppress = millis();
        }
      }
    }
    /*
       SWING DETECTION
       We detect swings as hilt's orientation change
       since IMUs sucks at determining relative position in space
    */
    else if (
      (ActionModeSubStates != AS_BLADELOCKUP or lockuponclash)// end lockuponclash event on a swing
      and abs(curRotation.w * 1000) < 999 // some rotation movement have been initiated
      and (
#if defined BLADE_X

        (
          (millis() - sndSuppress > SWING_SUPPRESS) // The movement doesn't follow another to closely
          and (abs(curDeltAccel.y) > soundFont.getSwingThreshold()  // and it has suffisent power on a certain axis
               or abs(curDeltAccel.z) > soundFont.getSwingThreshold()
               or abs(curDeltAccel.x) > soundFont.getSwingThreshold() * 10)
        )
        or (// A reverse movement follow a first one
          (millis() - sndSuppress2 > SWING_SUPPRESS)   // The reverse movement doesn't follow another reverse movement to closely
          // and it must be a reverse movement on Vertical axis
          and (
            abs(curDeltAccel.y) > abs(curDeltAccel.z)
            and abs(prevDeltAccel.y) > soundFont.getSwingThreshold()
            and (
              (prevDeltAccel.y > 0
               and curDeltAccel.y < -soundFont.getSwingThreshold())
              or (
                prevDeltAccel.y < 0
                and curDeltAccel.y	> soundFont.getSwingThreshold()
              )
            )
          )
        )
        or (// A reverse movement follow a first one
          (millis() - sndSuppress2 > SWING_SUPPRESS)  // The reverse movement doesn't follow another reverse movement to closely
          and ( // and it must be a reverse movement on Horizontal axis
            abs(curDeltAccel.z) > abs(curDeltAccel.y)
            and abs(prevDeltAccel.z) > soundFont.getSwingThreshold()
            and (
              (prevDeltAccel.z > 0
               and curDeltAccel.z < -soundFont.getSwingThreshold())
              or (
                prevDeltAccel.z < 0
                and curDeltAccel.z	> soundFont.getSwingThreshold()
              )
            )
          )
        )
      )

      // the movement must not be triggered by pure blade rotation (wrist rotation)
      and not (
        abs(prevRotation.x * 1000 - curRotation.x * 1000) > abs(prevRotation.y * 1000 - curRotation.y * 1000)
        and
        abs(prevRotation.x * 1000 - curRotation.x * 1000) > abs(prevRotation.z * 1000 - curRotation.z * 1000)
      )

#endif
#if defined BLADE_Y
      (
        (millis() - sndSuppress > SWING_SUPPRESS) // The movement doesn't follow another to closely
        and (abs(curDeltAccel.x) > soundFont.getSwingThreshold()  // and it has suffisent power on a certain axis
             or abs(curDeltAccel.z) > soundFont.getSwingThreshold()
             or abs(curDeltAccel.y) > soundFont.getSwingThreshold() * 10)
      )
      or (// A reverse movement follow a first one
        (millis() - sndSuppress2 > SWING_SUPPRESS)   // The reverse movement doesn't follow another reverse movement to closely
        // and it must be a reverse movement on Vertical axis
        and (
          abs(curDeltAccel.x) > abs(curDeltAccel.z)
          and abs(prevDeltAccel.x) > soundFont.getSwingThreshold()
          and (
            (prevDeltAccel.x > 0
             and curDeltAccel.x < -soundFont.getSwingThreshold())
            or (
              prevDeltAccel.x < 0
              and curDeltAccel.x	> soundFont.getSwingThreshold()
            )
          )
        )
      )
      or (// A reverse movement follow a first one
        (millis() - sndSuppress2 > SWING_SUPPRESS)  // The reverse movement doesn't follow another reverse movement to closely
        and ( // and it must be a reverse movement on Horizontal axis
          abs(curDeltAccel.z) > abs(curDeltAccel.x)
          and abs(prevDeltAccel.z) > soundFont.getSwingThreshold()
          and (
            (prevDeltAccel.z > 0
             and curDeltAccel.z < -soundFont.getSwingThreshold())
            or (
              prevDeltAccel.z < 0
              and curDeltAccel.z	> soundFont.getSwingThreshold()
            )
          )
        )
      )
    )

      // the movement must not be triggered by pure blade rotation (wrist rotation)
      and not (
        abs(prevRotation.y * 1000 - curRotation.y * 1000) > abs(prevRotation.x * 1000 - curRotation.x * 1000)
        and
        abs(prevRotation.y * 1000 - curRotation.y * 1000) > abs(prevRotation.z * 1000 - curRotation.z * 1000)
      )
#endif
#if defined BLADE_Z
      (
        (millis() - sndSuppress > SWING_SUPPRESS) // The movement doesn't follow another to closely
        and (abs(curDeltAccel.y) > soundFont.getSwingThreshold()  // and it has suffisent power on a certain axis
             or abs(curDeltAccel.x) > soundFont.getSwingThreshold()
             or abs(curDeltAccel.z) > soundFont.getSwingThreshold() * 10)
      )
      or (// A reverse movement follow a first one
        (millis() - sndSuppress2 > SWING_SUPPRESS)   // The reverse movement doesn't follow another reverse movement to closely
        // and it must be a reverse movement on Vertical axis
        and (
          abs(curDeltAccel.y) > abs(curDeltAccel.x)
          and abs(prevDeltAccel.y) > soundFont.getSwingThreshold()
          and (
            (prevDeltAccel.y > 0
             and curDeltAccel.y < -soundFont.getSwingThreshold())
            or (
              prevDeltAccel.y < 0
              and curDeltAccel.y	> soundFont.getSwingThreshold()
            )
          )
        )
      )
      or (// A reverse movement follow a first one
        (millis() - sndSuppress2 > SWING_SUPPRESS)  // The reverse movement doesn't follow another reverse movement to closely
        and ( // and it must be a reverse movement on Horizontal axis
          abs(curDeltAccel.x) > abs(curDeltAccel.y)
          and abs(prevDeltAccel.x) > soundFont.getSwingThreshold()
          and (
            (prevDeltAccel.x > 0
             and curDeltAccel.x < -soundFont.getSwingThreshold())
            or (
              prevDeltAccel.x < 0
              and curDeltAccel.x	> soundFont.getSwingThreshold()
            )
          )
        )
      )
      )

      // the movement must not be triggered by pure blade rotation (wrist rotation)
      and not (
        abs(prevRotation.z * 1000 - curRotation.z * 1000) > abs(prevRotation.y * 1000 - curRotation.y * 1000)
        and
        abs(prevRotation.z * 1000 - curRotation.z * 1000) > abs(prevRotation.x * 1000 - curRotation.x * 1000)
      )
#endif
      ) { // end of the condition definition for swings



      if ( ActionModeSubStates != AS_BLASTERDEFLECTMOTION and ActionModeSubStates != AS_BLASTERDEFLECTPRESS) {
        /*
            THIS IS A SWING !
        */
        prevDeltAccel = curDeltAccel;
#if defined LS_SWING_DEBUG
        Serial.print(F("SWING\ttime="));
        Serial.println(millis() - sndSuppress);
        Serial.print(F("\t\tcurRotation\tw="));
        Serial.print(curRotation.w * 1000);
        Serial.print(F("\t\tx="));
        Serial.print(curRotation.x);
        Serial.print(F("\t\ty="));
        Serial.print(curRotation.y);
        Serial.print(F("\t\tz="));
        Serial.print(curRotation.z);
        Serial.print(F("\t\tAcceleration\tx="));
        Serial.print(curDeltAccel.x);
        Serial.print(F("\ty="));
        Serial.print(curDeltAccel.y);
        Serial.print(F("\tz="));
        Serial.println(curDeltAccel.z);
#endif

        ActionModeSubStates = AS_SWING;
#ifndef FIREBLADE // FIREBLADE triggers false swings due to a random() function all, not yet understood
        SinglePlay_Sound(soundFont.getSwing());
#endif
        /* NORMAL SWING */




        if (millis() - sndSuppress > SWING_SUPPRESS) {
          sndSuppress = millis();
        }
        if (millis() - sndSuppress2 > SWING_SUPPRESS) {
          sndSuppress2 = millis();
        }

      }
    }
    else { // simply flicker
      if (ActionModeSubStates != AS_BLASTERDEFLECTMOTION and ActionModeSubStates != AS_BLADELOCKUP) { // do not deactivate blaster move deflect mode in case the saber is idling
        ActionModeSubStates = AS_HUM;
      }
      else if (ActionModeSubStates == AS_BLASTERDEFLECTMOTION) {
        #ifdef ACCENT_LED
            accentLEDControl(AL_PULSE);
        #else if MULTICOLOR_ACCENT_LED
            accentLEDControl(AL_PULSE, currentColor);
        #endif
      }
      // relaunch hum if more than HUM_RELAUNCH time elapsed since entering AS_HUM state
      if (millis() - sndSuppress > HUM_RELAUNCH and not hum_playing and ActionModeSubStates != AS_BLADELOCKUP) {
        HumRelaunch();
      }

#ifdef PIXELBLADE
      getColor(storage.sndProfile[storage.soundFont].mainColor);
      //lightFlicker(0, ActionModeSubStates);
#endif
      if (lockuponclash) {
        #ifdef ACCENT_LED
            accentLEDControl(AL_PULSE);
        #else if MULTICOLOR_ACCENT_LED
            accentLEDControl(AL_PULSE, currentColor);
        #endif
      }
    }
    // ************************* blade movement detection ends***********************************

  } ////END ACTION MODE HANDLER///////////////////////////////////////////////////////////////////////////////////////


  /*//////////////////////////////////////////////////////////////////////////////////////////////////////////
     CONFIG MODE HANDLER
  *//////////////////////////////////////////////////////////////////////////////////////////////////////////
  else if (SaberState == S_CONFIG) {
    if (PrevSaberState == S_STANDBY) { // entering config mode
      PrevSaberState = S_CONFIG;
      SinglePlay_Sound(3);
      delay(600);

/*#if defined PIXELBLADE
      pixelblade_KillKey_Disable();
#endif*/

#if defined LS_INFO
      Serial.println(F("START CONF"));
#endif
      enterMenu = true;
#ifdef BATTERY_CHECK
      ConfigModeSubStates = CS_BATTERYLEVEL;
      //      int batLevel = 100 * (1 / batCheck() - 1 / LOW_BATTERY) / (1 / FULL_BATTERY - 1 / LOW_BATTERY);
      int batLevel = 100 * ((batCheck() - LOW_BATTERY) / (FULL_BATTERY - LOW_BATTERY));
      Serial.println(batLevel);
      if (batLevel > 95) {        //full
        SinglePlay_Sound(19);
      } else if (batLevel > 60) { //nominal
        SinglePlay_Sound(15);
      } else if (batLevel > 30) { //diminished
        SinglePlay_Sound(16);
      } else if (batLevel > 0) {  //low
        SinglePlay_Sound(17);
      } else {                    //critical
        SinglePlay_Sound(18);
      }
      BladeMeter(batLevel);
      delay(500);
#else
      ConfigModeSubStates = CS_MAINCOLOR;
      SinglePlay_Sound(6);
      delay(500);
#endif
    }
#ifndef COLORS
#if defined(PIXELBLADE)
    if (ConfigModeSubStates == CS_MAINCOLOR or ConfigModeSubStates == CS_CLASHCOLOR or ConfigModeSubStates == CS_BLASTCOLOR) {
      modification = GravityVector();
      //Serial.println(modification);
      switch (modification) {
        case (0): // red +
          currentColor.r = 100; currentColor.g = 0; currentColor.b = 0;
          break;
        case (1): // red -
          currentColor.r = 20; currentColor.g = 0; currentColor.b = 0;
          break;
        case (2): // green +
          currentColor.r = 0; currentColor.g = 100; currentColor.b = 0;
          break;
        case (3): // green -
          currentColor.r = 0; currentColor.g = 20; currentColor.b = 0;
          break;
        case (4): // blue +
          currentColor.r = 0; currentColor.g = 0; currentColor.b = 100;
          break;
        case (5): // blue -
          currentColor.r = 0; currentColor.g = 0; currentColor.b = 20;
          break;
      }
      #ifdef PIXELBLADE
        lightOn(currentColor, NUMPIXELS - 5, NUMPIXELS);
      #endif
    }
#endif // PIXELBLADE
#endif // not COLORS

  } //END CONFIG MODE HANDLER

  /*//////////////////////////////////////////////////////////////////////////////////////////////////////////
     STANDBY MODE
  *//////////////////////////////////////////////////////////////////////////////////////////////////////////
  else if (SaberState == S_STANDBY) {

    if (ActionModeSubStates == AS_RETRACTION) { // we just leaved Action Mode
      //detachInterrupt(0);

      SinglePlay_Sound(soundFont.getPowerOff());
      ActionModeSubStates = AS_HUM;
      changeMenu = false;
      //ignition = false;
      //blasterBlocks = false;
      modification = 0;
#if defined LS_INFO
      Serial.println(F("END ACTION"));
#endif

#if defined PIXELBLADE
      lightRetract(soundFont.getPowerOffTime(), soundFont.getPowerOffEffect());
      //pixelblade_KillKey_Enable();
      //retractBlade();
#endif

    }
    if (PrevSaberState == S_CONFIG) { // we just leaved Config Mode
      saveConfig();
      PrevSaberState = S_STANDBY;

      /*
         RESET CONFIG
      */
      //			for (unsigned int i = 0; i < EEPROMSizeATmega328; i++) {
      //				//			 if (EEPROM.read(i) != 0) {
      //				EEPROM.update(i, 0);
      //				//			 }
      //			}

      SinglePlay_Sound(3);
      //browsing = false;
      enterMenu = false;
      modification = 0;
      //dfplayer.setVolume(storage.volume);
      menu = 0;

#if defined PIXELBLADE
      getColor(storage.sndProfile[storage.soundFont].mainColor);
#endif

#if defined LS_INFO
      Serial.println(F("END CONF"));
#endif
    }

    // switch of light in Stand-by mode
    lightOff();

#ifdef ACCENT_LED
    accentLEDControl(AL_ON);
#else if MULTICOLOR_ACCENT_LED
    accentLEDControl(AL_ON, currentColor);
#endif

  } // END STANDBY MODE

} //loop

// ====================================================================================
// ===           	  			MOTION DETECTION FUNCTIONS	            			===
// ====================================================================================
inline void motionEngine() {
  // if programming failed, don't try to do anything
  if (!dmpReady)
    return;

  // wait for MPU interrupt or extra packet(s) available
  //	while (!mpuInterrupt && mpuFifoCount < packetSize) {
  //		/* other program behavior stuff here
  //		 *
  //		 * If you are really paranoid you can frequently test in between other
  //		 * stuff to see if mpuInterrupt is true, and if so, "break;" from the
  //		 * while() loop to immediately process the MPU data
  //		 */
  //	}
  // reset interrupt flag and get INT_STATUS byte
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // get current FIFO count
  mpuFifoCount = mpu.getFIFOCount();

  // check for overflow (this should never happen unless our code is too inefficient)
  if ((mpuIntStatus & 0x10) || mpuFifoCount == 1024) {
    // reset so we can continue cleanly
    mpu.resetFIFO();

    // otherwise, check for DMP data ready interrupt (this should happen frequently)
  } else if (mpuIntStatus & 0x02) {
    // wait for correct available data length, should be a VERY short wait
    while (mpuFifoCount < packetSize)
      mpuFifoCount = mpu.getFIFOCount();

    // read a packet from FIFO
    mpu.getFIFOBytes(fifoBuffer, packetSize);

    // track FIFO count here in case there is > 1 packet available
    // (this lets us immediately read more without waiting for an interrupt)
    mpuFifoCount -= packetSize;

    //Making the last orientation the reference for next rotation
    prevOrientation = curOrientation.getConjugate();
    prevAccel = curAccel;

    //retrieve current orientation value
    mpu.dmpGetQuaternion(&curOrientation, fifoBuffer);
    mpu.dmpGetAccel(&curAccel, fifoBuffer);
    curDeltAccel.x = prevAccel.x - curAccel.x;
    curDeltAccel.y = prevAccel.y - curAccel.y;
    curDeltAccel.z = prevAccel.z - curAccel.z;

    //We calculate the rotation quaternion since last orientation
    prevRotation = curRotation;
    curRotation = prevOrientation.getProduct(
                    curOrientation.getNormalized());

#if defined LS_MOTION_HEAVY_DEBUG
    // display quaternion values in easy matrix form: w x y z
    printQuaternion(curRotation);
#endif

  }
} //motionEngine

inline void dmpDataReady() {
  mpuInterrupt = true;
} //dmpDataReady

#if defined LS_MOTION_DEBUG
inline void printQuaternion(Quaternion quaternion) {
  Serial.print(F("\t\tQ\t\tw="));
  Serial.print(quaternion.w * 1000);
  Serial.print(F("\t\tx="));
  Serial.print(quaternion.x);
  Serial.print(F("\t\ty="));
  Serial.print(quaternion.y);
  Serial.print(F("\t\tz="));
  Serial.println(quaternion.z);
} //printQuaternion
#endif

uint8_t GravityVector() {
  uint8_t Orientation; // 0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z
  int16_t ax, ay, az;

  //mpu.dmpGetAccel(&curAccel, fifoBuffer);
  mpu.getAcceleration(&ax, &ay, &az);
  //printAccel(ax, ay, az);
  if (ax < 0) {
    Orientation = 1; // -X
  }
  else {
    Orientation = 0; // +X
  }
  if (abs(abs(ax) - 16000) > abs(abs(ay) - 16000)) {
    if (ay < 0) {
      Orientation = 3; // -Y
    }
    else {
      Orientation = 2; // +Y
    }
  }
  if ( (abs(abs(ay) - 16000) > abs(abs(az) - 16000)) and (abs(abs(ax) - 16000) > abs(abs(az) - 16000)) ) {
    if (az < 0) {
      Orientation = 5; // -Z
    }
    else {
      Orientation = 4; // +Z
    }
  }
  //Serial.print(F("\t\Orientation="));
  //Serial.println(Orientation);
  return Orientation;
}

//#if defined LS_MOTION_DEBUG
//inline void printAccel(int16_t ax, int16_t ay, int16_t az) {
//  Serial.print(F("\t\tx="));
//  Serial.print(ax);
//  Serial.print(F("\t\ty="));
//  Serial.print(ay);
//  Serial.print(F("\t\tz="));
//  Serial.println(az);
//} //printQuaternion
//#endif

// ====================================================================================
// ===           	  			EEPROM MANIPULATION FUNCTIONS	            		===
// ====================================================================================

inline bool loadConfig() {
  bool equals = true;
  EEPROM.readBlock(configAdress, storage);
  for (uint8_t i = 0; i <= 2; i++) {
    if (storage.version[i] != CONFIG_VERSION[i]) {
      equals = false;
      Serial.println("Wrong config!");
    }
  }
  Serial.println(storage.version);
  return equals;
} //loadConfig

inline void saveConfig() {
  EEPROM.updateBlock(configAdress, storage);
#ifdef LS_DEBUG
  // dump values stored in EEPROM
  for (uint8_t i = 0; i < 255; i++) {
    Serial.print(i); Serial.print("\t"); Serial.println(EEPROM.readByte(i));
  }
#endif
} //saveConfig

// ====================================================================================
// ===                          SOUND FUNCTIONS                                     ===
// ====================================================================================

void HumRelaunch() {
  LoopPlay_Sound(soundFont.getHum());
  sndSuppress = millis();
  hum_playing = true;
}

void SinglePlay_Sound(uint8_t track) {
  myDFPlayer.play(track);
}

void LoopPlay_Sound(uint8_t track) {
  myDFPlayer.loop(track);
}

void Set_Volume() {
  myDFPlayer.volume(storage.volume);
  delay(50);
}

void Set_Loop_Playback() {
//myDFPlayer.loop(); <-- line to adapt ?
//mp3_single_loop(true); <-- old line
}

void InitDFPlayer() {
  mySoftwareSerial.begin(9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to communicate with DFPlayer:"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.volume(25);  //Set volume value. From 0 to 30
}

void Pause_Sound() {
  myDFPlayer.pause();
}

void Resume_Sound() {
   myDFPlayer.start();
}

// ====================================================================================
// ===                         BATTERY CHECKING FUNCTIONS                           ===
// ====================================================================================

#ifdef BATTERY_CHECK
float batCheck() {
  float sum = 0;
  // take a number of analog samples and add them up
  analogReference(INTERNAL);
  for (int i = 0; i < 10; i++) {
    analogRead(BATTERY_READPIN);  // clear the reads after reference switch
    delay(1);
  }
  for (int i = 0; i < 10; i++) {
    sum += analogRead(BATTERY_READPIN);
    //Serial.println(analogRead(BATTERY_READPIN));
    delay(10);
  }
  // 5.0V is the calibrated reference voltage
  float voltage = ((float)sum / 10 * BATTERY_FACTOR) / 1023.0;
  analogReference(DEFAULT);
  for (int i = 0; i < 10; i++) {
    analogRead(BATTERY_READPIN);  // clear the reads after reference switch
    delay(1);
  }
  Serial.print(F("Battery Level: ")); Serial.println(voltage);
  return voltage;
  // return 3.2; //temporary value for testing
}
#endif
/*
void deployBlade(cRGB color) {
  for (uint16_t i = 0 ; i < NUMPIXELS ; i ++ ) {
      pixels.set_crgb_at(i, color);
      pixels.sync();
      delay(10);
  }
}

void retractBlade() {
  for (uint16_t i = NUMPIXELS ; i > 0 ; i -- ) {
      pixels[i] = cRGB::Black;
      pixels.sync();
      delay(10);
  }*/

void initLedStrip() {
  pixels.setOutput(13);
}

void initCrystalLED() {
  #ifdef CRYSTAL_COMMON_ANODE
      pinMode(CRYSTAL_COMMON_ANODE, OUTPUT);
  #else
      pinMode(RED_CRYSTAL_LED, OUTPUT);
      pinMode(GREEN_CRYSTAL_LED, OUTPUT);
      pinMode(BLUE_CRYSTAL_LED, OUTPUT);
  #endif
}
