#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69

#include "Arduino.h"

// Use the external SPI SD card as storage
#include <SPI.h>
#include <SD.h>

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID     100  // The same on all nodes that talk to each other
#define NODEID        2    // The unique identifier of this node
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
#define SD_PIN        7  // SD Card CS pin
#define TABLE_SIZE    8192
//*********************************************************************************************

 
int16_t packetnum = 0;  // packet counter, we increment per xmission
 
//RFM69 radio = RFM69(RFM69_CS, RFM69_IRQ, IS_RFM69HCW, RFM69_IRQN);
RFM69 radio;

#define RECORDS_TO_CREATE 10

char* db_name = "/db/edb_test.db";
File dbFile;

// Arbitrary record definition for this table.
// This should be modified to reflect your record needs.
struct LogEvent {
    int id;
    int temperature;
}
logEvent;

// The read and write handlers for using the SD Library
// Also blinks the led while writing/reading
void writer (unsigned long address, byte data) {
    digitalWrite(LED, HIGH);
    dbFile.seek(address);
    dbFile.write(data);
    dbFile.flush();
    digitalWrite(LED, LOW);
}

byte reader (unsigned long address) {
    digitalWrite(LED, HIGH);
    dbFile.seek(address);
    byte b = dbFile.read();
    digitalWrite(LED, LOW);
    return b;
}

// Create an EDB object with the appropriate write and read handlers
//EDB db(&writer, &reader);

void setup() {
  while (!Serial); // wait until serial console is open, remove if not tethered to computer
  Serial.begin(SERIAL_BAUD);
  pinMode(RFM69_CS, OUTPUT);
  pinMode(SD_PIN, OUTPUT);
  
  digitalWrite(RFM69_CS, HIGH);
  digitalWrite(SD_PIN, HIGH);
  
  initSDCard();
  initRadio();
  countRecords();
  selectAll();
  
}

void loop() {
  sendMessage();
  createRecord();
  countRecords();
  selectAll();
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
  Serial.println(" Extended Database Library + External SD CARD storage demo");
    Serial.println();

    randomSeed(analogRead(0));

    if (!SD.begin(SD_PIN)) {
        Serial.println("No SD-card.");
        return;
    }

    // Check dir for db files
    if (!SD.exists("/db")) {
        Serial.println("Dir for Db files does not exist, creating...");
        SD.mkdir("/db");
    }

    if (SD.exists(db_name)) {

        dbFile = SD.open(db_name, FILE_WRITE);

        // Sometimes it wont open at first attempt, espessialy after cold start
        // Let's try one more time
        if (!dbFile) {
            dbFile = SD.open(db_name, FILE_WRITE);
        }

        if (dbFile) {
            Serial.print("Openning current table... ");
//            EDB_Status result = db.open(0);
//            if (result == EDB_OK) {
                Serial.println("DONE");
         //   } else {
                Serial.println("ERROR");
                Serial.println("Did not find database in the file " + String(db_name));
                Serial.print("Creating new table... ");
             //   db.create(0, TABLE_SIZE, (unsigned int)sizeof(logEvent));
                Serial.println("DONE");
             //   return;
          //  }
       // } else {
            Serial.println("Could not open file " + String(db_name));
         //   return;
        }
    } else {
        Serial.print("Creating table... ");
        // create table at with starting address 0
     //   dbFile = SD.open(db_name, FILE_WRITE);
//        db.create(0, TABLE_SIZE, (unsigned int)sizeof(logEvent));
        Serial.println("DONE");
    }
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


void createRecord()
{
  turnOffAllCSPins();
  switchForSDTransfer();
 
    Serial.print("Creating Record... ");
        logEvent.id = 2;
        logEvent.temperature = random(1, 125);
//        EDB_Status result = db.appendRec(EDB_REC logEvent);
//        if (result != EDB_OK) printError(result);
    Serial.println("Record created");
}

void countRecords()
{
  turnOffAllCSPins();
  switchForSDTransfer();
    Serial.print("Record Count: ");
//    Serial.println(db.count());
}

void selectAll()
{
  turnOffAllCSPins();
  switchForSDTransfer();
//    for (int recno = 1; recno <= db.count(); recno++)
   // {
        //EDB_Status result = db.readRec(recno, EDB_REC logEvent);
//        if (result == EDB_OK)
       // {
            Serial.print("Recno: ");
//            Serial.print(recno);
            Serial.print(" ID: ");
       //     Serial.print(logEvent.id);
            Serial.print(" Temp: ");
//Serial.println(logEvent.temperature);
       // }
       // else printError(result);
   // }
}

