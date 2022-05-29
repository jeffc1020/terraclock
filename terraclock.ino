#include <TimeLib.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <SevSeg.h>

/* TERRACLOCK
 * A robust GPS-synchronized clock
 * Copyright 2022 Jeff Cutcher
 * 
 * 595 Shift register IC pinout
 *      ---o---
 *  Qb |1    16| Vcc
 *  Qc |2    15| Qa
 *  Qd |3    14| SER (DATA)
 *  Qe |4    13| OE (Output Enable) (pulled low)
 *  Qf |5    12| RCLK (LATCH)
 *  Qg |6    11| SRCLK (CLOCK)
 *  Qh |7    10| SRCLR (held high)
 * GND |8     9| Qh' (daisy chain)
 *      -------
 * 
 * LuckyLight KW4-56NCHGA-P 7-segment display pinout
 * FRONT VIEW
 * 
 *     -   +   +   -   -   +   +
 *    h10  a   f  h1  m10  b   :
 *   --|---|---|---|---|---|---|--
 *  |                             |
 *   --|---|---|---|---|---|---|--
 *     e   d  dot  c   g   m1  :
 *     +   +   +   +   +   -   -
 *     
 *  max 15mA per segment (333 ohms at 5v)
 *  1K ohms works great
 *  2K ohms is an even more reasonable brightness
 */ 

const byte segA = 2;
const byte segB = 3;
const byte segC = 4;
const byte segD = 5;
const byte segE = 6;
const byte segF = 7;
const byte segG = 8;
const byte colon = 9;
const byte dig1 = A0;
const byte dig2 = A1;
const byte dig3 = A2;
const byte dig4 = A3;
const byte secsBtn = A5;
const byte rxPin = 11;
const byte txPin = 10;

int currentHour = -1;
int currentMinute = -1;
int timeZone = -4;
bool neverSynched = true;

SoftwareSerial mySerial(rxPin, txPin);
Adafruit_GPS GPS(&mySerial);
SevSeg sevseg;

void setup() {
  pinMode(segA, OUTPUT);
  pinMode(segB, OUTPUT);
  pinMode(segC, OUTPUT);
  pinMode(segD, OUTPUT);
  pinMode(segE, OUTPUT);
  pinMode(segF, OUTPUT);
  pinMode(segG, OUTPUT);
  pinMode(colon, OUTPUT);
  pinMode(dig1, OUTPUT);
  pinMode(dig2, OUTPUT);
  pinMode(dig3, OUTPUT);
  pinMode(dig4, OUTPUT);
  pinMode(secsBtn, INPUT);
  
  Serial.begin(115200);
  GPS.begin(9600); // GPS module uses 9600 baud
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY); // get recommended minimum amount of data plus fix data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ); // updates once every 10 seconds

  byte numDigits = 4;
  byte digitPins[] = {dig1, dig2, dig3, dig4};
  byte segmentPins[] = {segA, segB, segC, segD, segE, segF, segG};
  bool resistorsOnSegments = true;
  byte hardwareConfig = COMMON_CATHODE;
  bool updateWithDelays = false;
  bool leadingZeros = false;
  bool disableDecPoint = true;

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays,
    leadingZeros, disableDecPoint);
}

void loop() {
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA()); // parsing is working!! somehow
    currentHour = GPS.hour;
    currentMinute = GPS.minute;
    setTime(timeZoneCorrection(GPS.hour, timeZone), GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
  }

  if(GPS.fix) {
    neverSynched = false;
  }

  updateDisplay();
}

void updateDisplay() {
  if(!GPS.fix && neverSynched) {  // display dashes if there is no GPS fix and RTC never received correct time
    sevseg.setChars("----");
    sevseg.refreshDisplay();
    digitalWrite(colon, HIGH);
  } else {
    if(digitalRead(secsBtn) == HIGH) { // display seconds
      sevseg.setChars(formatSeconds(second()).c_str());
      Serial.write(formatSecondsC(second()));
      Serial.write("\n");
      sevseg.refreshDisplay();
      digitalWrite(colon, LOW);
    } else {                           // display normal time
      sevseg.setChars(formatTime(hour(), minute(), true).c_str());
      // sevseg.setNumber(hour() * 100 + minute());
      sevseg.refreshDisplay();
      if(second() % 2 == 0) {  // blinks colon
        digitalWrite(colon, HIGH);
      } else {
        digitalWrite(colon, LOW);
      }
    }
  }
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
  } else if(result > 23) {
    return (result - 24);
  } else {
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
    } else {
      newHr = String(h % 12);
    }
  } else {
    if(h < 10) {
      newHr = "0" + String(h);
    } else {
      newHr = String(h);
    }
  }
  if(m < 10) {
    newMin = "0" + String(m);
  } else {
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
  } else {
    return "  " + String(s);
  }
}

char formatSecondsC(int s) {
  char secondsString[10];
  sprintf(secondsString, "%d", s);
  return secondsString;
}
