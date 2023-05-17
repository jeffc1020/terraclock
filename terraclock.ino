/* 
 * TERRACLOCK
 * A GPS-synchronized clock
 * Copyright 2023 Jeff Cutcher 
*/

#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GPS.h>
//#include <SoftwareSerial.h>

#define TIME_MODE 0
#define SECONDS_MODE 1
#define TIME_ZONE_MODE 2
#define SET1224_MODE 3
const uint8_t modeCount = 4;
volatile uint8_t currentMode = TIME_MODE;

/* Pinouts */
const uint8_t modeBtn = 6;
const uint8_t upBtn = 7;
const uint8_t dwnBtn = 8;
const uint8_t gpsRx = 3;
const uint8_t gpsTx = 4;
/* I2C 7-segment pins:
 * SDA: 18
 * SCL: 19 */

/* Button debouncing */
volatile uint64_t modeBtnTime = 0;
volatile uint64_t lastModeBtnTime = 0;
volatile uint64_t upBtnTime = 0;
volatile uint64_t lastUpBtnTime = 0;
volatile uint64_t dwnBtnTime = 0;
volatile uint64_t lastDwnBtnTime = 0;

bool neverSynched = true;

bool hr12 = true;

/* Time zone info (these must be ints) */
int hourOffset = -5;
int minuteOffset = 0;

Adafruit_7segment disp = Adafruit_7segment();

SoftwareSerial mySerial(gpsRx, gpsTx);
Adafruit_GPS GPS(&mySerial);

void setup() {
  pinMode(modeBtn, INPUT_PULLUP);
  pinMode(upBtn, INPUT_PULLUP);
  pinMode(dwnBtn, INPUT_PULLUP);

  GPS.begin(9600); 
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  disp.begin(0x70);
  disp.setBrightness(0);

  attachInterrupt(digitalPinToInterrupt(modeBtn), cycleDisplayMode_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(upBtn), upButtonPress_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(dwnBtn), dwnButtonPress_ISR, FALLING);
}

uint64_t dispUpdateTimer = millis();
volatile uint8_t dispUpdateFlag = 0;
volatile uint64_t timeSinceInteraction = millis();

void loop() {

  /* Read from GPS receiver */
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());
    dispUpdateFlag = 1;
  }

  if (GPS.fix)
    neverSynched = false;

  /* Update display periodically or after some event */
  if (dispUpdateFlag || (millis() - dispUpdateTimer > 250)) {
    dispUpdateFlag = 0;
    dispUpdateTimer = millis();
    updateDisplay();
  }

  /* Return to time display if 20 seconds have passed since the clock has been
    interacted with */
  if (millis() - timeSinceInteraction > 20000) {
    currentMode = TIME_MODE;
    timeSinceInteraction = millis();
  }
}

void cycleDisplayMode_ISR() {
  timeSinceInteraction = millis();
  modeBtnTime = millis();
  if (modeBtnTime - lastModeBtnTime > 200) {
    if (currentMode == modeCount) {
      currentMode = 0;
    } else {
      currentMode++;
    }
    lastModeBtnTime = modeBtnTime;
    dispUpdateFlag = 1;
  }
}

void upButtonPress_ISR() {
  timeSinceInteraction = millis();
  upBtnTime = millis();
  if (upBtnTime - lastUpBtnTime > 150) {
    if (currentMode == SET1224_MODE) {
      hr12 = !hr12;
    } else if (currentMode == TIME_ZONE_MODE) {
      if (hourOffset == 12)
        hourOffset = -12;
      else
        ++hourOffset;
    }
    lastUpBtnTime = upBtnTime;
    dispUpdateFlag = 1;
  }
}

void dwnButtonPress_ISR() {
  timeSinceInteraction = millis();
  dwnBtnTime = millis();
  if (dwnBtnTime - lastDwnBtnTime > 150) {
    if (currentMode == SET1224_MODE) {
      hr12 = !hr12;
    } else if (currentMode == TIME_ZONE_MODE) {
      if (hourOffset == -12)
        hourOffset = 12;
      else
        --hourOffset;
    }
    lastDwnBtnTime = dwnBtnTime;
    dispUpdateFlag = 1;
  }
}

void updateDisplay() {
  switch (currentMode) {
    case TIME_MODE:
      if (neverSynched) {
          displayDashes();
      } else {
        displayTime(GPS.hour, GPS.minute, hourOffset, hr12, 
          (GPS.seconds % 2 == 1));
      }
      break;
    case SECONDS_MODE:
      if (neverSynched) {
          displayDashes();
      } else {
        displaySeconds(GPS.seconds);
      }
      break;
    case TIME_ZONE_MODE:
      disp.print(hourOffset);
      disp.writeDisplay();
      break;
    case SET1224_MODE:
      if (hr12) {
        disp.writeDigitAscii(0, '1', false);
        disp.writeDigitAscii(1, '2', false);
        disp.writeDigitAscii(3, 'h', false);
        disp.writeDigitAscii(4, 'r', false);
      } else {
        disp.writeDigitAscii(0, '2', false);
        disp.writeDigitAscii(1, '4', false);
        disp.writeDigitAscii(3, 'h', false);
        disp.writeDigitAscii(4, 'r', false);
      }
      disp.writeDisplay();
      break;
    default:
      currentMode = TIME_MODE;
      break;
  }
}

void displayTime(int h, int m, int timeOffset, bool hr12, bool showColon) {
  h = timeZoneCorrection(h, timeOffset);
  bool isPm = false;
  if (hr12) {
    if (h >= 12) {
      isPm = true;
      if (h > 12) {
        h = h - 12;
      }
    } else if (h == 0) {
      h = 12;
    }
    disp.print(h * 100 + m, DEC);
    if (isPm) {
      disp.writeDigitNum(4, m % 10, true);
    }
  } else {
    disp.writeDigitNum(0, h / 10, false);
    disp.writeDigitNum(1, h % 10, false);
    disp.writeDigitNum(3, m / 10, false);
    disp.writeDigitNum(4, m % 10, false);
  }
  
  disp.drawColon(showColon);
  disp.writeDisplay();
}

void displayDashes() {
  disp.writeDigitRaw(0, 0b01000000);
  disp.writeDigitRaw(1, 0b01000000);
  disp.writeDigitRaw(2, 0b01000000);
  disp.writeDigitRaw(3, 0b01000000);
  disp.writeDigitRaw(4, 0b01000000);
  disp.drawColon(true);
  disp.writeDisplay();
}

void displaySeconds(byte sec) {
  disp.print(sec, 10);
  if (sec < 10) {
    disp.writeDigitNum(3, 0);
  }
  disp.writeDisplay();
}

/* Offsets inputHour by timeOffset and returns the result. 
  Does not handle dates (yet!) */
int timeZoneCorrection(int inputHour, int timeOffset) {
  int result = inputHour + timeOffset;
  if (result < 0) {
    return 24 + result;
  } 
  else if (result > 23) {
    return result - 24;
  } 
  else {
    return result;
  }
}
