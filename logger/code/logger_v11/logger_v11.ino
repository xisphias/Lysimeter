/**
 * TODO:
 * Check to make sure it compilies
 * Correct the pin mappings
 * Test.
 *
 *
 */


#include <Arduino.h> //built in
#include <SPI.h> //built in
#include <RFM69_ATC.h>//https://www.github.com/lowpowerlab/rfm69
#include <SparkFunDS3234RTC.h> //https://github.com/kinasmith/SparkFun_DS3234_RTC_Arduino_Library
#include "SdFat.h" //https://github.com/greiman/SdFat

#define NETWORKID     100
#define NODEID 0 //Address on Network
#define FREQUENCY RF69_433MHZ //hardware frequency of Radio
#define ATC_RSSI -70 //ideal signal strength
#define ACK_WAIT_TIME 100 // # of ms to wait for an ack
#define ACK_RETRIES 10 // # of attempts before giving up
#define SERIAL_BAUD 115200 //connection speed
#define RFM69_CS 10 //RFM69 CS pin
#define LED 9 //LED pin
#define SD_CS_PIN 7 //Chip Select pin for SD card
#define RTC_CS_PIN 8
#define CARD_DETECT 6 //Pin to detect presence of SD card
//#define INTERRUPT_PIN 2 // DeadOn RTC SQW/interrupt pin (optional)

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

/*==============|| RFM69 ||==============*/
RFM69_ATC radio; //Declare Radio
byte lastRequesterNodeID = NODEID; //Last Sensor to communicate with this device
int8_t NodeID_latch = -1; //Listen only to this Sensor #

/*==============|| DS3231_RTC ||==============*/
uint32_t now; //Holder for Current Time

/*==============|| SD ||==============*/
SdFat SD; //Declare the sd Card
uint8_t CARD_PRESENT; //var for Card Detect sensor reading

/*==============|| Data ||==============*/

struct TimeStamp { //Place to Store the time
	uint32_t timestamp;
};
TimeStamp theTimeStamp;

//Holder for recieving data from sensors.
//NOTE: This must be THE SAME as the payload on the sensors.
struct Payload {
  uint32_t time = 0;
  uint16_t count = 0;
  int battery_voltage = 0;
  int board_temp = 0;
  long w[8] = {0,0,0,0,0,0,0,0};
  int t[8] = {0,0,0,0,0,0,0,0};
};
Payload thePayload;


void setup() {
	#ifdef SERIAL_EN
		Serial.begin(SERIAL_BAUD);
	#endif
  DEBUGln("+++++++++++++++++++++++++++++++++++++");
	DEBUGln("-- Datalogger for Lysimeter System --");
	pinMode(LED, OUTPUT); //Set LED to output
	pinMode(CARD_DETECT, INPUT_PULLUP); //Init. 10k internal Pullup resistor
  initRTC();
  initSDCard();
  initRadio();
	DEBUGln("=========================="); 
}

void loop() {
	/*
	NOTE: There are a couple interesting things here.
	The radio listens for incoming packets on its network.
	If it recieves a packet, receiveDone() is called, triggering the code inside.
	The sensors will request information, or send information. The requests are logged
	inside of the receiveDone() function, but carried out when the function finishes.
	You can't do a bunch of slow stuff (like writing to an SD card) inside
	receiveDone() or it will break.
	 */
	//initialize triggers as false
	bool writeData = false;
	bool reportTime = false;
	bool ping = false;

	if (radio.receiveDone()) { //if recieve packets from sensor...
		DEBUG("rcv < "); DEBUG('['); DEBUG(radio.SENDERID); DEBUG("] ");
		lastRequesterNodeID = radio.SENDERID; //SENDERID is the Node ID of the device that sent the packet of data
    rtc.update();
    now = rtc.unixtime(); //record time of this event
		theTimeStamp.timestamp = now; //and save it to the global variable

		/*=== TIME ===*/
		//the sensor sends a 't' to request the time to be returned to it
		if(radio.DATALEN == 1 && radio.DATA[0] == 't') {
			DEBUGln("t");
			reportTime = true;
		}
		/*=== PING ===*/
		//the sensor will send a 'p' before sending data
		//NOTE: This doesn't actually work in practice, because if one Node latches
		//NOTE: there is nothing to stop another node from stealing that latch for itself.
		if(radio.DATALEN == 1 && radio.DATA[0] == 'p') {
		//NOTE: Untested, but should work. The nuetral state of this varibale is -1...
		  DEBUGln("p");
		  if(NodeID_latch < 0) { //NOTE: TEST THIS BEFORE DEPLOYMENT, or just comment out
				DEBUG("latch->"); DEBUG('['); DEBUG(radio.SENDERID); DEBUG("]");
        DEBUG(" [RX_RSSI:"); DEBUG(radio.RSSI); DEBUGln("]");
				NodeID_latch = radio.SENDERID;
				ping = true;
			}
		}
		/*=== UN-LATCH ===*/
		//only allow the original latcher to unlatch the connetion.
		if(radio.DATALEN == 1 && radio.DATA[0] == 'r') {
			if(NodeID_latch == radio.SENDERID) {
				DEBUGln("r");
				DEBUG("unlatch->"); DEBUG('['); DEBUG(radio.SENDERID); DEBUGln("] ");
				NodeID_latch = -1;
			}
		}
		/*=== WRITE DATA ===*/
		if(NodeID_latch > 0) {
      DEBUGln();
      DEBUG("Latched to ");DEBUG(NodeID_latch);DEBUG(" ?= SenderID: ");DEBUG(radio.SENDERID);
      DEBUG(" Datalen: ");DEBUG(radio.DATALEN);DEBUG(" =? Payload: ");DEBUGln(sizeof(thePayload));
			if (radio.DATALEN == sizeof(thePayload) && radio.SENDERID == NodeID_latch) {
				thePayload = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
				writeData = true;
				DEBUG("["); DEBUG(radio.SENDERID); DEBUGln("] ");
				DEBUG("@: "); DEBUGln(thePayload.time);
        DEBUGln("     : Y0\tY1\tY2\tY3\tY4\tY5\tY6\tY7");
        DEBUG("wts  : ");
        for(int i = 0; i < 8; i++) {
          DEBUG(float(thePayload.w[i])/10000.0);
          DEBUG(",\t");
        }
        DEBUGln();DEBUG("temps: ");
        for(int i = 0; i < 8; i++) {
          DEBUG(thePayload.t[i]/100);
          DEBUG(",\t");
        }
        DEBUGln();
				DEBUG(" temp: "); DEBUGln(thePayload.board_temp);
				DEBUG(" battery voltage: "); DEBUGln(thePayload.battery_voltage/100);
				DEBUG(" cnt: "); DEBUG(thePayload.count);
				DEBUGln();
			}
		}
		if (radio.ACKRequested()) radio.sendACK(); //send acknoledgement of reciept
		Blink(LED,5); //blink the LED really fast
	}
	/*=== DO THE RESPONSES TO THE MESSAGES ===*/
	//Sends the time to the sensor that requested it
	if(reportTime) {
		DEBUG("snd > "); DEBUG('['); DEBUG(lastRequesterNodeID); DEBUG("] ");
		if(radio.sendWithRetry(lastRequesterNodeID, (const void*)(&theTimeStamp), sizeof(theTimeStamp), ACK_RETRIES, ACK_WAIT_TIME)) {
			DEBUGln(theTimeStamp.timestamp);
		} else {
			DEBUGln("Failed . . . no ack");
		}
	}
	//sends a response back to confirm that the datalogger is alive
	if(ping) {
		DEBUG("snd > "); DEBUG('['); DEBUG(lastRequesterNodeID); DEBUG("] ");
		if(radio.sendWithRetry(lastRequesterNodeID, (const void*)(1), sizeof(1), ACK_RETRIES, ACK_WAIT_TIME)) {
			DEBUGln("1");
		} else {
			DEBUGln("Failed . . . no ack");
		}
	}
	//write recieved data to the SD Card
	if(writeData) {
		File f; //declares a File
		String address = String(String(NETWORKID) + "_" + String(lastRequesterNodeID)); //creates a file name based off of Sender Address
		String fileName = String(address + ".csv"); //Save is a CSV file...because
		//convert String to Char Array
		char _fileName[fileName.length() +1];
		fileName.toCharArray(_fileName, sizeof(_fileName));
		//Open the File
		if (!f.open(_fileName, FILE_WRITE)) {
			DEBUG("sd - error opening ");
			DEBUG(_fileName);
			DEBUGln();
		}
		//And write data to that file
		DEBUG("sd - writing to "); DEBUG(_fileName); DEBUGln();
		f.print(NETWORKID); f.print(".");
		f.print(radio.SENDERID); f.print(",");
    f.print(thePayload.time); f.print(",");
    for(int i = 0; i < 8; i++) {
      f.print(thePayload.w[i]); // these are *10000 need post processed to /10000
      f.print(",");
      f.print(float(thePayload.t[i])/100.0);
      f.print(",");
    }
		f.print(thePayload.count); f.println();
    f.print(float(thePayload.battery_voltage)/100.0); f.print(",");
    f.print(thePayload.board_temp); f.print(",");
    f.close();
		//when done write, Close the File.
		//If file isn't closed, the data won't be saved
	}
	checkSdCard(); //Checks for card insertion
}

/**
 * Checks to make sure the SD Card is still present.
 */
void checkSdCard() {
	CARD_PRESENT = digitalRead(CARD_DETECT); //
	if (!CARD_PRESENT) {
		DEBUGln("sd - card Not Present");
		while (1) {
			Blink(LED, 100);
			Blink(LED, 200); //blink to show an error.
		}
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
  DEBUG(String(rtc.hour()) + ":"); // Print hour
  if (rtc.minute() < 10)
    DEBUG('0'); // Print leading '0' for minute
  DEBUG(String(rtc.minute()) + ":"); // Print minute
  if (rtc.second() < 10)
    DEBUG('0'); // Print leading '0' for second
  DEBUG(String(rtc.second())); // Print second

  if (rtc.is12Hour()) // If we're in 12-hour mode
  {
    // Use rtc.pm() to read the AM/PM state of the hour
    if (rtc.pm())
    {
      DEBUG(" PM"); // Returns true if PM
    }else{ DEBUG(" AM");}
  }

  DEBUG(" | ");

  // Few options for printing the day, pick one:
  DEBUG(rtc.dayStr()); // Print day string
  //DEBUG(rtc.dayC()); // Print day character
  //DEBUG(rtc.day()); // Print day integer (1-7, Sun-Sat)
  DEBUG(" - ");
#ifdef PRINT_USA_DATE
  DEBUG(String(rtc.month()) + "/" +   // Print month
               String(rtc.date()) + "/");  // Print date
#else
  DEBUG(String(rtc.date()) + "/" +    // (or) print date
               String(rtc.month()) + "/"); // Print month
#endif
  DEBUGln(String(rtc.year()));        // Print year
}
/**
   [initRadio description]
*/
void initRadio()
{
  DEBUG("Arduino RFM69HCW Transmitter");
  //Now initialise the radio
  bool check = radio.initialize(FREQUENCY, NODEID, NETWORKID);
  if (check)
  {
    DEBUG("-- Network Address: "); DEBUG(NETWORKID); DEBUG("."); DEBUGln(NODEID);
    radio.setHighPower();    // Only for RFM69HCW & HW!
    //radio.setPowerLevel(31); // power output ranges from 0 (5dBm) to 31 (20dBm)
    radio.encrypt(NULL);
    // radio.encrypt(ENCRYPTKEY);
    // radio.setFrequency(433000000);
#ifdef ENABLE_ATC
    DEBUGln("RFM69_ATC Enabled (Auto Transmission Control)");
    radio.enableAutoPower(ATC_RSSI);
#endif
    DEBUGln("\Transmitting at "); DEBUG(radio.getFrequency());
  } else {
    DEBUGln("Cannot initialize radio");
  }
}
void initRTC()
{
  // Call rtc.begin([cs]) to initialize the library
  // The chip-select pin should be sent as the only parameter
  rtc.begin(RTC_CS_PIN); //initialize RTC
  //*****
  // rtc.autoTime();
  //*****
  // Update time/date values
  rtc.update();
  now = rtc.unixtime(); //get current time
  #ifdef INTERRUPT_PIN // If using the SQW pin as an interrupt
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  #endif
  //printTime();
  digitalWrite(RTC_CS_PIN, HIGH);
}
void initSDCard()
{
  //Check if the SD Card initialized and able to write data
  //NOTE: This is SUPER important!!! If the SD Card fails to initialize
  //NOTE: no data will be saved. It is a MUST to test writing to a file, and having
  //NOTE: foolproof notifications if something goes wrong!!!
  bool sd_OK = false;
  digitalWrite(LED, HIGH); //turn LED on to signal start of test
  CARD_PRESENT = digitalRead(CARD_DETECT); //read Card Detect pin
  if(CARD_PRESENT) { //If the Card is inserted correctly...
    DEBUG("-- SD Present, ");
    if (SD.begin(SD_CS_PIN)) { //Try initializing the Card...
      DEBUG("initialized, ");
      File f; //declare a File
//      SPI doesnt like using two spi devices concurrently 
//      rtc.update();
//      now = rtc.unixtime(); //get current time
      if(f.open("start.txt", FILE_WRITE)) { //Try opening that File
        DEBUGln("file write, OK!");
        DEBUG("-- Time is "); DEBUGln(now);
        printTime();
        //Print to open File
        f.print("program started at: ");
        f.print(now);
        f.println();
        f.close(); //Close file
        sd_OK = true; //SD card is OK!
        digitalWrite(LED, LOW); //turn LED off
      }
    }
  }
  if(!sd_OK) { //If SD is not ok, blink forever.
    while(1) {
      Blink(LED, 100); //blink to show an error.
      Blink(LED, 200); //blink to show an error.
    }
  }
}


