/**
*   removed ping, timestamp, eeprom functions,
*	just takes measurements and sends
*   TODO:
*   Fix hang!
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
#define NODEID    10 //Node Address
#define NETWORKID 100 //Network to communicate on
/****************************************************************************/

#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
#define IS_RFM69HW    //uncomment only for RFM69HW! 
//#define ATC_RSSI -70 //ideal Signal Strength of trasmission
#define ACK_WAIT_TIME 30 // # of ms to wait for an ack
#define ACK_RETRIES 3 // # of attempts before giving up
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
const uint8_t scaleNmeasurements = 20; //no times to measure load cell for average
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
// Define calibration for LC   Y0   , Y1  , Y2  , Y3  , Y4  , Y5  , Y6  , Y8  }
long calibration_factor[8] = {56000,55293,54723,55427,55972,55125,1,1};
long zero_factor[8] =        {1087,-16155,21443,20056,12145,50450,1,1};
uint16_t count = 0; //measurement number

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 9; //sleep time in minutes 
// 80 measurements * 8 ~ 120 sec, 40 ~ 56sec + 12 sec other stuff
const uint16_t SLEEP_MS = 60000; //one minute in milliseconds
const uint32_t SLEEP_SECONDS = SLEEP_INTERVAL * (SLEEP_MS / 1000); //Sleep interval in seconds

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

  pinMode(eX, OUTPUT); // Set excitation
  pinMode(BAT_EN, OUTPUT); // Set excitation
  digitalWrite(eX, LOW);
  digitalWrite(BAT_EN, LOW);
  pinMode(LED, OUTPUT); //led

  //--SETUP OTHER STUFF
  scale.set_scale(calibration_factor[0]);
  scale.set_offset(zero_factor[0]);

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
  while (!ping()) {
    Serial.println("Failed to setup ping");
    //If datalogger doesn't respond, Blink, wait x seconds, and try again
  Serial.flush();
  radio.sleep();
  for (uint8_t i = 0; i < 10; i++)
    Sleepy::loseSomeTime(SLEEP_MS);
    Blink(20, 5);
  }
  DEBUGln("-- Datalogger Available");
}

void loop() {
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
      digitalWrite(LED, HIGH); //turn on LED
      if (radio.sendWithRetry(GATEWAYID, (const void*)(&thisPayload), sizeof(thisPayload)), ACK_RETRIES, ACK_WAIT_TIME) {
        Serial.println(thisPayload.count);
        DEBUG(sizeof(thisPayload)); DEBUG(" bytes -> ");
        DEBUG('['); DEBUG(GATEWAYID); DEBUG("] ");
        DEBUG(" [RX] ");DEBUGln(radio.RSSI);
        digitalWrite(LED, LOW); //Turn Off LED
        //Blink(100, 1);
      } else {
        digitalWrite(LED, LOW); //Turn Off LED
        Serial.println("- send fail");
        DEBUGln("snd - Failed . . . no ack");
        DEBUG("- Saving Locally");
        Blink(20, 2);
      }
      DEBUGln();
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

void Blink(byte DELAY_MS, byte loops) {
  for (byte i = 0; i < loops; i++)
  {
    digitalWrite(LED, HIGH);
    delay(DELAY_MS);
    digitalWrite(LED, LOW);
    delay(DELAY_MS);
  }
}
/**
   Requests a response from the Datalogger. For checking that the Datalogger is online
 
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
      DEBUGln("failed: no ping ack");
      return false; //if there is no response, returns false and exits function
    }
}
