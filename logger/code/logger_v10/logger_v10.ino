#include <Arduino.h> //built in
#include <SPI.h> //built in
#include <RFM69_ATC.h>//https://www.github.com/lowpowerlab/rfm69
#include "DS3231.h" //https://github.com/kinasmith/DS3231
#include "SdFat.h" //https://github.com/greiman/SdFat

#define NODEID 0 //Address on Network
#define FREQUENCY RF69_433MHZ //hardware frequency of Radio
#define ATC_RSSI -70 //ideal signal strength
#define ACK_WAIT_TIME 100 // # of ms to wait for an ack
#define ACK_RETRIES 10 // # of attempts before giving up
#define SERIAL_BAUD 115200 //connection speed
#define LED 9 //LED pin
#define SD_CS_PIN 4 //Chip Select pin for SD card
#define CARD_DETECT 5 //Pin to detect presence of SD card

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
uint8_t setAddress();
void Blink(byte, int);
void checkSdCard();

/*==============|| RFM69 ||==============*/
RFM69_ATC radio; //Declare Radio
uint8_t NETWORKID; //The Network this device is on (set via solder jumpers)
byte lastRequesterNodeID = NODEID; //Last Sensor to communicate with this device
int8_t NodeID_latch; //Listen only to this Sensor #

/*==============|| DS3231_RTC ||==============*/
DateTime now; //Holder for Current Time
DS3231 rtc; //Declare the Real Time Clock

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
	uint32_t timestamp;
	uint16_t count;
	uint32_t sense;
	double brd_tmp;
	double bat_v;
	double ref_v;
};
Payload thePayload;


void setup() {
	#ifdef SERIAL_EN
		Serial.begin(SERIAL_BAUD);
	#endif
	DEBUGln("-- Datalogger for Dendrometer System --");
	Wire.begin(); //begin i2c connection
	NETWORKID = setAddress(); //Set the network address based on solder jumper selections
	pinMode(LED, OUTPUT); //Set LED to output
	pinMode(CARD_DETECT, INPUT_PULLUP); //Init. 10k internal Pullup resistor
	rtc.begin(); //initialize RTC
	//*****
	// rtc.adjust(DateTime((__DATE__), (__TIME__))); //sets the RTC to the computer time.
	//*****
	//Initializes the Radio
	radio.initialize(FREQUENCY,NODEID,NETWORKID);
	radio.setHighPower();
	radio.enableAutoPower(ATC_RSSI);
	radio.encrypt(null);
	DEBUG("-- Network Address: "); DEBUG(NETWORKID); DEBUG("."); DEBUGln(NODEID);

	//Check if the SD Card initialized and able to write data
	//NOTE: This is SUPER important!!! If the SD Card fails to initialize
	//NOTE: no data will be saved. It is a MUST to test writing to a file, and having
	//NOTE: foolproof notifications if something goes wrong!!!
	bool sd_OK = false;
	digitalWrite(LED, HIGH); //turn LED on to signal start of test
	CARD_PRESENT = !digitalRead(CARD_DETECT); //read Card Detect pin (logic inverted to stay logical)
	if(CARD_PRESENT) { //If the Card is inserted correctly...
		DEBUG("-- SD Present, ");
		if (SD.begin(SD_CS_PIN)) { //Try initializing the Card...
			DEBUG("initialized, ");
			File f; //declare a File
			now = rtc.now(); //get current time
			if(f.open("start.txt", FILE_WRITE)) { //Try opening that File
				DEBUGln("file write, OK!");
				DEBUG("-- Time is "); DEBUGln(now.unixtime());
				//Print to open File
				f.print("program started at: ");
				f.print(now.unixtime());
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
		now = rtc.now(); //record time of this event
		theTimeStamp.timestamp = now.unixtime(); //and save it to the global variable

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
		if(NodeID_latch < 0) { //NOTE: TEST THIS BEFORE DEPLOYMENT, or just comment out
				DEBUGln("p");
				DEBUG("latch->"); DEBUG('['); DEBUG(radio.SENDERID); DEBUGln("] ");
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
			if (radio.DATALEN == sizeof(thePayload) && radio.SENDERID == NodeID_latch) {
				thePayload = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
				writeData = true;
				DEBUG("["); DEBUG(radio.SENDERID); DEBUG("] ");
				DEBUG("@-"); DEBUG(thePayload.timestamp);
				DEBUG(" sensor-"); DEBUG(thePayload.sense);
				DEBUG(" temp-"); DEBUG(thePayload.brd_tmp);
				DEBUG(" battery voltage-"); DEBUG(thePayload.bat_v);
				DEBUG(" reference voltage-"); DEBUG(thePayload.ref_v);
				DEBUG(" cnt-"); DEBUG(thePayload.count);
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
		f.print(thePayload.timestamp); f.print(",");
		f.print(thePayload.sense); f.print(",");
		f.print(thePayload.brd_tmp); f.print(",");
		f.print(thePayload.bat_v); f.print(",");
		f.print(thePayload.ref_v); f.print(",");
		f.print(thePayload.count); f.println();
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
	CARD_PRESENT = !digitalRead(CARD_DETECT); //invert for logic's sake
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
/**
 * Sets the Network Address by reading the Solder Jumper positions
 * @return The Network Address (100-107)
 */
uint8_t setAddress() {
	//sets network address based on which solder jumpers are closed
	//NOTE: The jumpers are in Binary with three values.
	//NOTE: 000 = address 100
	//NOTE: 100 = address 101
	//NOTE: 010 = address 102
	//NOTE: 110 = address 103
	//NOTE: 001 = address 104
	//NOTE: 011 = address 106
	//NOTE: etc...
	uint8_t addr01, addr02, addr03;
	pinMode(6, INPUT_PULLUP);
	pinMode(7, INPUT_PULLUP);
	pinMode(8, INPUT_PULLUP);
	addr01 = !digitalRead(8);
	addr02 = !digitalRead(6) * 2;
	addr03 = !digitalRead(7) * 4;
	pinMode(6, OUTPUT);
	pinMode(7, OUTPUT);
	pinMode(8, OUTPUT);
	digitalWrite(6, LOW);
	digitalWrite(7, LOW);
	digitalWrite(8, LOW);
	uint8_t addr = addr01 | addr02 | addr03;
	return addr += 100;
}
