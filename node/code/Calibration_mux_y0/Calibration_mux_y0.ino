/*
 Example using the SparkFun HX711 breakout board with a scale
 By: Nathan Seidle
 SparkFun Electronics
 Date: November 19th, 2014
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
 
 This is the calibration sketch. Use it to determine the calibration_factor that the main example uses. It also
 outputs the zero_factor useful for projects that have a permanent mass on the scale in between power cycles.
 
 Setup your scale and start the sketch WITHOUT a weight on the scale
 Once readings are displayed place the weight on the scale
 Press +/- or a/z to adjust the calibration_factor until the output readings match the known weight
 Use this calibration_factor on the example sketch
 
 This example assumes pounds (lbs). If you prefer kilograms, change the Serial.print(" lbs"); line to kg. The
 calibration factor will be significantly different but it will be linearly related to lbs (1 lbs = 0.453592 kg).
 
 Your calibration factor may be very positive or very negative. It all depends on the setup of your scale system
 and the direction the sensors deflect from zero state

 This example code uses bogde's excellent library: https://github.com/bogde/HX711
 bogde's library is released under a GNU GENERAL PUBLIC LICENSE

 Arduino pin 2 -> HX711 CLK
 3 -> DOUT
 5V -> VCC
 GND -> GND
 
 Most any pin on the Arduino Uno will be compatible with DOUT/CLK.
 
 The HX711 board can be powered from 2.7V to 5V so the Arduino 5V power should be fine.
 
*/
#include "Thermistor.h"
#include "HX711.h"

#define DOUT  8
#define CLK  7

HX711 scale(DOUT, CLK);
Thermistor temp(7);

float calibration_factor = 54509;//54672; //52298.5;
float zero_factor = -4160; //24570;//-23350;//-2200;
//float calibration_factor = 53388.5;//53865.5; //52564.50; //52496.50; //52686.50; //45736.5; //48745; //-7050 worked for my 440lb max scale setup
//float zero_factor = -8760; //-14860; //-13133; //-156; //23167;
const int zOutput = 3.3; 
const int zInput = A7; // Connect common (Z) to A7 (analog input)
const int eX = A5; //Thermister excitation voltage

void setup() {
  Serial.begin(115200);
  // mux 1,y0
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  pinMode(zInput, INPUT); // Set up Z as an input
  pinMode(eX, OUTPUT); // Set excitation
  digitalWrite(eX, HIGH);
  Serial.println("HX711 calibration mux sketch ch1 y0");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");
  Serial.println("Press - or z to decrease calibration factor");
  Serial.println("Press q to increase calibration factor step by 10x");
  Serial.println("Press w to decrease calibration factor step by 10x");

  scale.set_scale();

  scale.set_offset(zero_factor);
  //scale.tare();	//Reset the scale to 0
  //long zero_factor = scale.read_average(100); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
}

float factor = 10.0;

void loop() {

  scale.set_scale(calibration_factor); //Adjust to this calibration factor
  int reading = temp.getTemp();
  Serial.print(String(reading) + "\t");
  Serial.print("Reading: ");
  Serial.print(scale.get_units(10), 4);
  Serial.print(" kg"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
  Serial.print(" calibration_factor: ");
  Serial.print(calibration_factor);
  zero_factor = scale.read_average(10);
  Serial.print("\t zero: ");
  Serial.print(zero_factor,0);

  scale.power_down();

  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += factor;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= factor;
    else if(temp == 'q')
      factor = 10.0f*factor;
    else if(temp == 'w')
      factor = factor/10.0f;
  }
  factor = roundf(factor*10.0f)/10.0f;
  if(factor <= 0.1)
    factor = 0.1;
  Serial.print("\t factor: ");
  Serial.print(factor,1);
  Serial.println();
  scale.power_up();
}
