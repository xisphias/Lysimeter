/**
 * TODO:
 * Add timeout function to ping and timestamp
 *
 */
#include "Thermistor.h"
#include "HX711.h" //https://github.com/bogde/HX711
#include <EEPROM.h> //Built in
#include "Sleepy.h" //https://github.com/kinasmith/Sleepy
#include "RFM69_ATC.h" //https://www.github.com/lowpowerlab/rfm69


/****************************************************************************/
/***********************    DON'T FORGET TO SET ME    ***********************/
/****************************************************************************/
#define NODEID    2 //Node Address
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
#define LED 9
//battery voltage divider. Measure and set these values manually
//#define bat_div_R1 14700
//#define bat_div_R2 3820

//battery voltage divider. Measure and set these values manually
const int bat_div_R1 = 14700;
const int bat_div_R2 = 3820;
const uint8_t scaleNmeasurements = 10; //no times to measure load cell for average
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
// Define calibration for LC   Y0   , Y1  , Y2  , Y3  , Y4  , Y5  , Y6  , Y8  }
long calibration_factor[8] = {55267,53376,54458,58637,54084,54652,1,1};
long zero_factor[8] =        {-28487,-25673,-29885,-28678,-7084,-4948,1,1};
uint16_t count = 0; //measurement number
uint16_t EEPROM_ADDR = 5; //Start of data storage in EEPROM

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 5; //sleep time in minutes
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
  int battery_voltage = 0;
  int board_temp = 0;
  long w[8] = {0,0,0,0,0,0,0,0};
  int t[8] = {0,0,0,0,0,0,0,0};
};

struct Payload {
  uint32_t time = 0;
  uint16_t count = 0;
  int battery_voltage = 0;
  int board_temp = 0;
  long w[8] = {0,0,0,0,0,0,0,0};
  int t[8] = {0,0,0,0,0,0,0,0};
};
Payload thisPayload;

void setup() {
  Serial.begin(115200); // Initialize the serial port
  // Set up the select pins as outputs:
  for (int i=0; i<3; i++)
  {
    pinMode(muxSelectPins[i], OUTPUT);
    digitalWrite(muxSelectPins[i], HIGH);
  }
  // pinMode(zInput, INPUT); // Set up Z as an input Don't need to do this, see below
  pinMode(eX, OUTPUT); // Set excitation
  pinMode(BAT_EN, OUTPUT); // Set excitation
  // pinMode(BAT_V, INPUT); //Don't need to set pinMode for analog inputs. Setting it to INPUT sets as a DIGITAL INPUT, and is then set back to AnalogInput when analogRead() is called
  digitalWrite(eX, LOW);
  digitalWrite(BAT_EN, LOW);
  pinMode(LED, OUTPUT); //led

  //--SETUP OTHER STUFF
  scale.set_scale(calibration_factor[0]);
  scale.set_offset(zero_factor[0]);
  //scale.tare();  //Reset the scale to 0
  //long zero_factor = scale.read_average(10); //Get a baseline reading
  //Serial.print("Zero factor: "); Serial.println(zero_factor); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.

  //Setup Radio
	radio.initialize(FREQUENCY,NODEID,NETWORKID);
	radio.setHighPower();
	radio.encrypt(null);
	radio.enableAutoPower(ATC_RSSI);
	Serial.print("-- Network Address: "); Serial.print(NETWORKID); Serial.print("."); Serial.println(NODEID);
  //	 Ping the datalogger. If it is alive, it will respond with a 1
	while(!ping()) {
		Serial.println("Failed to Setup ping");
		//If datalogger doesn't respond, Blink, wait 60 seconds, and try again
		radio.sleep();
		Serial.flush();
		Blink(100,5);
		Sleepy::loseSomeTime(60000);
	}
	Serial.println("-- Datalogger Available");
}

void loop() {
  //Gets current time at start of measurement cycle. Stores in global theTimeStamp struct
  if(!getTime()) { //Gets time from datalogger and stores in Global Variable
    Serial.println("time - No Response from Datalogger");
    // If the node gets no response the next measurement cycle will have the same timestamp
    // as the last cycle. Could also set the timestamp here to a flag value...
  }
  Serial.println("- Measurement...");
  Blink(50,3);
  count++;
  thisPayload.count = count;
  thisPayload.battery_voltage = get_battery_voltage(); //NOTE: THIS IS NOT TESTED. MAKE SURE IT WORKS
  Serial.print("Bat V: ");
  Serial.println(float(thisPayload.battery_voltage)/100.0);
  thisPayload.board_temp = get_board_temp();
  Serial.print("Board TempC: ");
  Serial.println(thisPayload.board_temp);
  //   Print the header:
  Serial.println("Y0\t\tY1\t\tY2\t\tY3\t\tY4\t\tY5\t\tY6\t\tY7");
  Serial.println("---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---");
  for (uint8_t i = 0; i < 8; i++)
  {
    selectMuxPin(i); // Select one at a time
    delay(10); //this may or may not be needed
    measureWeight(i);
    digitalWrite(eX, HIGH);
    delay(10);
    thisPayload.t[i] = int(temp.getTemp()*100); // this is not tested, need to check thermistor resistance on cable
    digitalWrite(eX, LOW);

    Serial.print(float(thisPayload.w[i])/10000.0,4);
    Serial.print("kg ");
    Serial.print(float(thisPayload.t[i])/100.0,1);
    Serial.print(".C ");
  }
  Serial.println();
	if(ping()) {
		//If the Datalogger is listening and available to recieve data
		Serial.println("- Datalogger Available");
		//Check to see if there is data waiting to be sent
		thisPayload.time = theTimeStamp.timestamp; //set payload time to current time
    //Serial.println(EEPROM.read(5));
		if(EEPROM.read(5) > 0) { //check the first byte of data storage, if there is data, send it all
			Serial.println("- Stored Data Available, Sending...");
			sendStoredEEPROMData();
		}
		if(EEPROM.read(5) == 0) { //Make sure there is no data stored, then send the measurement that was just taken
			Serial.print("- No Stored Data, Sending ");
			digitalWrite(LED, HIGH); //turn on LED
			if (radio.sendWithRetry(GATEWAYID, (const void*)(&thisPayload), sizeof(thisPayload)), ACK_RETRIES, ACK_WAIT_TIME) {
				Serial.print(sizeof(thisPayload)); Serial.print(" bytes -> ");
				Serial.print('['); Serial.print(GATEWAYID); Serial.print("] ");
				Serial.print(thisPayload.time);
				digitalWrite(LED, LOW); //Turn Off LED
			} else {
				Serial.print("snd - Failed . . . no ack");
				Blink(50,2);
			}
			Serial.println();
		}
	}
	else {
		//If there is no response from the Datalogger save data locally
		Serial.println("- Datalogger Not Available, Saving Locally");
		writeDataToEEPROM(); //save that data to EEPROM
		Blink(50,2);
	}
	Serial.print("- Sleeping for "); Serial.print(SLEEP_SECONDS); Serial.print(" seconds"); Serial.println();
	Serial.flush();
	radio.sleep();
	for(uint8_t i = 0; i < SLEEP_INTERVAL; i++)
		Sleepy::loseSomeTime(SLEEP_MS);
//delay(1000);
	/*==============|| Wakes Up Here! ||==============*/
}

//measure load cell weight and store values
void measureWeight(uint8_t i)
{
    Blink(50,scaleNmeasurements);
    scale.power_up(); //powers on HX711
    scale.set_scale(calibration_factor[i]); //sets calibration to this LC
    scale.set_offset(zero_factor[i]);       //sets zero to this LC
    thisPayload.w[i] = round(scale.get_units(scaleNmeasurements)*10000);
    scale.power_down(); //powers off HX711
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

int get_board_temp() {
  int temperature = 0;
  //code to pull temp from RFM69
  temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
  if(temperature > 100) temperature -=255;
  return temperature;
}

int get_battery_voltage() {
  int readings = 0;
  float v = 0;
	float LSB = 3.3/1023;
	float bV = 0;

  digitalWrite(BAT_EN, HIGH);
  Serial.println(v);
  Serial.print(readings);Serial.print(" , ");
  delay(10);
  for (byte i=0; i<3; i++)
  {
    readings += analogRead(BAT_V);
    Serial.print(readings);Serial.print(" , ");
  }
  readings /= 3;
	v = readings * LSB;
	bV = ((bat_div_R1+bat_div_R2) * v)/bat_div_R2;
	bV *= 100.0;
  // v = (3.3) * (readings/1023.0) * (bat_div_R1/bat_div_R2) * 100.0; //Calculate battery voltage
  Serial.print("batV ADC:");Serial.println(readings);
  Serial.println(bV/(100.0));
  digitalWrite(BAT_EN, LOW);
  return int(bV);
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
	Serial.println(theData.battery_voltage); //this is where nothing would happen. EEPROM 5 was >0 so we got here, but battery V was 0, so we exited without sending and without clearing the eeprom.
	while (theData.count > 0) { //while there is data available in the EEPROM // changed to count instead of battery V b/c batV was 0 in theData
		uint32_t rec_time = eep_time + SLEEP_SECONDS*storeIndex; //Calculate the actual recorded time
		Serial.print(".rec time "); Serial.println(rec_time);
		//Save data into the Payload struct
		tmp.time = rec_time;
		tmp.count = theData.count;
    tmp.battery_voltage = theData.battery_voltage;
    tmp.board_temp = theData.board_temp;
    for(int i = 0; i<8; i++)
      tmp.w[i] = theData.w[i];
    for(int i = 0; i<8; i++)
      tmp.t[i] = theData.t[i];
		//Send data to datalogger
		digitalWrite(LED, HIGH); //turn on LED to signal transmission
		if (radio.sendWithRetry(GATEWAYID, (const void*)(&tmp), sizeof(tmp)), ACK_RETRIES, ACK_WAIT_TIME) {
			Serial.print(".data sent, erasing...");
			EEPROM.put(EEPROM_ADDR, blank); //If successfully sent, erase that data chunk...
			EEPROM_ADDR += sizeof(theData); //increment to next...
			Serial.print("reading index "); Serial.println(EEPROM_ADDR);
			EEPROM.get(EEPROM_ADDR, theData); // and read in the next saved data...
			storeIndex += 1; //and increment the count of saved values
			Serial.print(".store index "); Serial.println(storeIndex);
			digitalWrite(LED, LOW);
		} else {
			//this has never happened...
			Serial.print(".data send failed, waiting for retry");
			uint16_t waitTime = random(1000);
			Blink(100,5);
			radio.sleep();
			Sleepy::loseSomeTime(waitTime);
		}
	}
	Serial.println(1 + sizeof(theTimeStamp.timestamp));
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
  theData.battery_voltage = thisPayload.battery_voltage;
  theData.board_temp = thisPayload.board_temp;
  for(int i = 0; i<8; i++)
    theData.w[i] = thisPayload.w[i];
  for(int i = 0; i<8; i++)
    theData.t[i] = thisPayload.t[i];

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
    Serial.flush();
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
	digitalWrite(LED, HIGH); //turn on LED to signal tranmission event
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
				digitalWrite(LED, LOW); //turn off LED
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
 Serial.print(" waiting for ping response");
	while(!PING_RECIEVED && PING_SENT) { //Wait for the ping to be returned
		if (radio.receiveDone()) {
      Serial.print(radio.DATA[0]);
			if (radio.DATALEN == sizeof(1)) { //check to make sure it's the right size
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

void Blink(byte DELAY_MS, byte loops) {
  for (byte i = 0; i < loops; i++)
  {
    digitalWrite(LED, HIGH);
    delay(DELAY_MS);
    digitalWrite(LED, LOW);
    delay(DELAY_MS);
  }
}
