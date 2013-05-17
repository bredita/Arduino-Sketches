#include <MANCHESTER.h>
#include <TinyDebugSerial.h>

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
const int MAX_NODE_ID = 3;
NodeData nodes[MAX_NODE_ID];


// Receive buffers used for RF message reception
unsigned char databufA[5];
unsigned char databufB[5];
unsigned char* currentBuf = databufA;

// The configured time offset
unsigned long timeoffset = 0;

void setup() {
// Set digital RX pin
 delay(1);
 MANRX_SetRxPin(4);
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
 Serial.begin(9600);
 Serial.write("Hi, I'm bredos attiny85 receiver\r\n");
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
  }
}

