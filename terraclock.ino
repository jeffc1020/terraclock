#include <TimeLib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>

/* TERRACLOCK
 * A robust GPS-synchronized clock
 * Copyright 2022 Jeff Cutcher
 */ 

// Pinouts
const byte secsBtn = A5;
const byte rxPin = 2;
const byte txPin = 3;

time_t currentTime = 0;
int hourOffset = -4;
int minuteOffset = 0;
bool neverSynched = true;
int timeZone = -4;

byte displayMode = 0;

Adafruit_7segment matrix = Adafruit_7segment();

SoftwareSerial mySerial(rxPin, txPin);
Adafruit_GPS GPS(&mySerial);

void setup() {
  pinMode(secsBtn, INPUT);

  matrix.begin(0x70);

  while (!Serial);
  Serial.begin(115200);
  GPS.begin(9600); // GPS module uses 9600 baud
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY); // get recommended minimum amount of data plus fix data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // updates once every 10 seconds
}

uint32_t timer = millis();
void loop() {
  char c = GPS.read();
  if (c) {
    Serial.write(c);
  }
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA()); // parsing is working!! somehow
    currentTime = now();
    currentTime += timeZone * 3600;
    setTime(GPS.hour, GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
    matrix.print(GPS.seconds, DEC);
    matrix.writeDisplay();
  }

  if(millis() - timer > 2000) {
    timer = millis();
    Serial.write(c);
  }
  
  if(GPS.fix) {
    neverSynched = false;
  }

  // updateDisplay();  
}

void updateDisplay() {
  if(neverSynched) {
    matrix.print("----");
    matrix.writeDisplay();
  } else {
    matrix.print(formatTime(hour(), minute(), true).c_str());
    matrix.writeDisplay();
  }
  /*
  else {
    if(digitalRead(secsBtn) == HIGH) {
      sevseg.setChars(formatSeconds(second()).c_str());
      sevseg.refreshDisplay();
      digitalWrite(colon, LOW);
    } 
    else {
      sevseg.setChars(formatTime(hour(), minute(), true).c_str());
      // sevseg.setNumber(hour() * 100 + minute());
      sevseg.refreshDisplay();
      if(second() % 2 == 0) {
        digitalWrite(colon, HIGH);
      } 
      else {
        digitalWrite(colon, LOW);
      }
    }
  }*/
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

/**
 * Returns a 4 character string of the properly formatted time.
 *
 * @param h The hour
 * @param m The minute
 * @param 12hr Whether the returned string should be in 12 hour format
 * @return 4-character String of the properly formatted time to display on a 7-segment display
 * 
 * Examples:
 * formatTime(0, 9, false) -> "0009"
 * formatTime(0, 9, true) -> "1209"
 * formatTime(15, 30, false) -> "1530"
 * formatTime(15, 30, true) -> " 230"
*/
String formatTime(int h, int m, bool hr12) {
  String newHr;
  String newMin;
  if(hr12) {
    if(h == 0 || h == 12) {
      newHr = "12";
    } 
    else {
      newHr = String(h % 12);
    }
  } 
  else {
    if(h < 10) {
      newHr = "0" + String(h);
    } 
    else {
      newHr = String(h);
    }
  }
  if(m < 10) {
    newMin = "0" + String(m);
  } 
  else {
    newMin = String(m);
  }
  if(newHr.length() == 1) {
    return (" " + newHr + newMin);
  }
  return (newHr + newMin);
}

/**
 * Returns a 4 character string of the properly formatted seconds.
 * 
 * @param s The seconds
 * @return The properly formatted seconds.
 * 
 * Examples:
 * formatSeconds(5) -> "  05"
 * formatSeconds(43) -> "  43"
 */
String formatSeconds(int s) {
  if(s < 10) {
    return "  0" + String(s);
  } 
  else {
    return "  " + String(s);
  }
}
