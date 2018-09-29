/* 
 * Bedside clock
 * 
 * A cool alarm clock using a Nokia 3210 display that you can have on your beside table
 * 
 */ 

#include <Arduino.h>
#include <TimeLib.h>
#include <FrequencyTimer2.h>
#include <SPI.h>
#include <LCD_Functions.h>
#include <NewTone.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define TIME_HEADER  "T"   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

const int BTN_PLUS_PIN = 2;    // the number of the + button pin
const int BTN_MINUS_PIN = 10;    // the number of the - button pin
const int BTN_SET_PIN = 4;    // the number of the SET button pin
const int PIEZO_PIN = 8;

const unsigned long DEFAULT_TIME = 1514808000; // Jan 1 2018
const unsigned long DEBOUNCE_DELAY[3] = {100, 100, 50}; // the debounce time; increase if the output flickers
// const int SLEEP_LOOPS = 1000 / DEBOUNCE_DELAY;

int buttonState[3];             // the current reading from the input pin
int lastButtonState[3] = {LOW, LOW, LOW};   // the previous reading from the input pin
unsigned long lastDebounceTime[3] = {0,0,0};  // the last time the output pin was toggled
bool setButtonActionTriggered = false;

int setButtonState = 0;      // 0 - normal mode, 1 - set hours, 2 - set minutes
int timeAdjustment = 3600;

OneWire  ds(12);  // Connect your 1-wire device to pin 12
DallasTemperature sensors(&ds);
DeviceAddress insideThermometer = { 0x28, 0xE9, 0xCB, 0xF1, 0x06, 0x00, 0x00, 0xDC };

void setup() {
  //NewTone(PIEZO_PIN, 1000, 500);
  for (unsigned long freq = 125; freq <= 15000; freq += 10) {  
    NewTone(PIEZO_PIN, freq); // Play the frequency (125 Hz to 15 kHz sweep in 10 Hz steps).
    delay(1); // Wait 1 ms so you can hear it.
  }
  noNewTone(PIEZO_PIN); // Turn off the tone.


  serialSetup();
    discoverOneWireDevices();

  sensors.begin();
  sensors.setResolution(insideThermometer, 10);
  lcdBegin(); // This will setup our pins, and initialize the LCD
  updateDisplay(); // with displayMap untouched, SFE logo
  setContrast(40); // Good values range from 40-60
  delay(2000);
  clearDisplay(WHITE);
  updateDisplay();
  timeSetup();
  buttonSetup();
  FrequencyTimer2::setPeriod(1000);
  FrequencyTimer2::setOnOverflow(timer2ISR);
}

void serialSetup() {
  Serial.begin(9600);
  while (!Serial) ; // Needed for Leonardo only
}

void timer2ISR()
{ 
    buttonLoop();
}

void timeSetup() {
  pinMode(13, OUTPUT);
  setSyncProvider(requestSync);  //set function to call when sync required
  setTime(DEFAULT_TIME);  
}

void buttonSetup() {
    pinMode(BTN_PLUS_PIN, INPUT);
}

void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == -127.00) {
    Serial.print("Error getting temperature");
  } else {
    Serial.print("C: ");
    Serial.print(tempC);
    Serial.print(" F: ");
    Serial.print(DallasTemperature::toFahrenheit(tempC));
  }
}

void loop()
{
  sensors.requestTemperatures();
  timeLoop();
    Serial.print("Getting temperatures...\n\r");
  sensors.requestTemperatures();
  
  Serial.print("Inside temperature is: ");
  printTemperature(insideThermometer);
  Serial.print("\n\r\n\r");
  delay(1000);
/*  int i;
  for(i=0; i<SLEEP_LOOPS; i++) {
    buttonLoop();
    delay(DEBOUNCE_DELAY);
  }*/
}

void timeLoop() {
  if (Serial.available()) {
    processSyncMessage();
  }
  if (timeStatus()!= timeNotSet) {
    digitalClockDisplay();  
  }
//  if (timeStatus() == timeSet) {
//    digitalWrite(13, HIGH); // LED on if synced
//  } else {
//    digitalWrite(13, LOW);  // LED off if needs refresh
//  }  
}

int getButtonPosition(int buttonPin) {
  return buttonPin - 2;  
}

void nextSetMode() {
  setButtonState++;
  switch( setButtonState ) {
    case 1:
      timeAdjustment = 3600;
      break;
    case 2:
      timeAdjustment = 60;
      break;
    default:
      setButtonState = 0;
  }
}

void doButtonFunction(const int buttonPin) {
  switch( buttonPin )
  {
    case BTN_PLUS_PIN:
      adjustTime(timeAdjustment);
      break;
    case BTN_MINUS_PIN:
      adjustTime(-timeAdjustment);
      break;
    case BTN_SET_PIN:
      nextSetMode();
      break;
    default:
      break;
  }
}

void handleButton(const int buttonPin) {
  // Only allow adjustments if in "SET" mode
  if (setButtonState == 0 || buttonPin == BTN_SET_PIN) {
    return;
  }

  int buttonArrayPosition = getButtonPosition(buttonPin);
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState[buttonArrayPosition]) {
    // reset the debouncing timer
    lastDebounceTime[buttonArrayPosition] = millis();
  }
  
  if ((millis() - lastDebounceTime[buttonArrayPosition]) > DEBOUNCE_DELAY[buttonArrayPosition]) {
    if (reading != buttonState[buttonArrayPosition]) {
      buttonState[buttonArrayPosition] = reading;
    }
    if (buttonState[buttonArrayPosition] == HIGH) {
      doButtonFunction(buttonPin);
      //digitalClockDisplay();
    }
  }
  lastButtonState[buttonArrayPosition] = reading;
}

void handleSetButton(const int buttonPin) {
  if (buttonPin != BTN_SET_PIN) {
    return;
  }

  int buttonArrayPosition = getButtonPosition(buttonPin);
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState[buttonArrayPosition]) {
    // reset the debouncing timer
    lastDebounceTime[buttonArrayPosition] = millis();
  }
  
  if ((millis() - lastDebounceTime[buttonArrayPosition]) > DEBOUNCE_DELAY[buttonArrayPosition]) {
    if (reading != buttonState[buttonArrayPosition]) {
      buttonState[buttonArrayPosition] = reading;
    }
    if (buttonState[buttonArrayPosition] == HIGH && setButtonActionTriggered == false ) {
      doButtonFunction(buttonPin);
      digitalClockDisplay();
      setButtonActionTriggered = true;
    } else if (buttonState[buttonArrayPosition] == LOW) {
      setButtonActionTriggered = false;
    }
  }
  lastButtonState[buttonArrayPosition] = reading;
}

void buttonLoop() {
  handleSetButton(BTN_SET_PIN);
  handleButton(BTN_PLUS_PIN);
  handleButton(BTN_MINUS_PIN);  
}

void clearTerminal() {
  Serial.write(27);       // ESC command
  // Serial.print("[2J");    // clear screen command
  Serial.print("[1A");    // clear screen command
  Serial.write(27);
  Serial.print("[H");     // cursor to home command
}

void printBold(int value) {
  Serial.write(27);
  Serial.print("[1m");
  Serial.print(value);
  Serial.write(27);
  Serial.print("[m");
}

String getWeekday() {
  switch( weekday() ) {
    case 1:
      return "Sunday";
    case 2:
      return "Monday";
    case 3:
      return "Tuesday";
    case 4:
      return "Wednesday";
    case 5:
      return "Thursday";
    case 6:
      return "Friday";
    case 7:
      return "Saturday";
    default:
      return "What day?";
  }
}

String centerString(String text) {
  if(text.length()>=14)
    return text;
  if(text.length()==13)
    return "_" + text;

  String retVal = "";
  int whitespace = 14-text.length();
  int spaces = whitespace / 2;
  int halfSpace = whitespace % 2;

  while(spaces) {
    retVal += " ";
    spaces--;
  }
  
  if(halfSpace)
    retVal += "_";

  retVal += text;

  if(halfSpace)
    retVal += "_";
    
  return retVal;
}

String getDisplayText() {
  static bool blinkState = false;
  String retVal = "|";
  retVal += day();
  retVal += ".";
  retVal += month();
  retVal += ".";
  retVal += year();
  retVal += "  ";

  blinkState = !blinkState;

  retVal += "\r\n\r\n    _";
  if(setButtonState == 1 && blinkState) {
    retVal += "  ";
  } else {
    retVal += getDigits(hour(), false);
  }
  if(blinkState || setButtonState!=0)
    retVal += ":";
  else
    retVal += " ";
  if(setButtonState == 2 && blinkState)
    retVal += "  ";
  else
    retVal += getDigits(minute(), false);

  retVal += "\r\n";
  retVal += centerString(getWeekday());

  retVal += "\r\n";
  retVal += centerString(getTemperature());
  return retVal;
}
void digitalClockDisplay(){
  // digital clock display of the time
  // clearTerminal();
  writeToLCD(getDisplayText());
//  if (setButtonState == 1) {
//    printBold(hour());
//  } else {
//    Serial.print(hour());
//  }
//  if (setButtonState == 2) {
//    Serial.print(":");
//    printBold(minute());
//  } else {
//    printDigits(minute());
//  }
//  printDigits(second());
//  Serial.print(" ");
//  Serial.print(day());
//  Serial.print(" ");
//  Serial.print(month());
//  Serial.print(" ");
//  Serial.print(year());
//  Serial.print(" State: ");
//  Serial.print(setButtonState);
//  Serial.print(" timeadj: ");
//  Serial.print(timeAdjustment);
//  if( setButtonActionTriggered )
//  {
//    Serial.print("Triggered action active");
//  }
//  Serial.println();
}

String getDigits(int digits, bool printColon){
  // utility function for digital clock display: prints preceding colon and leading 0
  String retval = "";
  if(printColon)
    retval += ":";
  if(digits < 10)
    retval += "0";
  retval += digits;
  return retval;
}


void processSyncMessage() {
  unsigned long pctime;

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     if( pctime >= DEFAULT_TIME) { // check the integer is a valid time (greater than Jan 1 2013)
       setTime(pctime); // Sync Arduino clock to the time received on the serial port
     }
  }
}

void writeToLCD(String text) {
  static int cursorX = 0;
  static int cursorY = 0;
  
  if (text)
  {
    for(int i=0; i<text.length(); i++) {
      char c = text.charAt(i);
    
      switch (c)
      {
      case '\n': // New line
        cursorY += 8;
        break;
      case '\r': // Return feed
        cursorX = 0;
        break;
      case '~': // Use ~ to clear the screen.
        clearDisplay(WHITE);
        updateDisplay();
        cursorX = 0; // reset the cursor
        cursorY = 0;
        break;
      case '|':
        cursorX = 0; // reset the cursor
        cursorY = 0;
        break;
      case '_': // Half-space, used to center horizontally
        setChar(' ', cursorX, cursorY, BLACK);
        updateDisplay();
        cursorX += 3;
        break;
      default:
        setChar(c, cursorX, cursorY, BLACK);
        updateDisplay();
        cursorX += 6; // Increment cursor
        break;
      }
      // Manage cursor
      if (cursorX >= (LCD_WIDTH - 4)) 
      { // If the next char will be off screen...
        cursorX = 0; // ... reset x to 0...
        cursorY += 8; // ...and increment to next line.
        if (cursorY >= (LCD_HEIGHT - 7))
        { // If the next line takes us off screen...
          cursorY = 0; // ...go back to the top.
        }
      }
    }
  }
}

void discoverOneWireDevices(void) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
  Serial.print("Looking for 1-Wire devices...\n\r");

  while(ds.search(addr)) {
    Serial.print("\n\rFound \'1-Wire\' device with address:\n\r");

    for( i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');

      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(", ");

      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print("CRC is not valid!\n");
        return;
    }

    
  }
  Serial.print("\n\r\n\rThat's it.\r\n");
  ds.reset_search();
  return;
}

time_t requestSync()
{
  return 0; // the time will be sent later in response to serial mesg
}

String getTemperature()
{
  float tempC = sensors.getTempC(insideThermometer);
  if (tempC == -127.00) {
    return "Error";
  } else {
    String retval;
    retval += tempC;
    retval += " C";
    return retval;
  }
}
