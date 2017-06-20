/**
 * TODO:
 * Add timeout function to ping and timestamp
 *
 */
#include "Thermistor.h"
#include "HX711.h" //https://github.com/bogde/HX711
#include <EEPROM.h> //Built in
#include "Sleepy.h" //https://github.com/kinasmith/Sleepy

#define DOUT  8
#define CLK  7
#define zOutput 3.3
#define zInput A7 // Connect common (Z) to A7 (analog input)
#define eX A5 //Thermister excitation voltage
#define BAT_EN A0
#define BAT_V A6
#define LED 9

//battery voltage divider. Measure and set these values manually
const int bat_div_R1 = 14700;
const int bat_div_R2 = 3820;
const uint8_t scaleNmeasurements = 40; //no times to measure load cell for average
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
// Define calibration for LC   Y0   , Y1  , Y2  , Y3  , Y4  , Y5  , Y6  , Y8  }
long calibration_factor[8] = {55272,1,1,1,1,1,1,1};
long zero_factor[8] =        {21071,1,1,1,1,1,1,1};
uint16_t count = 0; //measurement number
uint16_t EEPROM_ADDR = 5; //Start of data storage in EEPROM

/*==============|| TIMING ||==============*/
const uint8_t SLEEP_INTERVAL = 1; //sleep time in minutes
const uint16_t SLEEP_MS = 60000; //one minute in milliseconds
const uint32_t SLEEP_SECONDS = SLEEP_INTERVAL * (SLEEP_MS/1000); //Sleep interval in seconds


HX711 scale(DOUT, CLK);
Thermistor temp(7);
ISR(WDT_vect){ Sleepy::watchdogEvent();} // set watchdog for Sleepy
//RFM69_ATC radio; //Declare radio (using Automatic Power Adjustments)

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
		Serial.flush();
		Blink(100,5);
}

void loop() {
  Blink(100,1);
  count++;
  thisPayload.count = count;
  thisPayload.battery_voltage = get_battery_voltage(); //NOTE: THIS IS NOT TESTED. MAKE SURE IT WORKS
//  Serial.print("Bat V: ");
//  Serial.println(float(thisPayload.battery_voltage)/100.0);
  //thisPayload.board_temp = get_board_temp();
//  Serial.print("Board TempC: ");
//  Serial.println(thisPayload.board_temp);
  //   Print the header:
//  Serial.println("Y0\t\tY1\t\tY2\t\tY3\t\tY4\t\tY5\t\tY6\t\tY7");
//  Serial.println("---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---\t\t---");
//  for (uint8_t i = 0; i < 8; i++)
//  {
//    selectMuxPin(i); // Select one at a time
//    delay(10); //this may or may not be needed
//    measureWeight(i);
//    digitalWrite(eX, HIGH);
//    delay(10);
//    thisPayload.t[i] = int(temp.getTemp()*100); // this is not tested, need to check thermistor resistance on cable
//    digitalWrite(eX, LOW);
//
//    Serial.print(float(thisPayload.w[i])/10000.0,4);
//    Serial.print("kg ");
//    Serial.print(float(thisPayload.t[i])/100.0,1);
//    Serial.print(".C ");
//  }
    selectMuxPin(0); // Select one at a time
    delay(10); //this may or may not be needed
    measureWeight(0);
    digitalWrite(eX, HIGH);
    delay(10);
    thisPayload.t[0] = int(temp.getTemp()*100); // this is not tested, need to check thermistor resistance on cable
    digitalWrite(eX, LOW);
    Serial.print(float(thisPayload.w[0])/10000.0,4);
    //Serial.print(" kg ");
    Serial.println();

	//Serial.print("- Sleeping for "); Serial.print(SLEEP_SECONDS); Serial.print(" seconds"); Serial.println();
	Serial.flush();
//	for(uint8_t i = 0; i < SLEEP_INTERVAL; i++)
//		Sleepy::loseSomeTime(SLEEP_MS);
//delay(100);
	/*==============|| Wakes Up Here! ||==============*/
}

//measure load cell weight and store values
void measureWeight(uint8_t i)
{
    scale.power_up(); //powers on HX711
    scale.set_scale(calibration_factor[i]); //sets calibration to this LC
    scale.set_offset(zero_factor[i]);       //sets zero to this LC
    scale.get_units(10);
    Blink(20,scaleNmeasurements);
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

//int get_board_temp() {
//  int temperature = 0;
//  //code to pull temp from RFM69
//  temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
//  if(temperature > 100) temperature -=255;
//  return temperature;
//}

int get_battery_voltage() {
  int readings = 0;
  float v = 0;
	float LSB = 3.3/1023;
	float bV = 0;

  digitalWrite(BAT_EN, HIGH);
  //Serial.println(v);
  //Serial.print(readings);Serial.print(" , ");
  delay(10);
  for (byte i=0; i<3; i++)
  {
    readings += analogRead(BAT_V);
    //Serial.print(readings);Serial.print(" , ");
  }
  readings /= 3;
	v = readings * LSB;
	bV = ((bat_div_R1+bat_div_R2) * v)/bat_div_R2;
	bV *= 100.0;
  // v = (3.3) * (readings/1023.0) * (bat_div_R1/bat_div_R2) * 100.0; //Calculate battery voltage
  //Serial.print("batV ADC:");Serial.println(readings);
  //Serial.println(bV/(100.0));
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
