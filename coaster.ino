#include <EEPROM.h>

#define AREF 3.3  // we tie 3.3V to  ARef and measure it with a multimeter!

//Vout TMP36 > A1
//3.3V > V+ TMP36  > AREF
//GND TMP36 > GND
//D6 > Piezo > GND
//D7 > Blue Led (+), Blue Led  (-) > 60 Ohm > GND
//D8 > Green Led (+), Green Led (-) > 560 Ohm > GND
//D9  > Red Led (+), Red Led (-) > 640 Ohm > GND

int iPinTemperature = 0;
int iTemperatureReading;
int iPinLedRed = 9;
int iPinLedGreen = 8;
int iPinLedBlue = 7;
int iPinBuzzer = 6;
String strSerialInput = "";
bool bSerialInputComplete = false;
float fTopTemperature = 100;
float fBottomTemperature = 0;
int iEEPROMTopTemperature = 0;
int iEEPROMBottomTemperature = 4;
int iEEPROMDisplayCelcius = 8;
int iEEPROMPlayTopTemperatureSound = 9;
int iEEPROMPlayBottomTemperatureSound = 10;
bool bPlayTopTemperatureSound = true;
bool bPlayBottomTemperatureSound = true;
bool bTempDisplaysCelcius = true;
float fPreviousTemperature = 0;
//  Number of average temperatures in celcius
#define AVGC 13
#define POSCOUNT 10
float fArrTemps[AVGC];       // Array used to calculate the average temperature
float fArrTempsD[AVGC];      // Array used to calculate the average delta temperature
int iLEDConfigCurrent = 0;   // 1 = Red, 0 = Green, -1 = Blue
int iLEDConfigPrevious = 0;  // 1 = Red, 0 = Green, -1 = Blue
bool bLEDsActive = false;
bool bLEDsBlink = false;
float fAverageTemp;
float fAverageTempD;
int iDeltaPositiveCount;
bool bInitAverage = true;
int iAvgIndex;
long lMilliSecondsSinceBeep = -1;
long iMilliSecondsSinceBeepCount = 50000;
long iMilliSecondsSinceLastSwitchCount = 900000;
long lMilliSecondsSinceLastSwitch = iMilliSecondsSinceLastSwitchCount;

void setup(void) {
  // We'll send debugging information via the Serial  monitor
  Serial.begin(9600);
  Serial.println("Setup");

  //  If you want to set the aref to something other than 5v
  analogReference(EXTERNAL);
  pinMode(iPinLedRed, OUTPUT);
  pinMode(iPinLedGreen, OUTPUT);
  pinMode(iPinLedBlue, OUTPUT);

  strSerialInput.reserve(20);

  long lTop = EEPROMReadlong(iEEPROMTopTemperature);
  if (lTop < 0 || lTop > 10000) {
    lTop = 2500;
  }
  long lBottom = EEPROMReadlong(iEEPROMBottomTemperature);
  if (lBottom < 0 || lBottom > 10000) {
    lBottom = 2100;
  }

  if (lTop < lBottom) {
    lTop = 2500;
    lBottom = 2100;
  }
  fTopTemperature = (float)lTop / (float)100;
  fBottomTemperature = (float)lBottom / (float)100;
  bTempDisplaysCelcius = EEPROM.read(iEEPROMDisplayCelcius) == 1;

  bPlayTopTemperatureSound = EEPROM.read(iEEPROMPlayTopTemperatureSound) == 1;
  bPlayBottomTemperatureSound = EEPROM.read(iEEPROMPlayBottomTemperatureSound) == 1;
}


void loop(void) {
  if (bSerialInputComplete) {
    Serial.println(strSerialInput);
    strSerialInput = "";
    bSerialInputComplete = false;
  }

  iTemperatureReading = analogRead(iPinTemperature);
  // Converting temperature voltage to temperature using AREF
  float fTemperatureVoltage = iTemperatureReading * AREF;
  fTemperatureVoltage /= 1024.0;
  float fTemperatureC = (fTemperatureVoltage - 0.5) * 100;  //converting from 10 mv per degree with 500  mV offset to degrees ((fTemperatureVoltage - 500mV) times 100

  // Initialize  average arrays etc
  if (bInitAverage) {
    for (int j = 0; j < AVGC; j++) {
      fArrTemps[j] = fTemperatureC;
      fArrTempsD[j] = (float)-0.01;
    }
    bInitAverage = false;
    iAvgIndex = 0;
    fAverageTemp = fTemperatureC;
    fPreviousTemperature = fTemperatureC;
    bLEDsActive = true;
    lMilliSecondsSinceBeep = millis();
    iLEDConfigPrevious = -1;
  }

  // Point to next average  in the arrays
  iAvgIndex++;
  if (iAvgIndex > AVGC - 1) {
    iAvgIndex = 0;
  }

  fArrTemps[iAvgIndex] = fTemperatureC;

  // Calculate  averages
  fAverageTemp = 0;
  int iPosCount = 0;
  for (int j = 0; j < AVGC; j++) {
    if (fArrTempsD[j] > (float)0.0) {
      iPosCount++;
    }
    fAverageTemp += fArrTemps[j];
    fAverageTempD += fArrTempsD[j];
  }

  fAverageTemp = fAverageTemp / (float)AVGC;
  fAverageTempD = fAverageTempD / (float)AVGC;

  Serial.print("[it:");
  Serial.print(fTemperatureC);
  Serial.print(",tt:");
  Serial.print(fTopTemperature);
  Serial.print(",bt:");
  Serial.print(fBottomTemperature);
  Serial.print(",dc:");
  Serial.print(bTempDisplaysCelcius);
  Serial.print(",at:");
  Serial.print(fAverageTemp);
  Serial.print(",pt:");
  Serial.print(bPlayTopTemperatureSound);
  Serial.print(",pb:");
  Serial.print(bPlayBottomTemperatureSound);
  Serial.println("]");

  if (fAverageTemp >= fTopTemperature) {
    iLEDConfigCurrent = 1;  // Red
  }
  if (fAverageTemp <= fBottomTemperature) {
    iLEDConfigCurrent = -1;  // Blue
  }
  if (fAverageTemp > fBottomTemperature && fAverageTemp < fTopTemperature) {
    iLEDConfigCurrent = 0;  // Green
  }

  if (iLEDConfigCurrent >= 0 || (iLEDConfigCurrent == -1 && fAverageTempD > 0 && iPosCount >= POSCOUNT)) {
    lMilliSecondsSinceLastSwitch = millis();
  }

  bLEDsActive = ((millis() - lMilliSecondsSinceLastSwitch) < iMilliSecondsSinceLastSwitchCount);

  if (!bLEDsActive || (fAverageTempD > 0 && iPosCount >= POSCOUNT && !bLEDsBlink)) {
    digitalWrite(iPinLedRed, LOW);
    digitalWrite(iPinLedGreen, LOW);
    digitalWrite(iPinLedBlue, LOW);
  } else {
    if (iLEDConfigCurrent == 1) {
      digitalWrite(iPinLedRed, HIGH);
      digitalWrite(iPinLedGreen, LOW);
      digitalWrite(iPinLedBlue, LOW);
    }
    if (iLEDConfigCurrent == 0) {
      digitalWrite(iPinLedRed, LOW);
      digitalWrite(iPinLedGreen, HIGH);
      digitalWrite(iPinLedBlue, LOW);
    }
    if (iLEDConfigCurrent == -1) {
      digitalWrite(iPinLedRed, LOW);
      digitalWrite(iPinLedGreen, LOW);
      digitalWrite(iPinLedBlue, HIGH);
    }
  }

  if (iLEDConfigPrevious == 1 && iLEDConfigCurrent == 0) {
    PlayUp();
  }

  if (iLEDConfigPrevious == 0 && iLEDConfigCurrent == -1) {
    PlayDown();
  }


  fArrTempsD[iAvgIndex] = fAverageTemp - fPreviousTemperature;
  fPreviousTemperature = fAverageTemp;
  iLEDConfigPrevious = iLEDConfigCurrent;
  bLEDsBlink = !bLEDsBlink;
  delay(450);
}

void serialEvent() {
  while (Serial.available()) {
    // get the new  byte:
    char inChar = (char)Serial.read();
    // add it to the strSerialInput:
    strSerialInput += inChar;
    // if the incoming character is a newline,  set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      bSerialInputComplete = true;

      if (strSerialInput.startsWith("tt:")) {
        strSerialInput = strSerialInput.substring(3);
        fTopTemperature = strSerialInput.toFloat();
        EEPROMWritelong(iEEPROMTopTemperature, (long)(fTopTemperature * (float)100));
      }

      if (strSerialInput.startsWith("bt:")) {
        strSerialInput = strSerialInput.substring(3);
        fBottomTemperature = strSerialInput.toFloat();
        EEPROMWritelong(iEEPROMBottomTemperature, (long)(fBottomTemperature * (float)100));
      }

      bTempDisplaysCelcius = ParseSerialOfBoolIfMatchSetEEPROMAndReturnBool("dc:", bTempDisplaysCelcius, iEEPROMDisplayCelcius);
      bPlayTopTemperatureSound = ParseSerialOfBoolIfMatchSetEEPROMAndReturnBool("pt:", bPlayTopTemperatureSound, iEEPROMPlayTopTemperatureSound);
      bPlayBottomTemperatureSound = ParseSerialOfBoolIfMatchSetEEPROMAndReturnBool("pb:", bPlayBottomTemperatureSound, iEEPROMPlayBottomTemperatureSound);
    }
  }
}

bool ParseSerialOfBoolIfMatchSetEEPROMAndReturnBool(String ip_strCommand, bool ip_bOriginalValue, int ip_iEEPROM_Location) {
  bool _bResult = ip_bOriginalValue;
  if (strSerialInput.startsWith(ip_strCommand)) {
    strSerialInput = strSerialInput.substring(3);
    _bResult = (strSerialInput == "1\
");
    EEPROM.write(ip_iEEPROM_Location, _bResult);
  }
  return _bResult;
}

void EEPROMWritelong(int address, long value) {
  byte by;
  for (int i = 0; i < 4; i++) {
    by = (value >> ((3 - i) * 8)) & 0x000000ff;
    EEPROM.write(address + i, by);
  }
}


long EEPROMReadlong(long address) {
  long lo = 0;

  for (int i = 0; i < 3; i++) {
    lo += EEPROM.read(address + i);
    lo = lo << 8;
  }
  lo += EEPROM.read(address + 3);
  return lo;
}

void PlayUp() {
  Serial.print("Perfect temp");
  //if (bPlayTopTemperatureSound && (millis() - lMilliSecondsSinceBeep  > iMilliSecondsSinceBeepCount))
  {
    tone(iPinBuzzer, 440, 100);
    delay(100);
    tone(iPinBuzzer, 880, 100);
    delay(100);
    tone(iPinBuzzer, 1760, 100);
    lMilliSecondsSinceBeep = millis();
    delay(30000);
  }
}

void PlayDown() {
  Serial.print("Too cold");
  //if (bPlayBottomTemperatureSound && (millis() - lMilliSecondsSinceBeep > iMilliSecondsSinceBeepCount))
  {
    tone(iPinBuzzer, 1760, 100);
    delay(100);
    tone(iPinBuzzer, 880, 100);
    delay(100);
    tone(iPinBuzzer, 440, 100);
    lMilliSecondsSinceBeep = millis();
    delay(30000);
  }
}
