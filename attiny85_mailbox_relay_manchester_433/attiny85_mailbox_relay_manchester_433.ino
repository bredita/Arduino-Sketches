#include <MANCHESTER.h>
#define TXpin 4          // digital pin the radio is connected to
#define RXpin 3    
#define repeatMessage 3  // How many times the 433 radio should send its message
#define NODE_ID 0        // The ID of this node
unsigned int readingNum = 0;

void setup() {
  pinMode(TXpin,OUTPUT);
  pinMode(RXpin,INPUT);
  MANCHESTER.SetTxPin(TXpin);
}

void loop() {
  if(RXpin==HIGH) {
    digitalWrite(1,HIGH);
    sendMsg(1,1);
  }
}

void sendMsg(unsigned int sensornum, unsigned int data)
{
  readingNum++;
  if (readingNum >= 65535) readingNum = 0;  // Isn't 65535 what you can store in an unsigned int?
  for(int c=1; c <= repeatMessage; c++) {   // repeat the message a few times
    doSendMsg(data, sensornum, readingNum);
  }
}
void doSendMsg(unsigned int xiData, unsigned int xiSensorNum, unsigned char xiMsgNum)
{
  // Send a message with the following format
  // 5 bit node ID 
  // 5 bit reading number 
  // 5 bit sensor number - I struggled a lot to add this. Seems to work now.
  // 1 bit unused 
  // 16 bit data
  // 8 bit checksum

    // This is a total of 5 unsigned chars
  unsigned char databuf[5];
  unsigned char nodeID = (NODE_ID & 0b11111);
  unsigned char msgNum = (xiMsgNum & 0b11111);
  unsigned char sensorNum = (xiSensorNum & 0b11111);

  databuf[0] = (nodeID << 3) | (msgNum >> 2);
  // databuf[1] = ((msgNum & 0b00000011) << 6 ) | (sensorNum >> 2);
  databuf[1] = (msgNum  << 6  | (sensorNum & 0b00011111) << 1);
  databuf[2] = ((0xFF00 & xiData) >> 8);
  databuf[3] = (0x00FF & xiData);
  databuf[4] = databuf[0] | databuf[1] | databuf[2] | databuf[3];

  MANCHESTER.TransmitBytes(5, databuf);
}



  
