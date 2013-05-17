// Node 3. Garage transmitter. 
// Tells me if the door is open or the light is on.
//
// 
// Node 3, Sensor 1 = Temperature  ( 1 = more than 4C, 0 = less)
// Node 3, Sensor 2 = Light (1 = on, 0 = off)
//
// Most of the code is "stolen" from mchr3k: 
// https://github.com/mchr3k/arduino/ and I'm using his manchester library: 
// https://github.com/mchr3k/arduino-libs-manchester
// Big thank you for making this available.
// 


#include <MANCHESTER.h> // Radio protocol unsigned int lightReading = 0;


// 433 radio
const int TXpin=3;
const int repeatMessage=2;
const int NODE_ID = 3; 
float tempC=0;

void setup() {

  MANCHESTER.SetTxPin(TXpin);
  pinMode(PA1,INPUT);
  pinMode(PA6,INPUT);
  digitalWrite(PA1,LOW);
  digitalWrite(PA6,LOW);
  analogReference(INTERNAL);
}

void loop() {
//  int lightStatus = 0;

//  unsigned int tempStatus = 0;
  // put your main code here, to run repeatedly:




//  if ( lightReading > 50 ) lightStatus=1;
//  if ( tempReading > 5 ) tempStatus=1;


 //sendMsg(1,tempStatus);  // sensor #1
 sendMsg(1,(unsigned int)(analogRead(PA6)) * 110 / 1024);  // sensor #1
 sendMsg(2,(unsigned int)(analogRead(PA1))); // sensor #2

  delay(60000);
}

unsigned int readingNum = 0;

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
