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
#include <SPI.h> //built in
#include <SparkFunDS3234RTC.h> 


#define SERIAL_BAUD 9600 //connection speed
#define LED 9 //LED pin

#define DS13074_CS_PIN 8 // DeadOn RTC Chip-select pin
//#define INTERRUPT_PIN 5 // DeadOn RTC SQW/interrupt pin (optional)

#define SERIAL_EN //Comment this out to remove Serial comms and save a few kb's of space
#ifdef SERIAL_EN
#define DEBUG(input)   {Serial.print(input); delay(1);}
#define DEBUGln(input) {Serial.println(input); delay(1);}
#define DEBUGFlush() { Serial.flush(); }
#else
#define DEBUG(input);
#define DEBUGln(input);
#define DEBUGFlush();
#endif

/*==============|| Functions ||==============*/
void Blink(byte, int);

/*==============|| DS3234_RTC ||==============*/
//DS3234 rtc; //Declare the Real Time Clock


void setup() 
{
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
  #endif
  #ifdef INTERRUPT_PIN // If using the SQW pin as an interrupt
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  #endif
  
  DEBUGln("-- Datalogger for Lysimeter System --");
  pinMode(LED, OUTPUT); //Set LED to output
  // Call rtc.begin([cs]) to initialize the library
  // The chip-select pin should be sent as the only parameter
  rtc.begin(DS13074_CS_PIN);
  //rtc.set12Hour(); // Use rtc.set12Hour to set to 12-hour mode
  // Now set the time...
  // You can use the autoTime() function to set the RTC's clock and
  // date to the compiliers predefined time. (It'll be a few seconds
  // behind, but close!)
  rtc.autoTime();
  // Or you can use the rtc.setTime(s, m, h, day, date, month, year)
  // function to explicitly set the time:
  // e.g. 7:32:16 | Monday October 31, 2016:
  //rtc.setTime(16, 32, 7, 2, 31, 10, 16);  // Uncomment to manually set time

  // Update time/date values, so we can set alarms
  rtc.update();
  // Configure Alarm(s):
  // (Optional: enable SQW pin as an intterupt)
  //rtc.enableAlarmInterrupt();
  // Set alarm1 to alert when seconds hits 30
  //rtc.setAlarm1(30);
  // Set alarm2 to alert when minute increments by 1
  //rtc.setAlarm2(rtc.minute() + 1);
  
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
