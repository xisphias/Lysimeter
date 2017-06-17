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
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5

HX711 scale(DOUT, CLK);
Thermistor temp(7);


//float calibration_factor = 53388.5;//53865.5; //52564.50; //52496.50; //52686.50; //45736.5; //48745; //-7050 worked for my 440lb max scale setup
//float zero_factor = -8760; //-14860; //-13133; //-156; //23167;
const int zOutput = 3.3; 
const int zInput = A7; // Connect common (Z) to A7 (analog input)
const int eX = A5; //Thermister excitation voltage

float calibration_factor = 54509;//54672; //52298.5;
float zero_factor = 7200;//-4160; //24570;//-23350;//-2200;
int weight = 3900; //3418;
bool zero = true;
bool cali = false;
int pin = 0;

void setup() {
  Serial.begin(115200);
// Set up the select pins as outputs:
  for (int i=0; i<3; i++)
  {
    pinMode(muxSelectPins[i], OUTPUT);
    digitalWrite(muxSelectPins[i], HIGH);
  }
  digitalWrite(6, LOW); //enable mux
  pinMode(zInput, INPUT); // Set up Z as an input
  pinMode(eX, OUTPUT); // Set excitation
  //digitalWrite(eX, LOW);
  digitalWrite(eX, HIGH); //turn on thermister
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
  //scale.tare();  //Reset the scale to 0
  //long zero_factor = scale.read_average(100); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
}


void loop() {
  selectMuxPin(pin);
  Serial.print("powering..");
  scale.power_up();
    scale.set_scale();
    scale.set_offset(zero_factor);
    scale.set_scale(calibration_factor); //Adjust to this calibration factor
  Serial.print("scale ready \t");
  int reading = temp.getTemp();  
  Serial.println(String(reading) + "C\t");
     Serial.print("--calibration_factor: ");
     Serial.print(calibration_factor);
     Serial.print("-- zero: ");
     Serial.println(zero_factor);
 
  if(Serial.available())
    {
      char temp = Serial.read();
      if(temp == 't')
        zero = true;
      if(temp == 'c')
        cali = true;
      if(temp == 'p')
        delay(5000);
    }
  while(zero){
    Serial.print(scale.get_units(30), 4);
    Serial.print(" kg ");
    zero_factor = scale.read_average(30);
    Serial.print("-- zero: ");
    Serial.println(zero_factor);
    scale.set_offset(zero_factor);
    if(Serial.available())
    {
      char temp = Serial.read();
      if(temp == 't')
        zero = false;
      if(temp == 'p')
        delay(5000);
    }
    zero = false;
  }
  if(cali){
    Serial.print("place weight: "); Serial.println(weight);
    delay(1000);
    Serial.println("starting calibration: ");
  }
  int count = 0;
  while(cali){
    float reading = scale.get_units(30) * 1000;
    Serial.print(reading);
    Serial.print(" g ");
    Serial.print("--calibration_factor: ");
     Serial.print(calibration_factor);
      scale.set_scale(calibration_factor);
    if(Serial.available())
    {
      char temp = Serial.read();
      if(temp == 'c')
        cali = false;
      if(temp == 'p')
        delay(5000);
    }
    
    float diff = weight - reading;
    Serial.print(" --diff: ");
     Serial.print(diff);
    if(diff>0){
      if(diff>100){
        calibration_factor -= 1000;
      }else if(diff>50){
        calibration_factor -= 500;
      }else if(diff>10){
        calibration_factor -= 100;
      }else if(diff>1){
        calibration_factor -= 10;
      }
      else if(diff>0.5){
        calibration_factor -= 1;
      }
    }else if(diff<0){
      if(-diff>100){
        calibration_factor += 1000;
      }else if(-diff>50){
        calibration_factor += 500;
      }else if(-diff>10){
        calibration_factor += 100;
      }else if(-diff>1){
        calibration_factor += 10;
      }
      else if(-diff>0.5){
        calibration_factor += 1;
      }
    }else{
      Serial.print(" No  change ");
      count++;
    }
    Serial.print("--calibration_factor: ");
     Serial.print(calibration_factor);
    Serial.println("---");
//    if(count>5){
//      Serial.print("--- Calibrated!");
//      cali=false;
//    }
  }

  if(!cali && !zero){
    Serial.print(scale.get_units(30), 4);
    Serial.print(" kg ");
    scale.power_down();
    delay(1000);
  }
  
}
// The selectMuxPin function sets the S0, S1, and S2 pins
// accordingly, given a pin from 0-7.
void selectMuxPin(byte pin)
{
   digitalWrite(eX, LOW);
  for (int i=0; i<3; i++)
  {
    if (pin & (1<<i))
      digitalWrite(muxSelectPins[i], HIGH);
    else
      digitalWrite(muxSelectPins[i], LOW);
  }
   digitalWrite(eX, HIGH);  //turn on hx711
}
