#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>
#include "Arduino.h"
#include <SparkFunDS3234RTC.h> 

// Use the external SPI SD card as storage
#include <SPI.h>


//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID     100  // The same on all nodes that talk to each other
#define NODEID        1    // The unique identifier of this node
#define RECEIVER      1    // The recipient of packets
 
//Match frequency to the hardware version of the radio on your Feather
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HCW   true // set to 'true' if you are using an RFM69HCW module
 
#define SERIAL_BAUD   9600
 
#define RFM69_CS      10
#define RFM69_IRQ     A0
#define RFM69_IRQN    A0 // Pin 2 is IRQ 0!
#define RFM69_RST     A2
 
#define LED           9  // onboard blinky
#define DS13074_CS_PIN 8 // DeadOn RTC Chip-select pin

//*********************************************************************************************
//*********************************************************************************************
//Auto Transmission Control - dials down transmit power to save battery
//Usually you do not need to always transmit at max output power
//By reducing TX power even a little you save a significant amount of battery power
//This setting enables this gateway to work with remote nodes that have ATC enabled to
//dial their power down to only the required level
#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************
 
 
//RFM69 radio = RFM69(RFM69_CS, RFM69_IRQ, IS_RFM69HCW, RFM69_IRQN);
#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

const int chipSelect = 7;

void setup() {
  while (!Serial); // wait until serial console is open, remove if not tethered to computer
  Serial.begin(SERIAL_BAUD);
  pinMode(RFM69_CS, OUTPUT);
  pinMode(DS13074_CS_PIN, OUTPUT);
  
  digitalWrite(RFM69_CS, HIGH);
  digitalWrite(DS13074_CS_PIN, HIGH);
  
  initRTC();
  initRadio();
  
}

int16_t packetnum = 0;  // packet counter, we increment per xmission
byte ackCount=0;
uint32_t packetCount = 0;
bool promiscuousMode = false;

void loop() {
  static int8_t lastSecond = -1;
  //sendMessage();
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
    Serial.println();
    Blink(LED,3,3);
    delay(250);
  }
  turnOffAllCSPins();
  switchForSDTransfer();
  rtc.update();
  if (rtc.second() != lastSecond) // If the second has changed
  {
    printTime(); // Print the new time
    Blink(LED,5,5);
    lastSecond = rtc.second(); // Update lastSecond value
  } 
  turnOffAllCSPins();
  delay(5000);
}

void initRadio(){
  turnOffAllCSPins();
  switchForRadioTransfer();
  
  Serial.println("Arduino RFM69HCW Transmitter");
  
      //Hard reset the radio so that we re-initialise the radio
    //  pinMode(RFM69_RST, OUTPUT);
    //  pinMode(12, OUTPUT);
    //  digitalWrite(12, HIGH);
    //  digitalWrite(RFM69_RST, HIGH);
    //  delay(100);
    //  digitalWrite(RFM69_RST, LOW);
    //  delay(100);
  
  //Now initialise the radio
  bool check = radio.initialize(FREQUENCY, NODEID, NETWORKID);
  if (check){
    if (IS_RFM69HCW) {
      radio.setHighPower();    // Only for RFM69HCW & HW!
    }
    //radio.setPowerLevel(31); // power output ranges from 0 (5dBm) to 31 (20dBm)
    radio.encrypt(ENCRYPTKEY);
    radio.setFrequency(433000000);
    #ifdef ENABLE_ATC
      Serial.println("RFM69_ATC Enabled (Auto Transmission Control)");
    #endif
    pinMode(LED, OUTPUT);

    Serial.print("\Transmitting at ");
    Serial.print(radio.getFrequency());
    Serial.println(" Hz");
  }else {
    Serial.println("Cannot initialize radio");
  }
}

void sendMessage(){
  turnOffAllCSPins();
  switchForRadioTransfer();
  
  String command = "Testing SD-Radio";     
  char radiopacket[20]; 
  command.toCharArray(radiopacket, 20);
  itoa(packetnum++, radiopacket+16, 10);
  Serial.print("Sending "); Serial.println(radiopacket);
    
  if (radio.sendWithRetry(RECEIVER, radiopacket, strlen(radiopacket))) { //target node Id, message as string or byte array, message length
    Serial.println("OK");
    Blink(LED, 50, 3); //blink LED 3 times, 50ms between blinks
  }
  radio.receiveDone(); //put radio in RX mode
  Serial.flush(); //make sure all serial data is clocked out before sleeping the MCU
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i=0; i<loops; i++)
  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}

void initRTC(){
  turnOffAllCSPins();
  switchForSDTransfer();
    Serial.println();

     Serial.println("rtc initializing.");
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
  Serial.println("rtc initialized.");
}


//Before Data transfer or switching mode, turn off all devices
void turnOffAllCSPins(){
  digitalWrite(DS13074_CS_PIN, HIGH);
  digitalWrite(RFM69_CS, HIGH);
  SPI.endTransaction();
}

//Turn on single device for data transfer
void turnOnCs(int csPin){
  digitalWrite(csPin, LOW);
}

void switchForSDTransfer(){
  //SPISettings settings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
  //SPI.beginTransaction(settings);
  digitalWrite(DS13074_CS_PIN, LOW);
}

void switchForRadioTransfer(){
  SPISettings settings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(RFM69_CS, LOW);
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



