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
#define NODEID    99 //Node Address
#define NETWORKID 100 //Network to communicate on
/****************************************************************************/

#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
#define IS_RFM69HW    //uncomment only for RFM69HW! 
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

//battery voltage divider. Measure and set these values manually
const int bat_div_R1 = 14700;
const int bat_div_R2 = 3820;
const uint8_t scaleNmeasurements = 10; //no times to measure load cell for average
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
// Define calibration for LC   Y0   , Y1  , Y2  , Y3  , Y4  , Y5  , Y6  , Y8  }
long calibration_factor[8] = {54004,57228,55087,53939,54166,54772,56301,53976};
long zero_factor[8] =        {36576,-13038,-492,-22636,-13376,-11023,-19119,-18178};
uint16_t count = 0; //measurement number

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 1; //sleep time in minutes 
// 80 measurements * 8 ~ 120 sec, 40 ~ 56sec + 12 sec other stuff
const uint16_t SLEEP_MS = 60000; //one minute in milliseconds
const uint32_t SLEEP_SECONDS = SLEEP_INTERVAL * (SLEEP_MS / 1000); //Sleep interval in seconds
unsigned long timeout_start = 0; //holder for timeout counter
int timeout = 5000; //time in milsec to wait for ping/ts response
byte Retries = 3;  //times to try ping and timestamp before giving up
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

struct Payload {
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
  #ifdef IS_RFM69HW 
    radio.setHighPower();
  #endif
  radio.encrypt("sampleEncryptKey");
  #ifdef ATC_RSSI
    radio.enableAutoPower(ATC_RSSI);
   #endif
  Serial.print("-- Network Address: "); Serial.print(NETWORKID); Serial.print("."); Serial.println(NODEID);
  //   Ping the datalogger. If it is alive, it will respond with a 1
  DEBUGln("-- Datalogger Available");
}

void loop() {
  radio.sleep();
  unsigned long starttime = millis();
  count++;
  DEBUG("- Measurement...");DEBUGln(count);
  //Blink(20, 3);
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
    delay(2); //this may or may not be needed
    measureWeight(i);
    digitalWrite(eX, HIGH);
    delay(2);
    thisPayload.t[i] = int(temp.getTemp() * 100); // need to check thermistor resistance on cable
    digitalWrite(eX, LOW);

    DEBUG(float(thisPayload.w[i]) / 10000.0);
    DEBUG("kg ");
    DEBUG(float(thisPayload.t[i]) / 100.0);
    DEBUG(".C ");
  }
  DEBUGln();
  Serial.print("-- mtime: ");Serial.println((millis()-starttime)/1000);
    //If the Datalogger is listening and available to recieve data
    DEBUGln("- Datalogger Available");
    
      digitalWrite(LED, HIGH); //turn on LED
      if (radio.sendWithRetry(GATEWAYID, (const void*)(&thisPayload), sizeof(thisPayload)), ACK_RETRIES, ACK_WAIT_TIME) {
        Serial.println(thisPayload.count);
        DEBUG(sizeof(thisPayload)); DEBUG(" bytes -> ");
        DEBUG('['); DEBUG(GATEWAYID); DEBUG("] ");
        digitalWrite(LED, LOW); //Turn Off LED
        //Blink(100, 1);
      } else {
        digitalWrite(LED, LOW); //Turn Off LED
        Serial.println("- send fail");
        DEBUGln("snd - Failed . . . no ack");
        Blink(20, 2);
      }
      DEBUGln();
  Serial.print("-- cycletime: "); Serial.println((millis()-starttime)/1000);
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
      if (radio.ACKRequested()) {
        radio.sendACK();       
        delay(10);
      }
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
