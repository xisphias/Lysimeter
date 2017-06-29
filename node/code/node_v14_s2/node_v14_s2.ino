/**
*   TODO:
*   Add timeout function to ping and timestamp
*
**/
#include "Thermistor.h"
#include "HX711.h" //https://github.com/bogde/HX711
#include <EEPROM.h> //Built in
#include "Sleepy.h" //https://github.com/kinasmith/Sleepy
#include "RFM69.h"
#include "RFM69_ATC.h" //https://www.github.com/lowpowerlab/rfm69


/****************************************************************************/
/***********************    DON'T FORGET TO SET ME    ***********************/
/****************************************************************************/
#define NODEID    2 //Node Address
#define NETWORKID 100 //Network to communicate on
/****************************************************************************/

#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
#define IS_RFM69HW    //uncomment only for RFM69HW! 
#define ATC_RSSI -70 //ideal Signal Strength of trasmission
#define ACK_WAIT_TIME 200 // # of ms to wait for an ack
#define ACK_RETRIES 5 // # of attempts before giving up
#define DOUT  8
#define CLK  7
#define zOutput 3.3
#define zInput A7 // Connect common (Z) to A7 (analog input)
#define eX A5 //Thermister excitation voltage
#define BAT_EN A0
#define BAT_V A6
#define LED 9

//#define SERIAL_EN //Comment this out to remove Serial comms and save a few kb's of space
#ifdef SERIAL_EN
#define DEBUG(input)   {Serial.print(input); delay(1);}
#define DEBUGln(input) {Serial.println(input); delay(1);}
#define DEBUGFlush() { Serial.flush(); }
#else
#define DEBUG(input);
#define DEBUGln(input);
#define DEBUGFlush();
#endif

//battery voltage divider. Measure and set these values manually
const int bat_div_R1 = 14700;
const int bat_div_R2 = 3820;
const uint8_t scaleNmeasurements = 80; //no times to measure load cell for average
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
// Define calibration for LC   Y0   , Y1  , Y2  , Y3  , Y4  , Y5  , Y6  , Y8  }
long calibration_factor[8] = {55267,53376,54458,58637,54084,54652,1,1};
long zero_factor[8] =        {-28487,-25673,-29885,-28678,-7084,-4948,1,1};
uint16_t count = 0; //measurement number
uint16_t EEPROM_ADDR = 5; //Start of data storage in EEPROM

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 8; //sleep time in minutes 
// 80 measurements * 8 ~ 120 sec, 40 ~ 56sec + 12 sec other stuff
const uint16_t SLEEP_MS = 60000; //one minute in milliseconds
const uint32_t SLEEP_SECONDS = SLEEP_INTERVAL * (SLEEP_MS / 1000); //Sleep interval in seconds
unsigned long timeout_start = 0; //holder for timeout counter
int timeout = 5000; //time in milsec to wait for ping/ts response
byte Retries = 1;  //times to try ping and timestamp before giving up
int cycletime = 0; //counter for measurment time to add to sleeptime

HX711 scale(DOUT, CLK);
Thermistor temp(7);
ISR(WDT_vect) {
  Sleepy::watchdogEvent(); // set watchdog for Sleepy
}
#ifdef ATC_RSSI
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif //Declare radio (using Automatic Power Adjustments)

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
  long w[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  int t[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

struct Payload {
  uint32_t time = 0;
  uint16_t count = 0;
  int battery_voltage = 0;
  int board_temp = 0;
  long w[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  int t[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};
Payload thisPayload;

void setup() {
  Serial.begin(115200); // Initialize the serial port
  // Set up the select pins as outputs:
  for (int i = 0; i < 3; i++)
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
  //DEBUG("Zero factor: "); DEBUGln(zero_factor); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.

  //Setup Radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
    //set radio rate to 4.8k
    radio.writeReg(0x03,0x1A);
    radio.writeReg(0x04,0x0B);
    //set baud to 9.6k
//  radio.writeReg(0x03,0x0D); 
//  radio.writeReg(0x04,0x05);
    //set radio rate to 1.2k
//  radio.writeReg(0x03,0x68);
//  radio.writeReg(0x04,0x2B);
  #ifdef IS_RFM69HW 
    radio.setHighPower();
  #endif
  radio.encrypt(null);
  #ifdef ATC_RSSI
    radio.enableAutoPower(ATC_RSSI);
   #endif
  DEBUG("-- Network Address: "); DEBUG(NETWORKID); DEBUG("."); DEBUGln(NODEID);
  //   Ping the datalogger. If it is alive, it will respond with a 1
  while (!ping()) {
    DEBUGln("Failed to setup ping");
    //If datalogger doesn't respond, Blink, wait x seconds, and try again
    radio.sleep();
    Serial.flush();
    Sleepy::loseSomeTime(1000);
    Blink(20, 5);
  }
  DEBUGln("-- Datalogger Available");
}

void loop() {
  //Gets current time at start of measurement cycle. Stores in global theTimeStamp struct
  unsigned long starttime = millis();
  bool gotTime = false;
  for (int i = 0; i < Retries; i++) {
    DEBUG("Timestamp request: "); DEBUGln(i);
    gotTime = getTime();
    if (gotTime) {
      i = Retries;
      Serial.print(" [RX:"); Serial.print(radio.RSSI); Serial.print("]");
    }
  }
  if (!gotTime) { //Gets time from datalogger and stores in Global Variable
    DEBUGln("time - No Response from Datalogger");
    Blink(100,2);
    // If the node gets no response the next measurement cycle will have the same timestamp
    // as the last cycle. Could also set the timestamp here to a flag value...
  }
  count++;
  DEBUG("- Measurement...");DEBUGln(count);
  Blink(20, 3);
  thisPayload.count = count;
  thisPayload.battery_voltage = get_battery_voltage();
  DEBUG("Bat V: ");
  DEBUGln(float(thisPayload.battery_voltage) / 100.0);
  thisPayload.board_temp = get_board_temp();
  DEBUG("Board TempC: ");
  DEBUGln(thisPayload.board_temp);
  //   Print the header:
  DEBUGln("ch1\t\tch2\t\tch3\t\tch4\t\tch5\t\tch6\t\tch7\t\tch8");
  DEBUGln("---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---");
  for (uint8_t i = 0; i < 8; i++)
  {
    selectMuxPin(i); // Select one at a time
    delay(10); //this may or may not be needed
    measureWeight(i);
    digitalWrite(eX, HIGH);
    delay(10);
    thisPayload.t[i] = int(temp.getTemp() * 100); // need to check thermistor resistance on cable
    digitalWrite(eX, LOW);

    DEBUG(float(thisPayload.w[i]) / 10000.0);
    DEBUG("kg ");
    DEBUG(float(thisPayload.t[i]) / 100.0);
    DEBUG(".C ");
  }
  DEBUGln();
//  unsigned long onemeasure = millis();
//  measureWeight(7);
//  DEBUG("onemeasure: ");DEBUGln((millis() - onemeasure)/1000);
  bool gotPing = false;
  for (int i = 0; i < Retries; i++) {
    DEBUG("Ping request: "); DEBUGln(i);
    gotPing = ping();
    if (gotPing) i = Retries;
  }
  cycletime = int((millis()-starttime)/1000);
  DEBUG("measurement cycle time: ");DEBUGln(cycletime);
  if (gotPing) {
    //If the Datalogger is listening and available to recieve data
    DEBUGln("- Datalogger Available");
    //Check to see if there is data waiting to be sent
    thisPayload.time = theTimeStamp.timestamp; //set payload time to current time
    //DEBUGln(EEPROM.read(5));
    if (EEPROM.read(5) > 0) { //check the first byte of data storage, if there is data, send it all
      DEBUGln("- Stored Data Available, Sending...");
      sendStoredEEPROMData();
    }
    if (EEPROM.read(5) == 0) { //Make sure there is no data stored, then send the measurement that was just taken
      DEBUG("- No Stored Data, Sending ");
      digitalWrite(LED, HIGH); //turn on LED
      starttime = millis();
      if (radio.sendWithRetry(GATEWAYID, (const void*)(&thisPayload), sizeof(thisPayload)), ACK_RETRIES, ACK_WAIT_TIME) {
        Serial.println(thisPayload.count);
        DEBUG("send time: ");DEBUGln(millis()-starttime);
        DEBUG(sizeof(thisPayload)); DEBUG(" bytes -> ");
        DEBUG('['); DEBUG(GATEWAYID); DEBUG("] ");
        DEBUG(thisPayload.time);
        digitalWrite(LED, LOW); //Turn Off LED
        Blink(100, 1);
      } else {
        digitalWrite(LED, LOW); //Turn Off LED
        Serial.println("- send fail");
        DEBUGln("snd - Failed . . . no ack");
        DEBUG("- Saving Locally");
        writeDataToEEPROM(); //save that data to EEPROM
        Blink(100, 2);
      }
      DEBUGln();
    } 
    else {
    //If there is no response from the Datalogger while sending the stored data, save
    DEBUGln("- Datalogger Not Available for EEPROM send, Saving Locally");
    writeDataToEEPROM(); //save that data to EEPROM
    Blink(100, 3);
    }
  }
  else {
    //If there is no response from the Datalogger save data locally
    Serial.println("- ping fail");
    DEBUGln("- Datalogger Not Available, Saving Locally");
    writeDataToEEPROM(); //save that data to EEPROM
    Blink(100, 3);
  }
  
  DEBUG("- Sleeping for "); DEBUG(SLEEP_SECONDS); DEBUG(" seconds"); DEBUGln();
  Serial.flush();
  radio.sleep();
  for (uint8_t i = 0; i < SLEEP_INTERVAL; i++)
    Sleepy::loseSomeTime(SLEEP_MS);
//    Sleepy::loseSomeTime(1000);
  /*==============|| Wakes Up Here! ||==============*/
}

//measure load cell weight and store values
void measureWeight(uint8_t i)
{
  Blink(20, int(scaleNmeasurements/10));
  scale.power_up(); //powers on HX711
  scale.set_scale(calibration_factor[i]); //sets calibration to this LC
  scale.set_offset(zero_factor[i]);       //sets zero to this LC
  thisPayload.w[i] = round(scale.get_units(scaleNmeasurements) * 10000);
  scale.power_down(); //powers off HX711
}
// The selectMuxPin function sets the S0, S1, and S2 pins
// accordingly, given a pin from 0-7.
void selectMuxPin(uint8_t pin)
{
  for (int i = 0; i < 3; i++)
  {
    if (pin & (1 << i))
      digitalWrite(muxSelectPins[i], HIGH);
    else
      digitalWrite(muxSelectPins[i], LOW);
  }
}

int get_board_temp() {
  int temperature = 0;
  //code to pull temp from RFM69
  temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
  if (temperature > 100) temperature -= 255;
  return temperature;
}

int get_battery_voltage() {
  int readings = 0;
  float v = 0;
  float LSB = 3.3 / 1023;
  float bV = 0;

  digitalWrite(BAT_EN, HIGH);
  delay(10);
  for (byte i = 0; i < 3; i++)
  {
    readings += analogRead(BAT_V);
  }
  readings /= 3;
  v = readings * LSB;
  bV = ((bat_div_R1 + bat_div_R2) * v) / bat_div_R2;
  bV *= 100.0;
  digitalWrite(BAT_EN, LOW);
  return int(bV);
}


/**
   Retrieves data which was stored in EEPROM and sends it to the datalogger
   with corrected timestamps for all recordings based off of the last stored
   time.
*/
void sendStoredEEPROMData() {
  DEBUGln("-- Retrieving Stored EEPROM DATA --");
  Data blank; //init blank struct to erase data
  Data theData; //struct to save data in
  Payload tmp; //for temporary holding of data
  /*
    The stored time is the time of the last successful transmission,
    so we need to add 1 Sleep Interval to the Stored Time to get accurate timestamp
  */
  uint8_t storeIndex = 1;
  uint32_t eep_time = 0UL;
  byte sendRetries = 3;
  EEPROM.get(1, eep_time); //get previously stored time (at address 1)
  EEPROM_ADDR = 1 + sizeof(theTimeStamp.timestamp); //Set to next address
  EEPROM.get(EEPROM_ADDR, theData); //Read in saved data to the Data struct
  DEBUG(".stored time "); DEBUGln(theTimeStamp.timestamp);
  DEBUGln(theData.battery_voltage); //this is where nothing would happen. EEPROM 5 was >0 so we got here, but battery V was 0, so we exited without sending and without clearing the eeprom.
  while (theData.count > 0 && sendRetries < 10) { //while there is data available in the EEPROM // changed to count instead of battery V b/c batV was 0 in theData
    uint32_t rec_time = eep_time + ((cycletime + SLEEP_SECONDS) * storeIndex); //Calculate the actual recorded time
    DEBUG(".eep time "); DEBUGln(eep_time);
    DEBUG(".rec time "); DEBUGln(rec_time);
    //Save data into the Payload struct
    tmp.time = rec_time;
    tmp.count = theData.count;
    tmp.battery_voltage = theData.battery_voltage;
    tmp.board_temp = theData.board_temp;
    for (int i = 0; i < 8; i++)
      tmp.w[i] = theData.w[i];
    for (int i = 0; i < 8; i++)
      tmp.t[i] = theData.t[i];
    //Send data to datalogger
    digitalWrite(LED, HIGH); //turn on LED to signal transmission
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&tmp), sizeof(tmp)), ACK_RETRIES, ACK_WAIT_TIME) {
      Serial.println("-stored sent");
      DEBUG(".data sent, erasing...");
      EEPROM.put(EEPROM_ADDR, blank); //If successfully sent, erase that data chunk...
      EEPROM_ADDR += sizeof(theData); //increment to next...
      DEBUG("reading index "); DEBUGln(EEPROM_ADDR);
      EEPROM.get(EEPROM_ADDR, theData); // and read in the next saved data...
      storeIndex += 1; //and increment the count of saved values
      DEBUG(".store index "); DEBUGln(storeIndex);
      digitalWrite(LED, LOW);
    } else {
      sendRetries++;
      Serial.println("-stored send fail");
      uint16_t waitTime = random(10000);
      digitalWrite(LED, LOW);
      DEBUG(".data send failed:"); DEBUG(sendRetries);
      DEBUG("..waiting for retry for:");
      DEBUG(waitTime);DEBUGln(" ms");
      Blink(20, 5);
      radio.sleep();
      Sleepy::loseSomeTime(waitTime);
    }
  }
  if(!(theData.count > 0)) { //only reset if eeprom was erased
    DEBUGln(1 + sizeof(theTimeStamp.timestamp));
    EEPROM_ADDR = 1 + sizeof(theTimeStamp.timestamp); //Reset Index to beginning of EEPROM_ADDR
    //NOTE: This will front load the fatigue on the EEPROM causing failures of the first indeces
    //NOTE: first. A good thing to do would be to build in a load balancing system to randomly
    //NOTE: place data in the EEPROM with pointers to it.
  }
}
/**
   Saves sensor readings to EEPROM in case of tranmission failures to the datalogger.

*/
void writeDataToEEPROM() {
  DEBUGln("-- Storing EEPROM DATA --");
  Data theData; //Data struct to store data to be written to EEPROM
  uint32_t eep_time = 0UL; //place to hold the saved time
  //pull data from Payload to local struct to save
  theData.count = thisPayload.count;
  theData.battery_voltage = thisPayload.battery_voltage;
  theData.board_temp = thisPayload.board_temp;
  for (int i = 0; i < 8; i++)
    theData.w[i] = thisPayload.w[i];
  for (int i = 0; i < 8; i++)
    theData.t[i] = thisPayload.t[i];

  //update the saved eeprom time to the time of the
  //last successful transaction (if they are different)
  EEPROM.get(1, eep_time);
  DEBUG(".saved time "); DEBUGln(eep_time);
  DEBUG(".current time "); DEBUGln(theTimeStamp.timestamp);
  if (theTimeStamp.timestamp != eep_time) {
    EEPROM.put(1, theTimeStamp.timestamp);
    DEBUGln(".updated time");
  }
  //check if there is space to save the next reading
  //if there isn't, put the device to sleep forever and prime it
  //so that next time it is started, it will dump all saved data to datalogger
  if (EEPROM_ADDR < EEPROM.length() - sizeof(theData)) {
    DEBUG(".saving "); DEBUG(sizeof(theData)); DEBUG(" bytes to address "); DEBUGln(EEPROM_ADDR);
    EEPROM.put(EEPROM_ADDR, theData);
    EEPROM_ADDR += sizeof(theData);
  } else { //if there is not room, set INDEX 0 to 255 and sleep forever
    EEPROM.write(0, 255); //this is not used...
    DEBUGln(".eeprom FULL Sleepying For 12 hours");
    Serial.flush();
    radio.sleep();
    //Sleepy::powerDown();
    for (uint8_t i = 0; i < (60*12); i++) //sleep for 12 hours
       Sleepy::loseSomeTime(SLEEP_MS);
  }
}

/**
   Requests the current time from the datalogger. Time is saved
   to the global struct theTimeStamp.timestamp
   @return boolean, TRUE on recieve, FALSE on failure
   NOTE: This would be better if it just returned the uint32 value
   of the timestamp instead of a boolean and then saving the time to a
   global variable.
   Success checking could still be done with if(!getTime()>0){} if it returned 0 or -1 on failure.
*/
bool getTime()
{
  bool HANDSHAKE_SENT = false;
  bool TIME_RECIEVED = false;
  timeout_start = millis();
  digitalWrite(LED, HIGH); //turn on LED to signal tranmission event
  //Send request for time to the Datalogger
  if (!HANDSHAKE_SENT) {
    DEBUG("time - ");
    if (radio.sendWithRetry(GATEWAYID, "t", 1, ACK_RETRIES, ACK_WAIT_TIME)) { //'t' requests time
      DEBUG("snd . . ");
      HANDSHAKE_SENT = true;
    }
    else {
      //if there is no response, returns false and exits function
      digitalWrite(LED, LOW); //turn off LED
     // Blink(200, 4);
      DEBUGln("failed . . . no ack");
      return false;
    }
  }
  //Wait for the time to be returned from the datalogger
  while (!TIME_RECIEVED && HANDSHAKE_SENT) {
    if (radio.receiveDone()) {
      if (radio.DATALEN == sizeof(theTimeStamp)) { //check to make sure it's the right size
        theTimeStamp = *(TimeStamp*)radio.DATA; //save data to global variable
        DEBUG(" rcv - "); DEBUG('['); DEBUG(radio.SENDERID); DEBUG("] ");
        DEBUG(theTimeStamp.timestamp); DEBUG(" [RX_RSSI:"); DEBUG(radio.RSSI); DEBUG("]"); DEBUGln();
        TIME_RECIEVED = true;
        digitalWrite(LED, LOW); //turn off LED
      }
      else {
        digitalWrite(LED, LOW); //turn off LED
        //Blink(200, 5);
        DEBUGln("failed . . . received not timestamp");
        return false;
      }
      if (radio.ACKRequested()) radio.sendACK();
    }
    if (millis() > timeout_start + timeout) { //it is possible that you could get the time and timeout, but only in the edge case
        DEBUGln(" ...timestamp timeout");
        return false;
    }
  }
  return true;
}

/**
   Requests a response from the Datalogger. For checking that the Datalogger is online
   before sending data to it. It also latches this sensor to the datalogger until a 'r' is sent
   eliminating issues with crosstalk and corruption due to multiple sensors sending data simultaniously.

   @return boolean, TRUE is successful, FALSE is failure
*/
bool ping()
{
  digitalWrite(LED, HIGH); //signal start of communication
   //Send request for status to the Datalogger
    DEBUG("ping - ");
    if (radio.sendWithRetry(GATEWAYID, "p", 1, ACK_RETRIES, ACK_WAIT_TIME)) {
      DEBUGln(" > p");
      digitalWrite(LED, LOW);
      return true;
    }
    else {
      digitalWrite(LED, LOW);
     // Blink(200, 6);
      DEBUGln("failed: no ping ack");
      return false; //if there is no response, returns false and exits function
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
