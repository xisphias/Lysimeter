#include "Thermistor.h"
#include "HX711.h"

/******************************************************************************
Mux_Thermister_Input

Hardware Hookup:
Mux Breakout ----------- Arduino
     S0 ------------------- 3
     S1 ------------------- 4
     S2 ------------------- 5
     Z -------------------- A7
     EX ------------------- A6
    VCC ------------------- 3.3V
    GND ------------------- GND
******************************************************************************/
/////////////////////
// Pin Definitions //
/////////////////////
const int muxSelectPins[3] = {3, 4, 5}; // S0~3, S1~4, S2~5
const int zOutput = 3.3;
const int zInput = A7; // Connect common (Z) to A7 (analog input)
const int eX = A5; //Thermister excitation voltage
#define DOUT  8
#define CLK  7
//#define zero_factor -2689

HX711 scale(DOUT, CLK);
Thermistor temp(7);

float calibration_factor = 48745;
float zero_factor = 23167;

void setup()
{
  Serial.begin(115200); // Initialize the serial port

  scale.set_scale(calibration_factor);

  scale.set_offset(zero_factor);
  //scale.tare();  //Reset the scale to 0
  //long zero_factor = scale.read_average(10); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  // Set up the select pins as outputs:
  for (int i=0; i<3; i++)
  {
    pinMode(muxSelectPins[i], OUTPUT);
    digitalWrite(muxSelectPins[i], HIGH);
  }
  pinMode(zInput, INPUT); // Set up Z as an input
  pinMode(eX, OUTPUT); // Set excitation
  digitalWrite(eX, LOW);
  // Print the header:
  Serial.println("Y0\t\tY1\t\tY2\t\tY3\t\tY4\t\tY5\t\tY6\t\tY7");
  Serial.println("---\t---\t---\t---\t---\t---\t---\t---");

}

void loop()
{
  //scale.set_scale(calibration_factor);

  // Loop through all eight pins.
  for (byte pin=0; pin<=7; pin++)
  {
    selectMuxPin(pin); // Select one at a time
    Serial.print(scale.get_units(10), 4);
    Serial.print(" kg");
    scale.power_down(); //Put the HX711 in sleep mode

    digitalWrite(eX, HIGH);
    delay(100);
    int reading = temp.getTemp();
    Serial.print(String(reading) + "\t");
    digitalWrite(eX, LOW);
    scale.power_up();
  }
  Serial.println();
}

// The selectMuxPin function sets the S0, S1, and S2 pins
// accordingly, given a pin from 0-7.
void selectMuxPin(byte pin)
{
  for (int i=0; i<3; i++)
  {
    if (pin & (1<<i))
      digitalWrite(muxSelectPins[i], HIGH);
    else
      digitalWrite(muxSelectPins[i], LOW);
  }
}
