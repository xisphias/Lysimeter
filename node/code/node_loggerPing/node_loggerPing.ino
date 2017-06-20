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
#define NODEID    99 //Node Address
#define NETWORKID 200 //Network to communicate on
/****************************************************************************/

#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
//#define ATC_RSSI -70 //ideal Signal Strength of trasmission
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
long calibration_factor[8] = {54337,55212,54938,53975,54750,54565,1,1};
long zero_factor[8] =        {26527,27452,6621,23456,-19810,7665,1,1};
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

  //Setup Radio
	radio.initialize(FREQUENCY,NODEID,NETWORKID);
	radio.setHighPower();
	radio.encrypt(null);
//	radio.enableAutoPower(ATC_RSSI);
	Serial.print("-- Network Address: "); Serial.print(NETWORKID); Serial.print("."); Serial.println(NODEID);
  //	 Ping the datalogger. If it is alive, it will respond with a 1
	while(!ping()) {
		Serial.println("Failed to Setup ping");
		//If datalogger doesn't respond, Blink, wait 60 seconds, and try again
		radio.sleep();
		Serial.flush();
		Blink(100,5);
		Sleepy::loseSomeTime(1000);
	}
	Serial.println("-- Datalogger Available, setup complete ");
}

void loop() {
  Blink(500,1);
  //Gets current time at start of measurement cycle. Stores in global theTimeStamp struct
  if(!getTime()) { //Gets time from datalogger and stores in Global Variable
    Serial.println("time - No Response from Datalogger");
    // If the node gets no response the next measurement cycle will have the same timestamp
    // as the last cycle. Could also set the timestamp here to a flag value...
  }
  Serial.println();
	if(ping()) {
		//If the Datalogger is listening and available to recieve data
		Serial.print("- Datalogger Available ");
	}
  delay(500);
  Blink(500,1);

  Serial.flush();
	radio.sleep();
//	for(uint8_t i = 0; i < SLEEP_INTERVAL; i++)
//		Sleepy::loseSomeTime(SLEEP_MS);
  delay(3000);
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
	//digitalWrite(LED, HIGH); //turn on LED to signal tranmission event
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
	//digitalWrite(LED, HIGH); //signal start of communication
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
				Serial.print(radio.DATA[0]); Serial.print(" [RX_RSSI:"); Serial.print(radio.RSSI); Serial.print("]"); 
				byte sigStr = 11 - byte(radio.RSSI * -0.1);
        Serial.print("   [sigStr:");Serial.print(sigStr);Serial.print("]");Serial.println();
        Blink(400,sigStr);
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
