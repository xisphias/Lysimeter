// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption, and Automatic Transmission Control
// Passes through any wireless received messages to the serial port & responds to ACKs
// It also looks for an onboard FLASH chip, if present
// **********************************************************************************
// Copyright Felix Rusu 2016, http://www.LowPowerLab.com/contact
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <RFM69.h>         //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>     //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPIFlash.h>      //get it here: https://www.github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)
#include <SdFat.h>
#include <SparkFunDS3234RTC.h> 

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        1    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW_HCW  //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
//*********************************************************************************************
//Auto Transmission Control - dials down transmit power to save battery
//Usually you do not need to always transmit at max output power
//By reducing TX power even a little you save a significant amount of battery power
//This setting enables this gateway to work with remote nodes that have ATC enabled to
//dial their power down to only the required level
#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************
//#define SERIAL_BAUD   115200
#define SERIAL_BAUD   9600

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  //#define FLASH_SS      8 // and FLASH SS on D8
#endif

#define DS13074_CS_PIN 8 // DeadOn RTC Chip-select pin
//#define INTERRUPT_PIN 5 // DeadOn RTC SQW/interrupt pin (optional)

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

SdFat sd;

SdFile file;

//SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
// Default SD chip select is SS pin
const uint8_t chipSelect = 7;

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

void setup() {
    pinMode(chipSelect,OUTPUT);
    pinMode(DS13074_CS_PIN,OUTPUT);
    pinMode(10,OUTPUT);
    digitalWrite(chipSelect, HIGH);
    digitalWrite(DS13074_CS_PIN, HIGH);
    digitalWrite(10, HIGH);
    
    Serial.begin(SERIAL_BAUD);
    delay(10);
    
  // Call rtc.begin([cs]) to initialize the library
  // The chip-select pin should be sent as the only parameter
  rtc.begin(DS13074_CS_PIN);
  // rtc.autoTime();
  // Update time/date values, so we can set alarms
  rtc.update();
  digitalWrite(chipSelect, HIGH);
  
  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
    error("sd begin failed");
    Serial.print("sd begin failed");
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
  digitalWrite(chipSelect, HIGH);
  delay(100); 

    radio.initialize(FREQUENCY,NODEID,NETWORKID);
    #ifdef IS_RFM69HW_HCW
      radio.setHighPower(); //must include this only for RFM69HW/HCW!
    #endif
      radio.encrypt(ENCRYPTKEY);
      //radio.promiscuous(promiscuousMode);
      //radio.setFrequency(919000000); //set frequency to some custom frequency
      char buff[50];
      sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
      Serial.println(buff);
     
    #ifdef ENABLE_ATC
      Serial.println("RFM69_ATC Enabled (Auto Transmission Control)");
    #endif
}

byte ackCount=0;
uint32_t packetCount = 0;
void loop() {
  //process any serial input
  static int8_t lastSecond = -1;
  // Call rtc.update() to update all rtc.seconds(), rtc.minutes(),
  // etc. return functions.
  rtc.update();
  
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input == 'r') //d=dump all register values
      radio.readAllRegs();
    if (input == 'E') //E=enable encryption
      radio.encrypt(ENCRYPTKEY);
    if (input == 'e') //e=disable encryption
      radio.encrypt(null);
    if (input == 'p')
    {
      promiscuousMode = !promiscuousMode;
      radio.promiscuous(promiscuousMode);
      Serial.print("Promiscuous mode ");Serial.println(promiscuousMode ? "on" : "off");
    }
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      Serial.print( "Radio Temp is ");
      Serial.print(temperature);
      Serial.print("C, ");
      Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      Serial.println('F');
    }
  }

  if (radio.receiveDone())
  {
    Serial.print("#[");
    Serial.print(++packetCount);
    Serial.print(']');
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    if (promiscuousMode)
    {
      Serial.print("to [");Serial.print(radio.TARGETID, DEC);Serial.print("] ");
    }
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i]);
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");
    
    if (radio.ACKRequested())
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      Serial.print(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        Serial.print(" Pinging node ");
        Serial.print(theNodeID);
        Serial.print(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
          Serial.print("ok!");
        else Serial.print("nothing");
      }
    }
    if (rtc.second() != lastSecond) // If the second has changed
    {
        Serial.println();
        printTime(); // Print the new time
        Blink(LED,5);
        //Serial.print(String(rtc.getYear()));
        lastSecond = rtc.second(); // Update lastSecond value
    } 
    Serial.println();
    Blink(LED,3);
    delay(250);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
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
