#include <MANCHESTER.h>

#include <LiquidCrystal.h>
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

#include <SPI.h>
#include <RF24.h>
#include "printf.h"
RF24 radio(14,15);

const uint64_t pipes[2] = { 
  0xF0F0F0F0D2LL, 0xF0F0F0F0E1LL };


int currentline=0;
char line[21];
char line0[21];
char line1[21];
char line2[21];
char line3[21];


typedef struct
{
  // 1 reading every 5 minutes
  // 12 readings per hour
  unsigned int readings[2];
  // Time of 12 readings per hour
  unsigned long times[2];
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

// Per node data
const int MAX_NODE_ID = 5;
NodeData nodes[MAX_NODE_ID];


// Receive buffers used for RF message reception
unsigned char databufA[5];
unsigned char databufB[5];
unsigned char* currentBuf = databufA;

// The configured time offset
unsigned long timeoffset = 0;
unsigned int readingNum = 0;

void setup() {
  Serial.begin(57600);
  printf_begin();
  // set up the LCD's number of columns and rows: 
  lcd.begin(20, 4);
  //Set digital RX pin
  delay(1);
  MANRX_SetRxPin(8);
  delay(1);
  //

  MANRX_SetupReceive();
  delay(1);
  MANRX_BeginReceiveBytes(5, currentBuf);
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(5);
  radio.setRetries(15,15);
  radio.setChannel(24); // channel 24 = my house number and is also
  // one of the less polluted channels I found :)
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(true);
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);
  radio.printDetails();
  radio.startListening();

  // Prepare data structures
  for (int i = 0; i < MAX_NODE_ID; i++)
  {
    nodes[i].lastindex = 0;
    nodes[i].lastmsgnum = 255;
    nodes[i].errorcount = 0;
    nodes[i].retranscount = 0;

  }
  Serial.write("Hi, I'm bredos new indoor monitor\r\n");
  strcpy(line,"Indoor monitor.");
  printline();
  strcpy(line,"Indoor monitor1.");
  printline();
  strcpy(line,"Indoor monitor2.");
  printline();
  strcpy(line,"Indoor monitor3.");
  printline();
  strcpy(line,"Indoor monitor4.");
  printline();
  strcpy(line,"Indoor monitor5.");
  printline();
  delay(1);
}

void loop() {
  // put your main code here, to run repeatedly: 
  listen433();
  listen24();
  if (digitalRead(9 )==HIGH) {
    radio.stopListening();
    Serial.println("button pushed. sending cmd...");
    while (!sendMsg(24,1000)) {
      Serial.println("Sending a button press failed. Retrying...\n\r");
    } 
    radio.startListening();

    Serial.println("Sending button press succeeded\n\r");
    strcpy(line,"Door triggered.");
    printline();
  }
}

void listen24() {
  // if there is data ready
  if ( radio.available() )
  {
    Serial.println("received something 24");
    // Dump the payloads until we've gotten everything
    unsigned char msgData[5] ;
    bool done = false;
    while (!done)
    {
      // Fetch the payload, and see if this was the last one.
      done = radio.read( &msgData, sizeof(msgData) );

      //printf("Got message %lu\n\r", got_message);
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
      Serial.write("NodeID24:");
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

      if (int(nodeID)==4) {

        if (int(sensorNum)==1) {
          if (int(reading) >= 100) {
            strcpy(line,"Garage24 light on.");
          }
          else {
            strcpy(line,"Garage24 light off.");
          }
        } 
        else if (int(sensorNum)==2) {
          strcpy(line,"Garage24 temp ");
          strcat(line,readingtxt);
          strcat(line,"C.");
        } 
        else if (int(sensorNum)==3) {
          if (int(reading) <= 50 && int(reading)>0) {
            strcpy(line,"Garage24 door open.");
          } 
          else { 
            strcpy(line,"Garage24 door shut.");
            printline();
            if (int(reading) <= 150 && int(reading)>0)
              strcpy(line,"Car in garage.");
            else
              strcpy(line,"Car not in garage.");
          }

        }
      }
      printline();
    }
  }
}

void listen433() {
  delay(1);
  if (MANRX_ReceiveComplete()) {
    Serial.println("received something 433");
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
    if (nodeID==0) {
      strcpy(line,"Mailbox opened.");
    } 
    if ((unsigned int)nodeID==1) {
      if((unsigned int)sensorNum==0) {
        if (reading > 1800) 
          strcpy(line,"Garage battery OK.  ");
        else 
          strcpy(line,"Garage battery low. ");
      } 
      else if ((unsigned int)sensorNum==1) {
        if (reading==1) { 
          strcpy(line,"Garage door closed. ");
        }
        else {  
          strcpy(line,"Garage door open.   ");
        }
      } 
      else if ((unsigned int)sensorNum==2) {
        if (reading ==1) 
          strcpy(line,"Garage light off.   ");
        else
          strcpy(line,"Garage light on.    ");
      }
    } 
    if ((unsigned int)nodeID==3) {
      if ((unsigned int)sensorNum==1) {
        if (reading < 3) 
          strcpy(line,"Shed temp low.      ");
        else
          strcpy(line,"Shed temp ");
        strcat(line,readingtxt);
        strcat(line,"C.      ");
      }
      if ((unsigned int)sensorNum==2) {
        if (reading > 30) 
          strcpy(line,"Shed light on.      ");
        else
          strcpy(line,"Shed light off.     ");
      }
    }
    printline();
  }
}



// start a new line

void printline() {
  if (currentline >=3) {
    if (strcmp(line,line3)==0) {        // line 4 matches this one. do nothing. 
    }      
    else if (strcmp(line,line2)==0) {   // line 3 ,matches this one
      strcpy(line2,line3);              // move line 4 up to line 3
      strcpy(line3,line);               // print this one on line 4
    } 
    else if (strcmp(line,line1)==0) {   // line 2 matches this one
      strcpy(line1,line2);              // move line 3 up to line 2
      strcpy(line2,line3);              // move line 4 up to line 3
      strcpy(line3,line);               // print this one on line 4
    } 
    else {  
      strcpy(line0,line1);               // move line 2 up to line 1
      strcpy(line1,line2);               // move line 3 up to line 2
      strcpy(line2,line3);               // move line 4 up to line 3
      strcpy(line3,line);                // print this one on line 4
    } 
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(line0);    
    lcd.setCursor(0,1);
    lcd.print(line1);    
    lcd.setCursor(0,2);
    lcd.print(line2);    
    lcd.setCursor(0,3);
    lcd.print(line3);    

  }
  else {
    lcd.setCursor(0,currentline);
    lcd.print(line);
    currentline++;
  }

}

bool doSendMsg(unsigned int xiData, unsigned int xiSensorNum, unsigned char xiMsgNum)
{
  // Send a message with the following format
  // 5 bit node ID 
  // 5 bit reading number 
  // 5 bit sensor number - I struggled a lot to add this. Seems to work now.
  // 1 bit unused 
  // 16 bit data
  // 8 bit checksum
  int NODE_ID=5;
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

  printf("Now sending message\r\n");
  bool ok = radio.write( &databuf, sizeof(databuf) );
  return ok;
}


bool sendMsg(unsigned int sensornum, unsigned int data)
{
  readingNum++;
  if (readingNum >= 65535) readingNum = 0;  // Isn't 65535 what you can store in an unsigned int?

  return(doSendMsg(data, sensornum, readingNum));
}




















