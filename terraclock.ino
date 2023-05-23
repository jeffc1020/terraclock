/* 
 * TERRACLOCK
 * A GPS-synchronized clock
 * Copyright 2023 Jeff Cutcher 
*/

#include <EEPROM.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GPS.h>

#define DEBOUNCE_TIME 150

#define OFFSET_ADDRESS 0
#define HR12_ADDRESS 4

#define TIME_MODE 0
#define SECONDS_MODE 1
#define ALARM_SET_MODE 2
#define TIME_ZONE_MODE 3
#define SET1224_MODE 4
const uint8_t modeCount = 5;

/* Pinouts */
const uint8_t gpsRx = 3;
const uint8_t gpsTx = 4;
const uint8_t modeButton = 6;
const uint8_t upButton = 7;
const uint8_t downButton = 8;
const uint8_t alarmBuzzer = 9;
const uint8_t fixLED = A7;
/* I2C 7-segment pins – SDA: 18, SCL: 19 */

/* Button debouncing */
volatile uint64_t modeBtnTime = 0;
volatile uint64_t lastModeBtnTime = 0;
volatile uint64_t upBtnTime = 0;
volatile uint64_t lastUpBtnTime = 0;
volatile uint64_t downBtnTime = 0;
volatile uint64_t lastDownBtnTime = 0;


/* Internal state */
volatile uint8_t currentMode = TIME_MODE;
int hourOffset = 0;
bool hr12 = true;
uint8_t alarmH = 0;
uint8_t alarmM = 0;
bool alarmEnabled = true;
bool alarmTripped = false;
bool alarmSilenced = false;
uint8_t brightness = 12;
bool displayOn = true;

uint64_t dispUpdateTimer = millis();
volatile uint64_t timeSinceInteraction = millis();

/* Flags (similar to OS signals) */
volatile uint8_t dispUpdateFlag = 0;
volatile uint8_t brightnessUpFlag = 0;
volatile uint8_t brightnessDownFlag = 0;
volatile uint8_t hourOffsetChangedFlag = 0;
volatile uint8_t hr12ChangedFlag = 0;

/* Library class initialization */
SoftwareSerial mySerial(gpsRx, gpsTx);
Adafruit_GPS GPS(&mySerial);
Adafruit_7segment disp = Adafruit_7segment();

void setup() {
  pinMode(modeButton, INPUT_PULLUP);
  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);

  pinMode(fixLED, OUTPUT);
  pinMode(alarmBuzzer, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(modeButton), modeButtonPress_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(upButton), upButtonPress_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(downButton), downButtonPress_ISR, FALLING);

  GPS.begin(9600); 
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  disp.begin(0x70);
  disp.setBrightness(brightness);

  /* Retrieve saved settings */
  int tempHourOffset;
  EEPROM.get(OFFSET_ADDRESS, tempHourOffset);
  if (tempHourOffset <= 12 && tempHourOffset >= -12)
    hourOffset = tempHourOffset;

  bool tempHr12;
  EEPROM.get(HR12_ADDRESS, tempHr12);
  hr12 = tempHr12 & 0x01;
}

void loop() {
  /* Read from GPS receiver */
  char c = GPS.read();
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());
    dispUpdateFlag = 1;
  }

  if (GPS.fix) {
    if (displayOn) 
      digitalWrite(fixLED, HIGH);
    else
      digitalWrite(fixLED, LOW);
  } else {
    digitalWrite(fixLED, LOW);
  }

  /* Trip the alarm */
  if (alarmEnabled && (alarmH == timeZoneCorrection(GPS.hour, hourOffset) 
    && alarmM == GPS.minute)) {
    alarmTripped = true;
  } else {
    alarmTripped = false;
    alarmSilenced = false;
  }

  /* Sound the alarm */
  if (alarmTripped && !alarmSilenced) {
    displayOn = true;
    long dividedMillis = millis() >> 8;
    if(dividedMillis % 2 == 0)
      turnOnBuzzer();
    else
      turnOffBuzzer();
  } else {
    turnOffBuzzer();
  }

  /* Button holding */
  if (digitalRead(upButton) == LOW  && (millis() - upBtnTime) > 200) {
    handleUpButtonPress();
    upBtnTime = millis();
  }

  if (digitalRead(downButton) == LOW  && (millis() - downBtnTime) > 200) {
    handleDownButtonPress();
    downBtnTime = millis();
  }

  /* Update display periodically or after some event */
  if (dispUpdateFlag || (millis() - dispUpdateTimer > 250)) {
    dispUpdateFlag = 0;
    dispUpdateTimer = millis();
    /* this should only be called here!!! */
    updateDisplay();
  }

  if (brightnessUpFlag) {
    if (!displayOn) {
      displayOn = true;
      dispUpdateFlag = 1;
    }
    brightnessUpFlag = 0;
  }

  if (brightnessDownFlag) {
    if (displayOn) {
      displayOn = false;
      dispUpdateFlag = 1;
    }
    brightnessDownFlag = 0;
  }

  if (hourOffsetChangedFlag) {
    EEPROM.put(OFFSET_ADDRESS, hourOffset);
    hourOffsetChangedFlag = 0;
  }

  if (hr12ChangedFlag) {
    EEPROM.put(HR12_ADDRESS, hr12);
    hr12ChangedFlag = 0;
  }

  /* Return to time display if 20 seconds have passed since the clock has been
    interacted with */
  if (millis() - timeSinceInteraction > 20000) {
    currentMode = TIME_MODE;
    timeSinceInteraction = millis();
    dispUpdateFlag = 1;
  }
}

void modeButtonPress_ISR() {
  timeSinceInteraction = millis();
  modeBtnTime = millis();
  /* Wake display if it is off */
  brightnessUpFlag = 1;
  if (modeBtnTime - lastModeBtnTime > DEBOUNCE_TIME) {
    if (alarmTripped && !alarmSilenced) {
      alarmSilenced = true;
    } else if (currentMode == modeCount) {
      currentMode = 0;
    } else {
      currentMode++;
    }
    lastModeBtnTime = modeBtnTime;
    dispUpdateFlag = 1;
  }
}

void upButtonPress_ISR() {
  upBtnTime = millis();
  if (upBtnTime - lastUpBtnTime > DEBOUNCE_TIME) {
    handleUpButtonPress();
  }
}

void downButtonPress_ISR() {
  downBtnTime = millis();
  if (downBtnTime - lastDownBtnTime > DEBOUNCE_TIME) {
    handleDownButtonPress();
  }
}

/* Perform actions that should occur when the up button is pressed */
void handleUpButtonPress() {
  timeSinceInteraction = millis();
  lastUpBtnTime = upBtnTime;
  if (alarmTripped && !alarmSilenced) {
    alarmSilenced = true;
  } else if (currentMode == TIME_MODE) {
    brightnessUpFlag = 1;
  } else if (currentMode == ALARM_SET_MODE) {
    if (alarmM == 59) {
      alarmM = 0;
    } else {
      ++alarmM;
    }
  } else if (currentMode == SET1224_MODE) {
    hr12 = !hr12;
    hr12ChangedFlag = 1;
  } else if (currentMode == TIME_ZONE_MODE) {
    if (hourOffset == 12)
      hourOffset = -12;
    else
      ++hourOffset;
    hourOffsetChangedFlag = 1;
  }
  dispUpdateFlag = 1;
}

/* Perform actions that should occur when the down button is pressed */
void handleDownButtonPress() {
  timeSinceInteraction = millis();
  lastDownBtnTime = downBtnTime;
  if (alarmTripped && !alarmSilenced) {
    alarmSilenced = true;
  } else if (currentMode == TIME_MODE) {
    brightnessDownFlag = 1;
  } else if (currentMode == ALARM_SET_MODE) {
    if (alarmH == 23) {
      alarmH = 0;
    } else {
      ++alarmH;
    }
  } else if (currentMode == SET1224_MODE) {
    hr12 = !hr12;
    hr12ChangedFlag = 1;
  } else if (currentMode == TIME_ZONE_MODE) {
    if (hourOffset == -12)
      hourOffset = 12;
    else
      --hourOffset;
    hourOffsetChangedFlag = 1;
  }
  dispUpdateFlag = 1;
}

void updateDisplay() {
  if (!displayOn) {
    disp.clear();
    disp.writeDisplay();
    return;
  }
  switch (currentMode) {
    case TIME_MODE:
      displayTime(GPS.hour, GPS.minute, hourOffset, hr12, 
        (GPS.seconds % 2 == 1));
      break;
    case SECONDS_MODE:
      displaySeconds(GPS.seconds);
      break;
    case ALARM_SET_MODE:
      displayTimeFast(alarmH, alarmM, hr12, true);
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

void displayTimeFast(uint8_t h, uint8_t m, bool hr12, bool showColon) {
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

void turnOnBuzzer() {
  analogWrite(alarmBuzzer, 120);
}

void turnOffBuzzer() {
  digitalWrite(alarmBuzzer, LOW);
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
