struct Payload {
  uint32_t time = 0;
  float batV = 0;
  float boardTemp = 0;
  float w1 = 0;
  float w2 = 0;
  float w3 = 0;
  float w4 = 0;
  float w5 = 0;
  float w6 = 0;
  float w7 = 0;
  float w8 = 0;
  float t1 = 0;
  float t2 = 0;
  float t3 = 0;
  float t4 = 0;
  float t5 = 0;
  float t6 = 0;
  float t7 = 0;
  float t8 = 0;
};
Payload thisPayload;

void setup() {
  //setup stuff
}

void loop() {
  //wake everything up

  setMuxAddress(); //set mux address
  thisPayload.boardTemp = readBoardTemp();
  thisPayload.batV = readBatV();
  //read from Lc Amp
  thisPayload.w1 = readLcAmp();
  thisPayload.t1 = readTemp();
  //etc etc
  //sleep all Muxes

  sendPayload();
  waitForAWhile();
}
