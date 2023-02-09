#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GPS.h>
//#include <SoftwareSerial.h>

/* TERRACLOCK
 * A robust GPS-synchronized clock
 * Copyright 2022 Jeff Cutcher
 */ 

// Pinouts 
const byte secsBtn = 2;
const byte rxPin = 3;
const byte txPin = 4;

int hourOffset = -4;
int minuteOffset = 0;

byte displayMode = 0;
/*
 * 0: normal time
 * 1: seconds
 * 2: alarm set
 * 3: time zone set
 * 4: 12hr / 24hr set
 * 5: brightness set
 */

unsigned long modeButtonTime = 0;
unsigned long lastModeButtonTime = 0;

bool neverSynched = true;

Adafruit_7segment disp = Adafruit_7segment();

SoftwareSerial mySerial(rxPin, txPin);
Adafruit_GPS GPS(&mySerial);

void setup() {
  pinMode(secsBtn, INPUT);

  GPS.begin(9600); // GPS module uses 9600 baud
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY); // get recommended minimum amount of data plus fix data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_2HZ);

  disp.begin(0x70);
  disp.setBrightness(0);

  attachInterrupt(digitalPinToInterrupt(secsBtn), cycleDisplayMode_ISR, FALLING);
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
    if(displayMode >= 5) {
    displayMode = 0;
    } else {
      displayMode++;
    }
  }
  lastModeButtonTime = modeButtonTime;
}

void updateDisplay() {
  switch(displayMode) {
    /* Time */
    case 0:
      if(neverSynched) {
          displayDashes();
      } else {
        displayTime(GPS.hour, GPS.minute, hourOffset, true, (GPS.seconds % 2 == 1));
      }
      break;
    /* Seconds */
    case 1:
      if(neverSynched) {
          displayDashes();
      } else {
        displaySeconds(GPS.seconds);
      }
      break;
    /* Alarm set */
    case 2:
      displayTime(0, 0, 0, true, true);
      break;
    /* Time zone */
    case 3:
      displayDashes();
      break;
    /* Info */
    case 4:
      displayDashes();
      break;
    case 5:
      displayDashes();
      break;
    default:
      displayMode = 0;
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
