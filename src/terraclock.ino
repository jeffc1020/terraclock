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

int currentHour = -1;
int currentMinute = -1;

int timeZone = -4;

const int segA = 2;
const int segB = 3;
const int segC = 4;
const int segD = 5;
const int segE = 6;
const int segF = 7;
const int segG = 8;
const int colon = 9;

const int dig1 = A0;
const int dig2 = A1;
const int dig3 = A2;
const int dig4 = A3;

const int secsBtn = A5;

SoftwareSerial mySerial(11, 10);
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
  // Serial.begin(115200);
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

uint32_t timer = millis();
void loop() {
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA()); // parsing is working!! somehow
    currentHour = GPS.hour;
    currentMinute = GPS.minute;
    setTime(timeZoneCorrection(GPS.hour, timeZone), GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);
    // adjustTime(timeZone * SECS_PER_HOUR);
  }

  if(millis() - timer > 1000) { // if a second has elapsed...
    timer = millis();           // reset timer
  }
  
  if(!GPS.fix) {              // display dashes if there is no GPS fix
    sevseg.setChars("----");
    sevseg.refreshDisplay();
    digitalWrite(colon, HIGH);
  } else {
    if(digitalRead(secsBtn) == HIGH) { // display seconds
      sevseg.setNumber(second());
      sevseg.setChars(formatSeconds(second()).c_str());
      sevseg.refreshDisplay();
      digitalWrite(colon, LOW);
    } else {                                // display normal time
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
  if(inputHour + timeOffset < 0) {
    return 24 + (inputHour + timeOffset);
  } else if(inputHour + timeOffset > 23) {
    return (inputHour + timeOffset - 24);
  } else {
    return inputHour + timeOffset;
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
  String result = "";
  String newHr;
  String newMin;
  if(hr12) {
    if(h == 0 || h == 12) {
      newHr = 12;
    } else {
      newHr = h % 12;
    }
  } else {
    if(h < 10) {
      newHr = "0" + h;
    } else {
      newHr = h;
    }
  }
  if(m < 10) {
    newMin = "0" + String(m);
  } else {
    newMin = m;
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
