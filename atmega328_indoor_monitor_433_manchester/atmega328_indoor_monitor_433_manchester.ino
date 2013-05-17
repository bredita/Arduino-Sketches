// assign each pin of the shift register to a colour:
const int mailYellow=0;
const int mailRed =1;
const int mailBlue= 2;
const int garageYellow= 5;
const int garageRed =3;
const int garageBlue =4;
const int shedBlue = 6;
const int shedYellow = 7;

//Pin connected to ST_CP of 74HC595
int latchPin = 8;
//Pin connected to SH_CP of 74HC595
int clockPin = 12;
////Pin connected to DS of 74HC595
int dataPin = 11;
byte LEDstate = 0;

#include <MANCHESTER.h>
const int RF433PIN=7;
const int RF433BAUD=110;

typedef struct
{
  // 1 reading every 5 minutes
  // 12 readings per hour
  unsigned int readings[6];
  // Time of 12 readings per hour
  unsigned long times[6];
  // count of readings: 0-11 (init'd to -1)
  byte lastindex;
  // last msg num: 0-31 (init'd to -1)
  byte lastmsgnum;
  // number of missed transmissions
  byte errorcount;
  // number of retransmissions
  byte retranscount;
}
NodeData;

const int MAX_NODE_ID = 3;
NodeData nodes[MAX_NODE_ID];
// Receive buffers used for RF message reception
unsigned char databufA[5];
unsigned char databufB[5];
unsigned char* currentBuf = databufA;

// The configured time offset
unsigned long timeoffset = 0;

void setup() {
  // put your setup code here, to run once:
  //set pins to output so you can control the shift register
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);  
  pinMode(RF433PIN, INPUT);  
  Serial.begin(9600);
  Serial.println("Hello I'm indoor monitor");
  LEDWrite(mailYellow,1);
  LEDWrite(mailBlue,1);
  LEDWrite(mailRed, 1);
  LEDWrite(garageRed, 1);
  LEDWrite(garageYellow, 1);
  LEDWrite(garageBlue, 1);
  LEDWrite(shedBlue,HIGH);
  LEDWrite(shedYellow,HIGH);
  delay(1000);
  LEDWrite(mailYellow,0);
  LEDWrite(mailBlue,0);
  LEDWrite(mailRed, 0);
  LEDWrite(garageBlue, 0);
  LEDWrite(garageYellow, 0);
  LEDWrite(garageRed, 0);
  LEDWrite(shedBlue,LOW);
  LEDWrite(shedYellow,LOW);
  // Set digital RX pin
  delay(1);
  MANRX_SetRxPin(RF433PIN);
  delay(1);
  MANRX_SetupReceive();
  delay(1);
  // Prepare data structures
  for (int i = 0; i < MAX_NODE_ID; i++)
  {
    nodes[i].lastindex = 0;
    nodes[i].lastmsgnum = 255;
    nodes[i].errorcount = 0;
    nodes[i].retranscount = 0;

  }
  delay(1);
  MANRX_BeginReceiveBytes(5, currentBuf);
}


void loop() {
  // put your main code here, to run repeatedly: 
  listen();

}
void listen() {
  delay(1);
  if (MANRX_ReceiveComplete()) {
    unsigned char receivedSize;
    unsigned char* msgData;
    delay(1);
    MANRX_GetMessageBytes(&receivedSize, &msgData);

    // Prepare for next msg
    if (currentBuf == databufA)
    {
      currentBuf = databufB;
    }
    else
    {
      currentBuf = databufA;
    }
    delay(1);
    MANRX_BeginReceiveBytes(5, currentBuf);

    // Check if we have a valid message
    // We expect to receive the following
    // 5 bit node ID
    // 5 bit reading number
    // 6 bit unused
    // 16 bit data
    // 8 bit checksum

    // This is a total of 5 unsigned chars
    byte nodeID = (byte)((msgData[0] & (0b11111 << 3)) >> 3);
    byte thisMsgNum = (byte)(((msgData[0] & 0b111) << 2) |
      ((msgData[1] & 0b11000000) >> 6));
    byte sensorNum  = (byte)(((msgData[1] & 0b00111110) >> 1));

    unsigned char checksum = msgData[0] | msgData[1] |
      msgData[2] | msgData[3];    // Ignore data which fails the checksum
    if (checksum != msgData[4]) {
      Serial.write("Checksum mismatch\r\n");
      return;
    }
    // Ignore duplicates
    if (nodes[nodeID].lastmsgnum == thisMsgNum)
    {
      nodes[nodeID].retranscount++;
      Serial.write("Retransmission\r\n");

      return;
    }
    else
    {
      byte lastnum = nodes[nodeID].lastmsgnum;

      {
        byte expectednum = lastnum + 1;
        if (expectednum >= 31)
        {
          expectednum = 0;
        }

        if (expectednum != thisMsgNum)
        {
          nodes[nodeID].errorcount++;

        }
        else if (nodes[nodeID].retranscount < 2)
        {
          nodes[nodeID].errorcount++;

        }
      }
    }

    unsigned int reading = (msgData[2] << 8) | msgData[3];

    unsigned long time = (millis() / 1000) + timeoffset;

    char nodetxt[9];
    itoa((unsigned int)nodeID,nodetxt,10);
    char msgnumtxt[6];
    itoa((unsigned int)thisMsgNum,msgnumtxt,10);
    char sensornumtxt[3];
    itoa((unsigned int)sensorNum,sensornumtxt,10);
    char readingtxt[6];
    itoa((unsigned int)reading,readingtxt,10);
    char timetxt[12];
    itoa((unsigned long)time,timetxt,10);
    // Add data reading
    Serial.write("NodeID:");
    Serial.print(nodetxt);
    Serial.write(" Msg#:");
    Serial.print(msgnumtxt);
    Serial.write(" Sensor#:");
    Serial.print(sensornumtxt);
    Serial.write(" Reading:");
    Serial.print(readingtxt);
    Serial.write(" Time:");
    Serial.print(timetxt);
    Serial.write("\r\n");
    delay(1);
    if(nodeID==3) {             // shed
      if (sensorNum==1 ) {     // temp
        if (reading < 5 )             // lower than 5 degrees C
          LEDWrite(shedBlue,HIGH);
        else if (reading >= 5)        // 5 degrees or more
          LEDWrite(shedBlue,LOW);
      } 
      else if (sensorNum==2){  // light
        if (reading > 50)              // on
          LEDWrite(shedYellow,HIGH);
        else if (reading <= 50)        // off
          LEDWrite(shedYellow,LOW);
      }
    }
    else if (nodeID == 2) {     // doesn't exist yet
    }
    else if (nodeID == 1) {     // garage
      if (sensorNum==0) {        // voltage
        if (reading >= 2000)           // > 2V
          LEDWrite(garageRed,LOW);
        else if (reading > 300)       // less than 2V
          LEDWrite(garageRed,HIGH);
      }
      if (sensorNum==1) {       // door
        if (reading == 1)               //closed
          LEDWrite(garageBlue,LOW);
        else if ( reading == 0)         //open
          LEDWrite(garageBlue,HIGH);
      }
      if (sensorNum==2) {      // light
        if (reading == 1)             //off
          LEDWrite(garageYellow,LOW);
        else if (reading == 0)         // on
          LEDWrite(garageYellow,HIGH);
      }
    } 
    else if (nodeID == 0)  {   // mailbox
      LEDWrite(mailRed,HIGH);
    }
  }
}

void LEDWrite(int whichPin, int whichState) {

  // turn off the output so the pins don't light up
  // while you're shifting bits:
  digitalWrite(latchPin, LOW);

  // turn on the next highest bit in bitsToSend:
  bitWrite(LEDstate, whichPin, whichState);

  // shift the bits out:
  shiftOut(dataPin, clockPin, MSBFIRST, LEDstate);

  // turn on the output so the LEDs can light up:
  digitalWrite(latchPin, HIGH);

}

