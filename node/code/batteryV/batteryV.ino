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
const int bat_div_R1 = 14700;
const int bat_div_R2 = 3820;

void setup() {
  Serial.begin(115200); // Initialize the serial port

  pinMode(BAT_EN, OUTPUT); // Set excitation
  pinMode(BAT_V, INPUT);
  digitalWrite(BAT_EN, LOW);
  pinMode(LED, OUTPUT); //led

}
float battery_voltage = 0.0;
void loop() {
  Serial.println("- Measurement...");
//  Blink(50,3);
  battery_voltage = get_battery_voltage(); //NOTE: THIS IS NOT TESTED. MAKE SURE IT WORKS
  Serial.print("Bat V: ");
  Serial.println(float(battery_voltage)/100.0);

delay(5000);

}

int get_battery_voltage() {
  int readings = 0;
  float v = 0;
  digitalWrite(BAT_EN, HIGH);
  digitalWrite(LED, HIGH);
  Serial.println(v);
  Serial.print(readings);Serial.print(" , ");
  delay(1000);
  for (byte i=0; i<3; i++)
  {
    readings += analogRead(BAT_V);
    Serial.print(readings);Serial.print(" , ");
  }
  readings /= 3;
 v = (3.3) * ((readings)/1023.0) * ((bat_div_R1)/bat_div_R2) * 100.0; //Calculate battery voltage
  Serial.print("batV ADC:");Serial.println(readings);
  Serial.println(v/100.0);
  digitalWrite(BAT_EN, LOW);
  digitalWrite(LED, LOW);
  return int(v);
}
