#include <Adafruit_AS7341.h>
#include <Adafruit_AW9523.h>
#include <RTClib.h>
#include <SdFat.h>

Adafruit_AS7341 as7341;
Adafruit_AW9523 aw;
RTC_PCF8523 rtc;
File myFile;

float sumChlF8 = 0;
int countChlF8 = 0;

float sumTurbF8 = 0;
int countTurbF8 = 0;

float calibrated_chla = 0;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int SD_CHIP_SELECT = 10;

// setup aw9523
const uint8_t LedPins[] = {0, 1, 3, 10, 5};
const int NumLEDs = sizeof(LedPins) / sizeof(LedPins[0]);

int LEDbright = 255;
int LEDdim = 0;

SdFat SD;

void setup() {
  Serial.begin(9600);
  while (!Serial) delay(1);

  if (!as7341.begin(0x39)){
    Serial.println("Could not find AS7341");
    while (1) delay(10);
  }

  as7341.setATIME(255);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_256X);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (!rtc.initialized()) {
    Serial.println("RTC is NOT running!");
  }

  if (!aw.begin(0x58)) {
    Serial.println("AW9523 not found? Check wiring!");
    while (1) delay(10);
  }
  Serial.println("AW9523 found!");

  for (int i = 0; i < NumLEDs; i++) {
    aw.pinMode(LedPins[i], AW9523_LED_MODE);
  }

  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD init failed!");
  }

  File file = SD.open("Test.csv", FILE_WRITE);
  if (file) {
    file.println("Date and Time,ASTEP,ATIME,GAIN,LED Status,LED Intensity,Test,Seconds,Milliseconds,Type,F1 (Basic),F2 (Basic),F3 (Basic),F4 (Basic),F5 (Basic),F6 (Basic),F7 (Basic),F8 (Basic),Clear (Basic),NIR (Basic),Type,F1 (Raw),F2 (Raw),F3 (Raw),F4 (Raw),F5 (Raw),F6 (Raw),F7 (Raw),F8 (Raw),Clear (Raw),NIR (Raw),Measurement Type");
    file.close();
  } else {
    Serial.println(F("Error opening file!"));
  }
}

void loop() {
  // blink on led
  blinkLED(2, 2);
  Serial.println("Begin chlorophyll measurements.");

  // chla on
  turnOnChlLED(7, 60000, "Chlorophyll-a");

  blinkLED(2, 2);
  Serial.println("End chlorophyll measurements.");

  // blink led 2
  blinkLED(2, 3);
  Serial.println("Begin turbidity measurements.");

  // turb on
  turnOnLED(0, 60000, "Turbidity");

  blinkLED(2, 2);
  Serial.println("End turbidity measurements.");

  // Final LED states
  aw.analogWrite(LedPins[3], LEDdim);
  aw.analogWrite(LedPins[2], LEDbright);

  // calculations
  if (countChlF8 > 0 && countTurbF8 > 0) {
    float avgChlF8 = sumChlF8 / countChlF8;
    float avgTurbF8 = sumTurbF8 / countTurbF8;
    float diff = avgChlF8 - avgTurbF8;
    calibrated_chla = 0.1332 * diff - 1.7127;

    File file = SD.open("Test.csv", FILE_WRITE);
    if (file) {
      file.println("SUMMARY");
      file.print("Average F8 Raw Chlorophyll,"); file.println(avgChlF8);
      file.print("Average F8 Raw Turbidity,"); file.println(avgTurbF8);
      file.print("Chl - Turb F8,"); file.println(diff);
      file.print("Calibrated Chlorophyll-a,"); file.println(calibrated_chla);
      file.close();
    }

    Serial.println("==== FINAL AVERAGE RESULTS ====");
    Serial.print("Chlorophyll-a Avg. Raw Fluorescence (counts): "); Serial.println(avgChlF8);
    Serial.print("Avg. Background (counts): "); Serial.println(avgTurbF8);
    Serial.print("Background Subtracted Fluorescence (counts): "); Serial.println(diff);
    Serial.print("Calibrated Chlorophyll-a (μg/L): "); Serial.println(calibrated_chla);
    Serial.println("================================");
  }

  while (1) delay(1);
}

// === HELPER FUNCTIONS ===

void blinkLED(uint8_t ledIndex, int blinkCount) {
  for (int i = 0; i < blinkCount; i++) {
    aw.analogWrite(LedPins[ledIndex], LEDbright);
    delay(500);
    aw.analogWrite(LedPins[ledIndex], LEDdim);
    delay(500);
  }
}

void turnOnLED(uint8_t ledIndex, unsigned long duration, const char* measurementType) {
  unsigned long startTime = millis();
  unsigned long lastProgressTime = 0;
  const int barWidth = 30;

  while (millis() - startTime < duration) {
    for (int i = 0; i < NumLEDs; i++) {
      aw.analogWrite(LedPins[i], (i == ledIndex) ? LEDbright : LEDdim);
    }

    fileprintresults(SD, rtc, measurementType);
    serialprintresults(rtc, measurementType);

    unsigned long now = millis();
    if (now - lastProgressTime >= 1000) {
      float percent = float(now - startTime) / duration;
      if (percent > 1.0) percent = 1.0;
      int hashes = percent * barWidth;
      Serial.print(measurementType);
      Serial.print(": [");
      for (int i = 0; i < barWidth; ++i) Serial.print((i < hashes) ? "#" : " ");
      Serial.print("] ");
      Serial.print((int)(percent * 100)); Serial.print("%   ");
      lastProgressTime = now;
    }
  }
}

// chla gpio
void turnOnChlLED(uint8_t gpioPin, unsigned long duration, const char* measurementType) {
  unsigned long startTime = millis();
  unsigned long lastProgressTime = 0;
  const int barWidth = 30;

  pinMode(gpioPin, OUTPUT);

  while (millis() - startTime < duration) {
    digitalWrite(gpioPin, HIGH);

    fileprintresults(SD, rtc, measurementType);
    serialprintresults(rtc, measurementType);

    unsigned long now = millis();
    if (now - lastProgressTime >= 1000) {
      float percent = float(now - startTime) / duration;
      if (percent > 1.0) percent = 1.0;
      int hashes = percent * barWidth;
      Serial.print(measurementType);
      Serial.print(": [");
      for (int i = 0; i < barWidth; ++i) Serial.print((i < hashes) ? "#" : " ");
      Serial.print("] ");
      Serial.print((int)(percent * 100)); Serial.print("%   ");
      lastProgressTime = now;
    }
  }

  digitalWrite(gpioPin, LOW);
}
//print results
void fileprintresults(SdFat &sd, RTC_PCF8523 &rtc, const char* measurementType) {
  File file = sd.open("Test.csv", FILE_WRITE);
  DateTime now = rtc.now();

  uint16_t readings[12];
  float counts[12];

  if (!as7341.readAllChannels(readings)){
    Serial.println("Error reading all channels!");
    return;
  }

  for(uint8_t i = 0; i < 12; i++) {
    if(i == 4 || i == 5) continue;
    counts[i] = as7341.toBasicCounts(readings[i]);
  }

  if (file) {
    file.print(now.year(), DEC); file.print('/');
    file.print(now.month(), DEC); file.print('/');
    file.print(now.day(), DEC); file.print(" (");
    file.print(daysOfTheWeek[now.dayOfTheWeek()]); file.print(") ");
    file.print(now.hour(), DEC); file.print(':');
    file.print(now.minute(), DEC); file.print(':');
    file.print(now.second(), DEC); file.print(',');
    file.print(as7341.getASTEP()); file.print(',');
    file.print(as7341.getATIME()); file.print(',');
    file.print(as7341.getGain()); file.print(',');
    file.print("LED Status"); file.print(',');
    file.print("LED Intensity"); file.print(',');
    file.print("6.0"); file.print(',');
    file.print(millis() / 1000); file.print(',');
    file.print(millis()); file.print(',');
    file.print("BASIC COUNTS"); file.print(',');
    file.print(counts[0]); file.print(',');
    file.print(counts[1]); file.print(',');
    file.print(counts[2]); file.print(',');
    file.print(counts[3]); file.print(',');
    file.print(counts[6]); file.print(',');
    file.print(counts[7]); file.print(',');
    file.print(counts[8]); file.print(',');
    file.print(counts[9]); file.print(',');
    file.print(counts[10]); file.print(',');
    file.print(counts[11]); file.print(',');
    file.print("RAW VALUES"); file.print(',');
    file.print(readings[0]); file.print(',');
    file.print(readings[1]); file.print(',');
    file.print(readings[2]); file.print(',');
    file.print(readings[3]); file.print(',');
    file.print(readings[6]); file.print(',');
    file.print(readings[7]); file.print(',');
    file.print(readings[8]); file.print(',');
    file.print(readings[9]); file.print(',');
    file.print(readings[10]); file.print(',');
    file.print(readings[11]); file.print(',');
    file.println(measurementType);
    file.close();
  } else {
    Serial.println(F("Error opening file!"));
  }
}

void serialprintresults(RTC_PCF8523 &rtc, const char* measurementType) {
  DateTime now = rtc.now();
  uint16_t readings[12];

  if (!as7341.readAllChannels(readings)){
    Serial.println("Error reading all channels!");
    return;
  }

  if (strcmp(measurementType, "Chlorophyll-a") == 0) {
    Serial.print("Raw F8: "); Serial.println(readings[9]);
    sumChlF8 += readings[9]; countChlF8++;
  } else if (strcmp(measurementType, "Turbidity") == 0) {
    Serial.print("Raw NIR: "); Serial.println(readings[11]);
    sumTurbF8 += readings[9]; countTurbF8++;
  }
}