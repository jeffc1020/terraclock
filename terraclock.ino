#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GPS.h>
//#include <SoftwareSerial.h>

/* TERRACLOCK
 * A GPS-synchronized clock
 * Copyright 2023 Jeff Cutcher */

#define TIME_MODE 0
#define SECONDS_MODE 1
#define TIME_ZONE_MODE 2

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
unsigned int modeButtonTime = 0;
unsigned int lastModeButtonTime = 0;
unsigned int upButtonTime = 0;
unsigned int lastUpButtonTime = 0;

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
}

void cycleDisplayMode_ISR() {
  modeButtonTime = millis();
  if(modeButtonTime - lastModeButtonTime > 200) {
    if(displayMode == TIME_ZONE_MODE) {
    displayMode = 0;
    } else {
      displayMode++;
    }
  lastModeButtonTime = modeButtonTime;
  }
}

void upButtonPress_ISR() {
  upButtonTime = millis();
  if(upButtonTime - lastUpButtonTime > 150) {
    if(displayMode == TIME_MODE) {
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
  }
  disp.print(h * 100 + m, DEC);
  if (isPm) {
    disp.writeDigitNum(4, m % 10, true);
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
