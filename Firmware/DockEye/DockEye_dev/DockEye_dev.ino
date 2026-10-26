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

float sumCdomF2 = 0;
int countCdomF2 = 0;

float sumBG_F8 = 0;
int countBG_F8 = 0;
float sumBG_F2 = 0;
int countBG_F2 = 0;

float calibrated_chla = 0;
float calibrated_cdom = 0;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int SD_CHIP_SELECT = 10;

const uint8_t LedPins[] = {0, 1, 3, 10, 5};
const int NumLEDs = sizeof(LedPins) / sizeof(LedPins[0]);

int LEDbright = 255;
int LEDdim = 0;

const int STEMMA_POWER_PIN = 7;

SdFat SD;

void setup() {
  Serial.begin(9600);
  while (!Serial) { delay(1); }

  pinMode(STEMMA_POWER_PIN, OUTPUT);
  digitalWrite(STEMMA_POWER_PIN, LOW);

  if (!as7341.begin(0x39)) {
    Serial.println("Could not find AS7341");
    while (1) { delay(10); }
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

  SD.begin(SD_CHIP_SELECT);

  File file = SD.open("DockEye Measurements.csv", FILE_WRITE);
  if (file) {
    file.println("Date and Time,ASTEP,ATIME,GAIN,LED Status,LED Intensity,Test,Seconds,Milliseconds,Type,F1 (Basic),F2 (Basic),F3 (Basic),F4 (Basic),F5 (Basic),F6 (Basic),F7 (Basic),F8 (Basic),Clear (Basic),NIR (Basic),Type,F1 (Raw),F2 (Raw),F3 (Raw),F4 (Raw),F5 (Raw),F6 (Raw),F7 (Raw),F8 (Raw),Clear (Raw),NIR (Raw),Measurement Type");
    file.close();
  } else {
    Serial.println(F("Error opening file!"));
  }
}

void loop() {
  // bg run all leds off
  blinkLED(2, 1);
  Serial.println("Begin background measurements.");

  // make sure all are off
  digitalWrite(STEMMA_POWER_PIN, LOW);
  for (int i = 0; i < NumLEDs; i++) {
    aw.analogWrite(LedPins[i], LEDdim);
  }

  runMeasurements(60000, "Background");

  blinkLED(2, 1);
  Serial.println("End background measurements.");

  // chlorophyll measurements
  blinkLED(2, 2);
  Serial.println("Begin chlorophyll measurements.");

  digitalWrite(STEMMA_POWER_PIN, HIGH);
  delay(50);
  runMeasurements(60000, "Chlorophyll-a");
  digitalWrite(STEMMA_POWER_PIN, LOW);

  blinkLED(2, 2);
  Serial.println("End chlorophyll measurements.");

    // double check all LEDs are off
  digitalWrite(STEMMA_POWER_PIN, LOW);
  for (int i = 0; i < NumLEDs; i++) {
    aw.analogWrite(LedPins[i], LEDdim);
  }

  // filter sample
  Serial.println();
  Serial.println("==== FILTER SAMPLE NOW ====");
  Serial.println("After filtering, press enter in Serial Monitor to start CDOM measurements...");
  while (Serial.available()) Serial.read();
  while (!Serial.available()) { delay(100); }
  while (Serial.available()) Serial.read();

  // cdom run
  blinkLED(2, 3);
  Serial.println("Begin CDOM measurements.");

  for (int i = 0; i < NumLEDs; i++) {
    if (i == 0) {
      aw.analogWrite(LedPins[i], LEDbright);
    } else {
      aw.analogWrite(LedPins[i], LEDdim);
    }
  }

  runMeasurements(60000, "CDOM");

  for (int i = 0; i < NumLEDs; i++) {
    aw.analogWrite(LedPins[i], LEDdim);
  }

  blinkLED(2, 2);
  Serial.println("End CDOM measurements.");

  // status led
  aw.analogWrite(LedPins[3], LEDdim);
  aw.analogWrite(LedPins[2], LEDbright);

  // summary
  if (countChlF8 > 0 && countCdomF2 > 0 && countBG_F8 > 0 && countBG_F2 > 0) {
    float avgChlF8  = sumChlF8  / countChlF8;
    float avgCdomF2 = sumCdomF2 / countCdomF2;
    float avgBG_F8  = sumBG_F8  / countBG_F8;
    float avgBG_F2  = sumBG_F2  / countBG_F2;

    float chlDiff  = avgChlF8  - avgBG_F8;
    float cdomDiff = avgCdomF2 - avgBG_F2;

    float calibrated_chla = 0.0871 * chlDiff - 1.3054;
    float calibrated_cdom = 0.0081 * cdomDiff - 11.787; // update coefficients when calibrated

    File file = SD.open("DockEye Measurements.csv", FILE_WRITE);
    if (file) {
      file.println("SUMMARY");
      file.print("Average F8 Raw Counts (Chl),");
      file.println(avgChlF8);
      file.print("Average F8 Background,");
      file.println(avgBG_F8);
      file.print("Background Subtracted F8,");
      file.println(chlDiff);
      file.print("Calibrated Chlorophyll-a,");
      file.println(calibrated_chla);
      file.print("Average F2 Raw Counts (CDOM),");
      file.println(avgCdomF2);
      file.print("Average F2 Background,");
      file.println(avgBG_F2);
      file.print("Background Subtracted F2,");
      file.println(cdomDiff);
      file.print("Calibrated CDOM,");
      file.println(calibrated_cdom);
      file.close();
    }

    Serial.println("==== FINAL AVERAGE RESULTS ====");
    Serial.print("F8 Avg. Raw Counts: ");
    Serial.println(avgChlF8);
    Serial.print("F8 Avg. Background: ");
    Serial.println(avgBG_F8);
    Serial.print("Background Subtracted F8: ");
    Serial.println(chlDiff);
    Serial.print("Calibrated Chlorophyll-a (ug/L): ");
    Serial.println(calibrated_chla);
    Serial.println("================================");
    Serial.print("F2 Avg. Raw Counts: ");
    Serial.println(avgCdomF2);
    Serial.print("F2 Avg. Background: ");
    Serial.println(avgBG_F2);
    Serial.print("Background Subtracted F2: ");
    Serial.println(cdomDiff);
    Serial.print("Calibrated CDOM: ");
    Serial.println(calibrated_cdom);
    Serial.println("================================");
  }

  while (1) { delay(1); }
}

// measurement loop
void runMeasurements(unsigned long duration, const char* measurementType) {
  unsigned long startTime = millis();
  unsigned long lastProgressTime = 0;
  const int barWidth = 30;

  while (millis() - startTime < duration) {
    fileprintresults(SD, rtc, measurementType);
    serialprintresults(rtc, measurementType);

    unsigned long now = millis();
    if (now - lastProgressTime >= 1000) {
      float percent = (float)(now - startTime) / duration;
      if (percent > 1.0) percent = 1.0;
      int hashes = (int)(percent * barWidth);

      Serial.print(measurementType);
      Serial.print(": [");
      for (int i = 0; i < barWidth; ++i) {
        if (i < hashes) Serial.print("#");
        else Serial.print(" ");
      }
      Serial.print("] ");
      Serial.print((int)(percent * 100));
      Serial.println("%");

      lastProgressTime = now;
    }
  }
}

void blinkLED(uint8_t ledIndex, int blinkCount) {
  for (int i = 0; i < blinkCount; i++) {
    aw.analogWrite(LedPins[ledIndex], LEDbright);
    delay(500);
    aw.analogWrite(LedPins[ledIndex], LEDdim);
    delay(500);
  }
}

void fileprintresults(SdFat &sd, RTC_PCF8523 &rtc, const char* measurementType) {
  File file = sd.open("DockEye Measurements.csv", FILE_WRITE);
  DateTime now = rtc.now();

  uint16_t readings[12];
  float counts[12];

  if (!as7341.readAllChannels(readings)) {
    Serial.println("Error reading all channels!");
    return;
  }

  for (uint8_t i = 0; i < 12; i++) {
    if (i == 4 || i == 5) continue;
    counts[i] = as7341.toBasicCounts(readings[i]);
  }

  if (file) {
    file.print(now.year(), DEC);
    file.print('/');
    file.print(now.month(), DEC);
    file.print('/');
    file.print(now.day(), DEC);
    file.print(" (");
    file.print(daysOfTheWeek[now.dayOfTheWeek()]);
    file.print(") ");
    file.print(now.hour(), DEC);
    file.print(':');
    file.print(now.minute(), DEC);
    file.print(':');
    file.print(now.second(), DEC);
    file.print(',');
    file.print(as7341.getASTEP());
    file.print(',');
    file.print(as7341.getATIME());
    file.print(',');
    file.print(as7341.getGain());
    file.print(',');
    file.print("LED Status");
    file.print(',');
    file.print("LED Intensity");
    file.print(',');
    file.print("6.0");
    file.print(',');
    file.print(millis() / 1000);
    file.print(',');
    file.print(millis());
    file.print(',');
    file.print("BASIC COUNTS");
    file.print(',');
    file.print(counts[0]);
    file.print(',');
    file.print(counts[1]);
    file.print(',');
    file.print(counts[2]);
    file.print(',');
    file.print(counts[3]);
    file.print(',');
    file.print(counts[6]);
    file.print(',');
    file.print(counts[7]);
    file.print(',');
    file.print(counts[8]);
    file.print(',');
    file.print(counts[9]);
    file.print(',');
    file.print(counts[10]);
    file.print(',');
    file.print(counts[11]);
    file.print(',');
    file.print("RAW VALUES");
    file.print(',');
    file.print(readings[0]);
    file.print(',');
    file.print(readings[1]);
    file.print(',');
    file.print(readings[2]);
    file.print(',');
    file.print(readings[3]);
    file.print(',');
    file.print(readings[6]);
    file.print(',');
    file.print(readings[7]);
    file.print(',');
    file.print(readings[8]);
    file.print(',');
    file.print(readings[9]);
    file.print(',');
    file.print(readings[10]);
    file.print(',');
    file.print(readings[11]);
    file.print(',');
    file.println(measurementType);
    file.close();
  } else {
    Serial.println(F("Error opening file!"));
  }
}

void serialprintresults(RTC_PCF8523 &rtc, const char* measurementType) {
  uint16_t readings[12];

  if (!as7341.readAllChannels(readings)) {
    Serial.println("Error reading all channels!");
    return;
  }

  if (strcmp(measurementType, "Background") == 0) {
    Serial.print("BG F8: ");
    Serial.println(readings[9]);
    sumBG_F8 += readings[9];
    countBG_F8++;
    Serial.print("BG F2: ");
    Serial.println(readings[1]);
    sumBG_F2 += readings[1];
    countBG_F2++;
  } else if (strcmp(measurementType, "Chlorophyll-a") == 0) {
    Serial.print("Raw F8: ");
    Serial.println(readings[9]);
    sumChlF8 += readings[9];
    countChlF8++;
  } else if (strcmp(measurementType, "CDOM") == 0) {
    Serial.print("Raw F2: ");
    Serial.println(readings[1]);
    sumCdomF2 += readings[1];
    countCdomF2++;
  }
}