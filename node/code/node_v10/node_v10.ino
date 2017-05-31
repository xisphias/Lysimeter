#include "Thermistor.h"
#include "HX711.h" //https://github.com/bogde/HX711
#include <EEPROM.h> //Built in
#include "Sleepy.h" //https://github.com/kinasmith/Sleepy
#include "RFM69_ATC.h" //https://www.github.com/lowpowerlab/rfm69


/****************************************************************************/
/***********************    DON'T FORGET TO SET ME    ***********************/
/****************************************************************************/
#define NODEID    9 //Node Address
#define NETWORKID 100 //Network to communicate on
/****************************************************************************/

#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
#define ATC_RSSI -70 //ideal Signal Strength of trasmission
#define ACK_WAIT_TIME 100 // # of ms to wait for an ack
#define ACK_RETRIES 10 // # of attempts before giving up

#define DOUT  8
#define CLK  7
#define zOutput 3.3
#define zInput A7 // Connect common (Z) to A7 (analog input)
#define eX A5 //Thermister excitation voltage
#define BAT_EN A0
#define BAT_V A6
//battery voltage divider. Measure and set these values manually
#define bat_div_R1 15000
#define bat_div_R2 3900

//#define zero_factor -2689

const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
float calibration_factor = 48745;
float zero_factor = 23167;
uint16_t count = 0; //measurement number
uint16_t EEPROM_ADDR = 5; //Start of data storage in EEPROM

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 15; //sleep time in minutes
const uint16_t SLEEP_MS = 60000; //one minute in milliseconds
const uint32_t SLEEP_SECONDS = SLEEP_INTERVAL * (SLEEP_MS/1000); //Sleep interval in seconds


HX711 scale(DOUT, CLK);
Thermistor temp(7);
ISR(WDT_vect){ Sleepy::watchdogEvent();} // set watchdog for Sleepy
RFM69_ATC radio; //Declare radio (using Automatic Power Adjustments)

/*==============|| DATA ||==============*/
//Data structure for transmitting the Timestamp from datalogger to sensor (4 bytes)
struct TimeStamp {
	uint32_t timestamp;
};
TimeStamp theTimeStamp; //creates global instantiation of this

//Data structure for storing data in EEPROM
struct Data {
	uint16_t count = 0;
  float battery_voltage = 0;
  float board_temp = 0;
  float w[8] = {0,0,0,0,0,0,0,0};
  float t[8] = {0,0,0,0,0,0,0,0};
};

struct Payload {
  uint32_t time = 0;
  float battery_voltage = 0;
  float board_temp = 0;
  float w[8] = {0,0,0,0,0,0,0,0};
  float t[8] = {0,0,0,0,0,0,0,0};
};
Payload thisPayload;

void setup() {
  Serial.begin(115200); // Initialize the serial port
  //Setup Radio
	radio.initialize(FREQUENCY,NODEID,NETWORKID);
	radio.setHighPower();
	radio.encrypt(null);
	radio.enableAutoPower(ATC_RSSI);
	Serial.print("-- Network Address: "); Serial.print(NETWORKID); Serial.print("."); Serial.println(NODEID);
	// Ping the datalogger. If it is alive, it will respond with a 1, and latch to this node until it recieves an "r"
	while(!ping()) {
		Serial.println("Failed to Setup ping");
		//If datalogger doesn't respond, Blink, wait 5 seconds, and try again
		radio.sleep();
		Serial.flush();
		Blink(250);
		Blink(250);
		Sleepy::loseSomeTime(5000);
	}
	Serial.println("-- Datalogger Available")
	//Tell datalogger to unlatch
	if (!radio.sendWithRetry(GATEWAYID, "r", 1)) {
		Serial.println("snd - unlatch failed...");
	}

  //--SETUP OTHER STUFF
  scale.set_scale(calibration_factor);
  scale.set_offset(zero_factor);
  //scale.tare();  //Reset the scale to 0
  //long zero_factor = scale.read_average(10); //Get a baseline reading
  Serial.print("Zero factor: "); Serial.println(zero_factor); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  // Set up the select pins as outputs:
  for (int i=0; i<3; i++)
  {
    pinMode(selectPins[i], OUTPUT);
    digitalWrite(selectPins[i], HIGH);
  }
  pinMode(zInput, INPUT); // Set up Z as an input
  pinMode(eX, OUTPUT); // Set excitation
  digitalWrite(eX, LOW);
  // Print the header:
  Serial.println("Y0\t\tY1\t\tY2\t\tY3\t\tY4\t\tY5\t\tY6\t\tY7");
  Serial.println("---\t---\t---\t---\t---\t---\t---\t---");
}

void loop() {
  //Gets current time at start of measurement cycle. Stores in global theTimeStamp struct
  if(!getTime()) { //Gets time from datalogger and stores in Global Variable
    Serial.println("time - No Response from Datalogger");
  }
  Serial.print("- Measurement...");

  thisPayload.battery_voltage = get_battery_voltage();
  thisPayload.board_temp = get_board_temp();
  for (uint8_t i = 0; i < 8; i++)
  {
    selectMuxPin(i); // Select one at a time
    delay(10); //this may or may not be needed
    scale.power_up(); //powers on HX711
    thisPayload.w[i] = scale.get_units(10), 4);
    scale.power_down(); //powers off HX711
    digitalWrite(eX, HIGH);
    delay(10);
    thisPayload.t[i] = temp.getTemp();
    digitalWrite(eX, LOW);
  }
	if(ping()) {
		//If the Datalogger is listening and available to recieve data
		Serial.println("- Datalogger Available");
		//Check to see if there is data waiting to be sent
		thePayload.timestamp = theTimeStamp.timestamp; //set payload time to current time
		if(EEPROM.read(5) > 0) { //check the first byte of data storage, if there is data, send it all
			Serial.println("- Stored Data Available, Sending...");
			sendStoredEEPROMData();
		}
		if(EEPROM.read(5) == 0) { //Make sure there is no data stored, then send the measurement that was just taken
			Serial.print("- No Stored Data, Sending ")
			digitalWrite(LED, LOW); //turn on LED
			if (radio.sendWithRetry(GATEWAYID, (const void*)(&thePayload), sizeof(thePayload)), ACK_RETRIES, ACK_WAIT_TIME) {
				Serial.print(sizeof(thePayload)); Serial.print(" bytes -> ");
				Serial.print('['); Serial.print(GATEWAYID); Serial.print("] ");
				digitalWrite(LED, HIGH); //Turn Off LED
			} else {
				Serial.print("snd - Failed . . . no ack");
				Blink(50);
				Blink(50);
			}
			Serial.println();
		}
		//disengage the the Datalogger
		if(!radio.sendWithRetry(GATEWAYID, "r", 1)) {
			Serial.println("snd - unlatch failed ...");
		}
	}
	else {
		//If there is no response from the Datalogger save data locally
		Serial.println("- Datalogger Not Available, Saving Locally");
		writeDataToEEPROM(); //save that data to EEPROM
		Blink(50);
		Blink(50);
	}
	Serial.print("- Sleeping for "); Serial.print(SLEEP_SECONDS); Serial.print(" seconds"); Serial.println();
	Serial.flush();
	radio.sleep();
	count++;
	for(uint8_t i = 0; i < SLEEP_INTERVAL; i++)
		Sleepy::loseSomeTime(SLEEP_MS);
	/*==============|| Wakes Up Here! ||==============*/
}

// The selectMuxPin function sets the S0, S1, and S2 pins
// accordingly, given a pin from 0-7.
void selectMuxPin(uint8_t pin)
{
  for (int i=0; i<3; i++)
  {
    if (pin & (1<<i))
      digitalWrite(muxSelectPins[i], HIGH);
    else
      digitalWrite(muxSelectPins[i], LOW);
  }
}

float get_board_temp() {
  float temp = 0;
  //code to pull temp from HX711
  return temp;
}

float get_battery_voltage() {
  uint16_t readings = 0;
  digitalWrite(BAT_EN, HIGH);
  delay(10);
  for (byte i=0; i<3; i++)
    readings += analogRead(BAT_V);
  readings /= 3;
  float v = 3.3 * (readings/1023.0) * (bat_div_R1/bat_div_R2); //Calculate battery voltage
  digitalWrite(BAT_EN, LOW);
  return v;
}


/**
 * Retrieves data which was stored in EEPROM and sends it to the datalogger
 * with corrected timestamps for all recordings based off of the last stored
 * time.
 */
void sendStoredEEPROMData() {
	Serial.println("-- Retrieving Stored EEPROM DATA --");
	Data blank; //init blank struct to erase data
	Data theData; //struct to save data in
	Payload tmp; //for temporary holding of data
/*
	The stored time is the time of the last successful transmission,
	so we need to add 1 Sleep Interval to the Stored Time to get accurate timestamp
 */
	uint8_t storeIndex = 1;
	uint32_t eep_time = 0UL;
	EEPROM.get(1, eep_time); //get previously stored time (at address 1)
	EEPROM_ADDR = 1 + sizeof(theTimeStamp.timestamp); //Set to next address
	EEPROM.get(EEPROM_ADDR, theData); //Read in saved data to the Data struct
	Serial.print(".stored time "); Serial.println(theTimeStamp.timestamp);
	while (theData.bat_v > 0) { //while there is data available in the EEPROM
		uint32_t rec_time = eep_time + SLEEP_SECONDS*storeIndex; //Calculate the actual recorded time
		Serial.print(".rec time "); Serial.println(rec_time);
		//Save data into the Payload struct
		tmp.timestamp = rec_time;
		tmp.count = theData.count;
    tmp.battery_voltage = theData.battery_voltage;
    tmp.board_temp = theData.board_temp;
    tmp.w = theData.w;
    tmp.t = theData.t;
		//Send data to datalogger
		digitalWrite(LED, LOW); //turn on LED to signal transmission
		if (radio.sendWithRetry(GATEWAYID, (const void*)(&tmp), sizeof(tmp)), ACK_RETRIES, ACK_WAIT_TIME) {
			Serial.print(".data sent, erasing...");
			EEPROM.put(EEPROM_ADDR, blank); //If successfully sent, erase that data chunk...
			EEPROM_ADDR += sizeof(theData); //increment to next...
			Serial.print("reading index "); Serial.println(EEPROM_ADDR);
			EEPROM.get(EEPROM_ADDR, theData); // and read in the next saved data...
			storeIndex += 1; //and increment the count of saved values
			Serial.print(".store index "); Serial.println(storeIndex);
			digitalWrite(LED, HIGH);
		} else {
			//this has never happened...
			Serial.print(".data send failed, waiting for retry");
			uint16_t waitTime = random(1000);
			for(int i = 0; i < 5; i++)
				Blink(100);
			radio.sleep();
			Sleepy::loseSomeTime(waitTime);
		}
	}
	EEPROM_ADDR = 1 + sizeof(theTimeStamp.timestamp); //Reset Index to beginning of EEPROM_ADDR
	//NOTE: This will front load the fatigue on the EEPROM causing failures of the first indeces
	//NOTE: first. A good thing to do would be to build in a load balancing system to randomly
	//NOTE: place data in the EEPROM with pointers to it.
}
/**
 * Saves sensor readings to EEPROM in case of tranmission failures to the datalogger.
 *
 */
void writeDataToEEPROM() {
	Serial.println("-- Storing EEPROM DATA --");
	Data theData; //Data struct to store data to be written to EEPROM
	uint32_t eep_time = 0UL; //place to hold the saved time
	//pull data from Payload to local struct to save
	theData.count = thisPayload.count;
  tmp.battery_voltage = thisPayload.battery_voltage;
  tmp.board_temp = thisPayload.board_temp;
  tmp.w = thisPayload.w;
  tmp.t = thisPayload.t;

	//update the saved eeprom time to the time of the
	//last successful transaction (if they are different)
	EEPROM.get(1, eep_time);
	Serial.print(".saved time "); Serial.println(eep_time);
	Serial.print(".current time "); Serial.println(theTimeStamp.timestamp);
	if(theTimeStamp.timestamp != eep_time) {
		EEPROM.put(1, theTimeStamp.timestamp);
		Serial.println(".updated time");
	}
	//check if there is space to save the next reading
	//if there isn't, put the device to sleep forever and prime it
	//so that next time it is started, it will dump all saved data to datalogger
	if(EEPROM_ADDR < EEPROM.length() - sizeof(theData)) {
		Serial.print(".saving "); Serial.print(sizeof(theData)); Serial.print(" bytes to address "); Serial.println(EEPROM_ADDR);
		EEPROM.put(EEPROM_ADDR, theData);
		EEPROM_ADDR += sizeof(theData);
	} else { //if there is not room, set INDEX 0 to 255 and sleep forever
		EEPROM.write(0, 255);
		Serial.println(".eeprom FULL Sleepying Forever");
		radio.sleep();
		Sleepy::powerDown();
	}
}

/**
 * Requests the current time from the datalogger. Time is saved
 * to the global struct theTimeStamp.timestamp
 * @return boolean, TRUE on recieve, FALSE on failure
 * NOTE: This would be better if it just returned the uint32 value
 * of the timestamp instead of a boolean and then saving the time to a
 * global variable.
 * Success checking could still be done with if(!getTime()>0){} if it returned 0 or -1 on failure.
 */
bool getTime()
{
	bool HANDSHAKE_SENT = false;
	bool TIME_RECIEVED = false;
	digitalWrite(LED, LOW); //turn on LED to signal tranmission event
	//Send request for time to the Datalogger
	if(!HANDSHAKE_SENT) {
		Serial.print("time - ");
		if (radio.sendWithRetry(GATEWAYID, "t", 1)) { //'t' requests time
			Serial.print("snd . . ");
			HANDSHAKE_SENT = true;
		}
		else {
			//if there is no response, returns false and exits function
			Serial.println("failed . . . no ack");
			return false;
		}
	}
	//Wait for the time to be returned from the datalogger
	while(!TIME_RECIEVED && HANDSHAKE_SENT) {
		if (radio.receiveDone()) {
			if (radio.DATALEN == sizeof(theTimeStamp)) { //check to make sure it's the right size
				theTimeStamp = *(TimeStamp*)radio.DATA; //save data to global variable
				Serial.print(" rcv - "); Serial.print('['); Serial.print(radio.SENDERID); Serial.print("] ");
				Serial.print(theTimeStamp.timestamp); Serial.print(" [RX_RSSI:"); Serial.print(radio.RSSI); Serial.print("]"); Serial.println();
				TIME_RECIEVED = true;
				digitalWrite(LED, HIGH); //turn off LED
			}
			if (radio.ACKRequested()) radio.sendACK();
		}
	}
	return true;
}
/**
 * Requests a response from the Datalogger. For checking that the Datalogger is online
 * before sending data to it. It also latches this sensor to the datalogger until a 'r' is sent
 * eliminating issues with crosstalk and corruption due to multiple sensors sending data simultaniously.
 *
 * @return boolean, TRUE is successful, FALSE is failure
 */
bool ping()
{
	bool PING_SENT = false;
	bool PING_RECIEVED = false;
	digitalWrite(LED, HIGH); //signal start of communication
	if(!PING_SENT) { //Send request for status to the Datalogger
		Serial.print("ping - ");
		if (radio.sendWithRetry(GATEWAYID, "p", 1)) {
			Serial.println(" > p");
			PING_SENT = true;
		}
		else {
			Serial.println("failed: no ack");
			return false; //if there is no response, returns false and exits function
		}
	}
	while(!PING_RECIEVED && PING_SENT) { //Wait for the ping to be returned
		if (radio.receiveDone()) {
			if (radio.DATALEN == sizeof('p')) { //check to make sure it's the right size
				Serial.print('['); Serial.print(radio.SENDERID); Serial.print("] > ");
				Serial.print(radio.DATA[0]); Serial.print(" [RX_RSSI:"); Serial.print(radio.RSSI); Serial.print("]"); Serial.println();
				PING_RECIEVED = true;
				digitalWrite(LED, LOW);
			}
			if (radio.ACKRequested()) radio.sendACK();
			return true;
		}
	}
}


void Blink(uint8_t t)
{
 	digitalWrite(LED, LOW); //turn LED on
 	delay(t);
 	digitalWrite(LED, HIGH); //turn LED off
 	delay(t);
}
