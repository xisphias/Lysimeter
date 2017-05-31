/*
 * This program tests the dateTimeCallback() function
 * and the timestamp() function.
 */
 /*
 * SparkFun MicroSD Breakout Board
 * MicroSD Breakout    mbed
 *  CS  o-------------o 7    (DigitalOut cs)
 *  DI  o-------------o 11    (SPI mosi)
 *  VCC o-------------o 3.3
 *  SCK o-------------o 13    (SPI sclk)
 *  GND o-------------o GND  
 *  DO  o-------------o 12    (SPI miso)
 *  CD  o-------------o 6
*/
/*
 * SparkFun RTC DS3234
  *  GND o-------------o GND
  *  VCC o-------------o 3.3
  *  SQW o-------------o X   
  *  CLK o-------------o 13   
  * MISO o-------------o 12   
  * MOSI o-------------o 11   
  *  SS  o-------------o 8    
*/
#include <SPI.h>
#include "SdFat.h"
#include <SparkFunDS3234RTC.h> 

SdFat sd;

SdFile file;

// Default SD chip select is SS pin
const uint8_t chipSelect = 7;

#define LED 9 //LED pin

#define DS13074_CS_PIN 8 // DeadOn RTC Chip-select pin
//#define INTERRUPT_PIN 5 // DeadOn RTC SQW/interrupt pin (optional)

// create Serial stream
ArduinoOutStream cout(Serial);
//------------------------------------------------------------------------------
// store error strings in flash to save RAM
#define error(s) sd.errorHalt(F(s))
//------------------------------------------------------------------------------
/*
 * date/time values for debug
 * normally supplied by a real-time clock or GPS
 */
// date 1-Oct-14
uint16_t year = 2014;
uint8_t month = 10;
uint8_t day = 1;

// time 20:30:40
uint8_t hour = 20;
uint8_t minute = 30;
uint8_t second = 40;
//------------------------------------------------------------------------------
/*
 * User provided date time callback function.
 * See SdFile::dateTimeCallback() for usage.
 */
void dateTime(uint16_t* date, uint16_t* time) {
  // User gets date and time from GPS or real-time
  // clock in real callback function
  rtc.update();
  year = 2000+rtc.getYear();
  month = rtc.getMonth();
  day = rtc.getDate();
  hour = rtc.getHour();
  minute = rtc.getMinute();
  second = rtc.getSecond();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(year, month, day);

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(hour, minute, second);
}
//------------------------------------------------------------------------------
/*
 * Function to print all timestamps.
 */
void printTimestamps(SdFile& f) {
  dir_t d;
  if (!f.dirEntry(&d)) {
    error("f.dirEntry failed");
  }

  cout << F("Creation: ");
  f.printFatDate(d.creationDate);
  cout << ' ';
  f.printFatTime(d.creationTime);
  cout << endl;

  cout << F("Modify: ");
  f.printFatDate(d.lastWriteDate);
  cout <<' ';
  f.printFatTime(d.lastWriteTime);
  cout << endl;

  cout << F("Access: ");
  f.printFatDate(d.lastAccessDate);
  cout << endl;
}
/*==============|| Functions ||==============*/
void Blink(byte, int);
//------------------------------------------------------------------------------
void setup(void) {
  Serial.begin(9600);
  // Wait for USB Serial
  while (!Serial) {
    SysCall::yield();
  }
  cout << F("Type any character to start\n");
  while (!Serial.available()) {
    SysCall::yield();  
  }
  
  pinMode(LED, OUTPUT); //Set LED to output
  
  // Call rtc.begin([cs]) to initialize the library
  // The chip-select pin should be sent as the only parameter
  rtc.begin(DS13074_CS_PIN);
  // rtc.autoTime();
  // Update time/date values, so we can set alarms
  rtc.update();
  
  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  // remove files if they exist
  sd.remove("callback.txt");
  sd.remove("default.txt");
  sd.remove("stamp.txt");

  // create a new file with default timestamps
  if (!file.open("default.txt", O_CREAT | O_WRITE)) {
    error("open default.txt failed");
  }
  cout << F("\nOpen with default times\n");
  printTimestamps(file);

  // close file
  file.close();
  /*
   * Test the date time callback function.
   *
   * dateTimeCallback() sets the function
   * that is called when a file is created
   * or when a file's directory entry is
   * modified by sync().
   *
   * The callback can be disabled by the call
   * SdFile::dateTimeCallbackCancel()
   */
  // set date time callback function
  SdFile::dateTimeCallback(dateTime);

  // create a new file with callback timestamps
  if (!file.open("callback.txt", O_CREAT | O_WRITE)) {
    error("open callback.txt failed");
  }
  cout << ("\nOpen with callback times\n");
  printTimestamps(file);

  // change call back date
  day += 1;

  // must add two to see change since FAT second field is 5-bits
  second += 2;

  // modify file by writing a byte
  file.write('t');

  // force dir update
  file.sync();

  cout << F("\nTimes after write\n");
  printTimestamps(file);

  // close file
  file.close();
  /*
   * Test timestamp() function
   *
   * Cancel callback so sync will not
   * change access/modify timestamp
   */
  SdFile::dateTimeCallbackCancel();

  // create a new file with default timestamps
  if (!file.open("stamp.txt", O_CREAT | O_WRITE)) {
    error("open stamp.txt failed");
  }
  // set creation date time
  if (!file.timestamp(T_CREATE, 2014, 11, 10, 1, 2, 3)) {
    error("set create time failed");
  }
  // set write/modification date time
  if (!file.timestamp(T_WRITE, 2014, 11, 11, 4, 5, 6)) {
    error("set write time failed");
  }
  // set access date
  if (!file.timestamp(T_ACCESS, 2014, 11, 12, 7, 8, 9)) {
    error("set access time failed");
  }
  cout << F("\nTimes after timestamp() calls\n");
  printTimestamps(file);

  file.close();
  cout << F("\nDone\n");
}

void loop() {
  static int8_t lastSecond = -1;
  
  // Call rtc.update() to update all rtc.seconds(), rtc.minutes(),
  // etc. return functions.
  rtc.update();

  if (rtc.second() != lastSecond) // If the second has changed
  {
    printTime(); // Print the new time
    Blink(LED,5);
    //Serial.print(String(rtc.getYear()));
    lastSecond = rtc.second(); // Update lastSecond value
  } 

}

/**
 * Blinks an LED
 * @param PIN      Pin to Blink
 * @param DELAY_MS Time to Delay
 */
void Blink(byte PIN, int DELAY_MS) {
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
  delay(DELAY_MS);
}
void printTime()
{
  Serial.print(String(rtc.hour()) + ":"); // Print hour
  if (rtc.minute() < 10)
    Serial.print('0'); // Print leading '0' for minute
  Serial.print(String(rtc.minute()) + ":"); // Print minute
  if (rtc.second() < 10)
    Serial.print('0'); // Print leading '0' for second
  Serial.print(String(rtc.second())); // Print second

  if (rtc.is12Hour()) // If we're in 12-hour mode
  {
    // Use rtc.pm() to read the AM/PM state of the hour
    if (rtc.pm()) Serial.print(" PM"); // Returns true if PM
    else Serial.print(" AM");
  }
  
  Serial.print(" | ");

  // Few options for printing the day, pick one:
  Serial.print(rtc.dayStr()); // Print day string
  //Serial.print(rtc.dayC()); // Print day character
  //Serial.print(rtc.day()); // Print day integer (1-7, Sun-Sat)
  Serial.print(" - ");
#ifdef PRINT_USA_DATE
  Serial.print(String(rtc.month()) + "/" +   // Print month
                 String(rtc.date()) + "/");  // Print date
#else
  Serial.print(String(rtc.date()) + "/" +    // (or) print date
                 String(rtc.month()) + "/"); // Print month
#endif
  Serial.println(String(rtc.year()));        // Print year
}
