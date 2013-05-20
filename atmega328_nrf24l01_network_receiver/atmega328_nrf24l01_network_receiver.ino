/*
 Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Simplest possible example of using RF24Network,
 *
 * RECEIVER NODE
 * Listens for messages from the transmitter and prints them out.
 */

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// nRF24L01(+) radio attached using Getting Started board 
RF24 radio(9,10);

// Network uses that radio
RF24Network network(radio);

// Address of our node
const uint16_t this_node = 0;

// Address of the other node
const uint16_t other_node = 1;

// Structure of our payload
struct payload_t
{
  unsigned long ms;
  unsigned long counter;
};

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
const int MAX_NODE_ID = 10;
NodeData nodes[MAX_NODE_ID];

void setup(void)
{
  Serial.begin(9600);
  Serial.println("RF24Network/examples/helloworld_rx/");
 
  SPI.begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  network.begin(/*channel*/ 88, /*node address*/ this_node);
  for (int i = 0; i < MAX_NODE_ID; i++)
  {
    nodes[i].lastindex = 0;
    nodes[i].lastmsgnum = 255;
    nodes[i].errorcount = 0;
    nodes[i].retranscount = 0;

  }}

void loop(void)
{
  // Pump the network regularly
  network.update();

  // Is there anything ready for us?
  while ( network.available() )
  {
    // If so, grab it and print it out
    RF24NetworkHeader header;
    unsigned char msgData[5];
    network.read(header,&msgData,5);
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

    unsigned long time = (millis() / 1000) ;

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
// vim:ai:cin:sts=2 sw=2 ft=cpp
