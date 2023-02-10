#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GPS.h>
//#include <SoftwareSerial.h>

/* TERRACLOCK
 * A GPS-synchronized clock
 * Copyright 2023 Jeff Cutcher */

#define TIME_MODE 0
#define SECONDS_MODE 1
#define TIME_ZONE_MODE 2
#define SET1224_MODE 3

byte modeCount = 4;

byte displayMode = TIME_MODE;

// Pinouts 
const byte modeBtn = 2;
const byte upBtn = 5;
const byte rxPin = 3;
const byte txPin = 4;

// Time zone info (these must be ints)
int hourOffset = -5;
int minuteOffset = 0;

// Button debouncing
unsigned long modeButtonTime = 0;
unsigned long lastModeButtonTime = 0;
unsigned long upButtonTime = 0;
unsigned long lastUpButtonTime = 0;

// Has the clock been synched with GPS?
bool neverSynched = true;

bool hr12 = true;

Adafruit_7segment disp = Adafruit_7segment();

SoftwareSerial mySerial(rxPin, txPin);
Adafruit_GPS GPS(&mySerial);

void setup() {
  pinMode(modeBtn, INPUT);

  GPS.begin(9600); // GPS module uses 9600 baud
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY); // get recommended minimum amount of data plus fix data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_2HZ);

  disp.begin(0x70);
  disp.setBrightness(0);

  attachInterrupt(digitalPinToInterrupt(modeBtn), cycleDisplayMode_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(upBtn), upButtonPress_ISR, RISING);
}

uint32_t timer = millis();
unsigned long timeSinceInteraction = millis();
void loop() {
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());
  }

  if(GPS.fix) {
    neverSynched = false;
  }

  if(millis() - timer > 100) {
    timer = millis();
    updateDisplay();
  }

  if(millis() - timeSinceInteraction > 20000) {
    displayMode = TIME_MODE;
    timeSinceInteraction = millis();
  }
}

void cycleDisplayMode_ISR() {
  timeSinceInteraction = millis();
  modeButtonTime = millis();
  if(modeButtonTime - lastModeButtonTime > 200) {
    if(displayMode == modeCount) {
      displayMode = 0;
    } else {
      displayMode++;
    }
  lastModeButtonTime = modeButtonTime;
  }
}

void upButtonPress_ISR() {
  timeSinceInteraction = millis();
  upButtonTime = millis();
  if(upButtonTime - lastUpButtonTime > 150) {
    if(displayMode == SET1224_MODE) {
      hr12 = !hr12;
    } else if (displayMode == TIME_ZONE_MODE) {
      if (hourOffset == 12)
        hourOffset = -12;
      else
        ++hourOffset;
    }
    lastUpButtonTime = upButtonTime;
  }
}

void updateDisplay() {
  switch(displayMode) {
    case TIME_MODE:
      if(neverSynched) {
          displayDashes();
      } else {
        displayTime(GPS.hour, GPS.minute, hourOffset, hr12, (GPS.seconds % 2 == 1));
      }
      break;
    case SECONDS_MODE:
      if(neverSynched) {
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
      if(hr12) {
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
      displayMode = TIME_MODE;
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

/**
 * Offsets inputHour by timeOffset and returns the result. Does not handle dates (yet!)
 * 
 * @param inputHour The hour (usually GMT)
 * @param timeOffset Number of hours to add or subtract from inputHour
 * @return inputHour + timeOffset, rolling over if needed
 */
int timeZoneCorrection(int inputHour, int timeOffset) {
  int result = inputHour + timeOffset;
  if(result < 0) {
    return 24 + (result);
  } 
  else if(result > 23) {
    return (result - 24);
  } 
  else {
    return result;
  }
}
