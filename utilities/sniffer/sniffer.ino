/**

*/
#include "RFM69.h" //https://www.github.com/lowpowerlab/rfm69
#include "RFM69_ATC.h" //https://www.github.com/lowpowerlab/rfm69


/****************************************************************************/
/***********************    DON'T FORGET TO SET ME    ***********************/
/****************************************************************************/
#define NODEID    0 //Node Address
#define NETWORKID 100 //Network to communicate on
/****************************************************************************/

//#define GATEWAYID 0 //Address of datalogger/reciever
#define FREQUENCY RF69_433MHZ
#define IS_RFM69HW    //uncomment only for RFM69HW! 
//#define ATC_RSSI -70 //ideal signal strength
//#define ACK_WAIT_TIME 200 // # of ms to wait for an ack
//#define ACK_RETRIES 3 // # of attempts before giving up

#define LED 9

#ifdef ATC_RSSI
RFM69_ATC radio;
#else
RFM69 radio;
#endif//Declare Radio

void setup() {
  Serial.begin(115200); // Initialize the serial port
  pinMode(LED, OUTPUT); //led

  //Setup Radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  //set radio rate to 4.8k
  radio.writeReg(0x03, 0x1A);
  radio.writeReg(0x04, 0x0B);
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
}

void loop() {
  if (radio.receiveDone()) {
    Serial.print(" rcv - "); Serial.print('['); Serial.print(radio.SENDERID); Serial.print("] ");
    Serial.print(" [RX:"); Serial.print(radio.RSSI); Serial.print("]");
    Serial.println();
    //byte sigStr = 11 - byte(radio.RSSI * -0.1);
    //Serial.print(" [sigStr:"); Serial.print(sigStr); Serial.print("]"); Serial.println();
    //Blink(400,sigStr);
    Serial.flush();
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
