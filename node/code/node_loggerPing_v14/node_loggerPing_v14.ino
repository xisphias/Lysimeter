/**
   TODO:
   Add timeout function to ping and timestamp

*/
#include "Thermistor.h"
#include "HX711.h" //https://github.com/bogde/HX711
#include <EEPROM.h> //Built in
#include "Sleepy.h" //https://github.com/kinasmith/Sleepy
#include "RFM69.h" //https://www.github.com/lowpowerlab/rfm69
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
//#define ATC_RSSI -70 //ideal signal strength
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

unsigned long timeout_start = 0; //holder for timeout counter
int timeout = 5000; //time in milsec to wait for ping/ts response
byte Retries = 3;  //times to try ping and timestamp before giving up

ISR(WDT_vect) {
  Sleepy::watchdogEvent(); // set watchdog for Sleepy
}
#ifdef ATC_RSSI
RFM69_ATC radio;
#else
RFM69 radio;
#endif//Declare Radio
/*==============|| DATA ||==============*/
//Data structure for transmitting the Timestamp from datalogger to sensor (4 bytes)
struct TimeStamp {
  uint32_t timestamp;
};
TimeStamp theTimeStamp; //creates global instantiation of this

void setup() {
  Serial.begin(115200); // Initialize the serial port
  pinMode(LED, OUTPUT); //led

  //Setup Radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  //set radio rate to 4.8k
//  radio.writeReg(0x03, 0x1A);
//  radio.writeReg(0x04, 0x0B);
  //    //set radio rate to 1.2k
  //    radio.writeReg(0x03,0x68);
  //    radio.writeReg(0x04,0x2B);
  //set baud to 9.6k
  //    radio.writeReg(0x03,0x0D);
  //    radio.writeReg(0x04,0x05);
#ifdef IS_RFM69HW
  radio.setHighPower();
#endif
  radio.encrypt(null);
#ifdef ATC_RSSI
  Serial.println("-- RFM69_ATC Enabled (Auto Transmission Control)");
  radio.enableAutoPower(ATC_RSSI);
#endif
  Serial.print("-- Network Address: "); Serial.print(NETWORKID); Serial.print("."); Serial.println(NODEID);
  //	 Ping the datalogger. If it is alive, it will respond with a 1
//  while (!ping()) {
//    Serial.println("Failed to Setup ping");
//    //If datalogger doesn't respond, Blink, wait 60 seconds, and try again
//    radio.sleep();
//    Serial.flush();
//    Blink(100, 5);
//    Sleepy::loseSomeTime(1000);
//  }
  Serial.println("-- Datalogger Available, setup complete ");
}

void loop() {
  Blink(500, 1);
  //Gets current time at start of measurement cycle. Stores in global theTimeStamp struct
  bool gotTime = false;
  for (int i = 0; i < Retries; i++) {
    delay(1000);
    Serial.print("Timestamp request: "); Serial.println(i);
    gotTime = getTime();
    if (gotTime) {
      i = Retries;
      Serial.print(" [RX:"); Serial.print(radio.RSSI); Serial.print("]");
    }
  }
  if (!gotTime) { 
    Serial.println("time - No Response from Datalogger");
    }
  bool gotPing = false;
  for (int i = 0; i < Retries; i++) {
    delay(1000);
    Serial.print("Ping request: "); Serial.println(i);
    gotPing = ping();
    if (gotPing) i = Retries;
  }
  delay(100);
  if (gotPing) {
    //If the Datalogger is listening and available to recieve data
    Serial.print("- Datalogger Available ");
  } else {
    Serial.print("- ping fail");
  }
  Serial.flush();
  radio.sleep();
  delay(3000);
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
  byte sigStr;
  //digitalWrite(LED, HIGH); //turn on LED to signal tranmission event
  //Send request for time to the Datalogger
  if (!HANDSHAKE_SENT) {
    Serial.print("time - ");
    if (radio.sendWithRetry(GATEWAYID, "t", 1,  ACK_RETRIES, ACK_WAIT_TIME)) { //'t' requests time
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
  while (!TIME_RECIEVED && HANDSHAKE_SENT) {
    if (radio.receiveDone()) {
      if (radio.DATALEN == sizeof(theTimeStamp)) { //check to make sure it's the right size
        theTimeStamp = *(TimeStamp*)radio.DATA; //save data to global variable
        Serial.print(" rcv - "); Serial.print('['); Serial.print(radio.SENDERID); Serial.print("] ");
        Serial.print(theTimeStamp.timestamp); Serial.print(" [RX_RSSI:"); Serial.print(radio.RSSI); Serial.print("]"); Serial.println();
        sigStr = 11 - byte(radio.RSSI * -0.1);
        Serial.print("   [sigStr:");Serial.print(sigStr);Serial.print("]");Serial.println();
        TIME_RECIEVED = true;
        digitalWrite(LED, LOW); //turn off LED
      }
      else {
        digitalWrite(LED, LOW); //turn off LED
        //Blink(200, 5);
        Serial.println("failed . . . received not timestamp");
        return false;
      }
      if (radio.ACKRequested()) {
        radio.sendACK();
        delay(10);
      }
    }
    if (millis() > timeout_start + timeout) { //it is possible that you could get the time and timeout, but only in the edge case
      Serial.println(" ...timestamp timeout");
      return false;
    }
  }
  Blink(400,sigStr);
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
  Serial.print("ping - ");
  if (radio.sendWithRetry(GATEWAYID, "p", 1, ACK_RETRIES, ACK_WAIT_TIME)) {
    Serial.println(" > p");
    return true;
  }
  else {
    Serial.println("failed: no ack");
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
