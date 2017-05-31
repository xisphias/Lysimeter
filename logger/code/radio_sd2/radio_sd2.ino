#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>
#include "Arduino.h"

// Use the external SPI SD card as storage
#include <SPI.h>
#include <SD.h>

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
//#define RFM69_IRQ     A0
//#define RFM69_IRQN    A0 // Pin 2 is IRQ 0!
//#define RFM69_RST     A2
 
#define LED           9  // onboard blinky
#define SD_PIN        7  // SD Card CS pin


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
  pinMode(SD_PIN, OUTPUT);
  
  digitalWrite(RFM69_CS, HIGH);
  digitalWrite(SD_PIN, HIGH);
  
  initSDCard();
  initRadio();
  
}

int16_t packetnum = 0;  // packet counter, we increment per xmission
byte ackCount=0;
uint32_t packetCount = 0;
bool promiscuousMode = false;

void loop() {
  sendMessage();
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
  
  // make a string for assembling the data to log:
  String dataString = "";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
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

void initSDCard(){
  turnOffAllCSPins();
  switchForSDTransfer();
    Serial.println();

    Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
}


//Before Data transfer or switching mode, turn off all devices
void turnOffAllCSPins(){
  digitalWrite(SD_PIN, HIGH);
  digitalWrite(RFM69_CS, HIGH);
  SPI.endTransaction();
}

//Turn on single device for data transfer
void turnOnCs(int csPin){
  digitalWrite(csPin, LOW);
}

void switchForSDTransfer(){
  SPISettings settings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(SD_PIN, LOW);
}

void switchForRadioTransfer(){
  SPISettings settings = SPISettings(SPI_CLOCK_DIV4, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(RFM69_CS, LOW);
}
/// Select the transceiver
void select() {
  noInterrupts();     // YOU NEED TO DO THIS!  This keeps the RFM69 from interrupting your 'transaction'
  //save current SPI settings
  _SPCR = SPCR;     // THIS saves the current settings so you can restore them when you're done
  _SPSR = SPSR;
  //set MY DEVICE SPI settings
  SPI.setDataMode(SPI_MODE0);   // SPI Transfer mode
  SPI.setBitOrder(LSBFIRST);         
  SPI.setClockDivider(SPI_CLOCK_DIV16);   // SELECT 1MHz transfer (on a 16MHz processor)
  digitalWrite(RFM69_CS, LOW);
}


//==========================================================================
//
//==========================================================================
/// UNselect the transceiver chip
void unselect() {
  digitalWrite(RFM69_CS, HIGH);
  //restore SPI settings to what they were before talking to MY DEVICE
  SPCR = _SPCR;
  SPSR = _SPSR;
  interrupts();    // re-enable interrupts
}



