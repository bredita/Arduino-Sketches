/*
 Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Simplest possible example of using RF24Network 
 *
 * TRANSMITTER NODE
 * Every 2 seconds, send a payload to the receiver node.
 */

#define lightSensor A5
#define tempSensor A4
#define relayPin 8
#define trigPin 7
#define echoPin 6

#define NODE_ID 4

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// nRF24L01(+) radio attached using Getting Started board 
RF24 radio(9,10);

// Network uses that radio
RF24Network network(radio);

// Address of our node
const uint16_t this_node = 1;

// Address of the other node
const uint16_t other_node = 0;
RF24NetworkHeader header(/*to node*/ other_node);

// How often to send 'hello world to the other unit
const unsigned long interval = 2000; //ms

// When did we last send?
unsigned long last_sent;

// How many have we sent already
unsigned long packets_sent;


void setup(void)
{
  Serial.begin(9600);
  Serial.println("RF24Network/examples/helloworld_tx/");
  pinMode(A5,INPUT);
  digitalWrite(A5,LOW);
   pinMode(A4,INPUT);
   digitalWrite(A0,HIGH);
   delay(3000);
    digitalWrite(A0,LOW);
  
 SPI.begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);

  network.begin(/*channel*/ 88, /*node address*/ this_node);
}

void loop(void)
{
  // Pump the network regularly
  network.update();
  
  // If it's time to send a message, send it!
  unsigned long now = millis();
  if ( now - last_sent >= interval  )
  {
    last_sent = now;

//    Serial.print("Sending...");
//    payload_t payload = { millis(), packets_sent++ };
//    RF24NetworkHeader header(/*to node*/ other_node);
//    bool ok = network.write(header,&payload,sizeof(payload));
//    if (ok)
//      Serial.println("ok.");
//    else
//      Serial.println("failed.");
    Serial.print( "Sensormsg sent:" );
  
    Serial.println (sendMsg(1,analogRead(A5)));
    Serial.println (sendMsg(2,gettemp()));
    Serial.println (sendMsg(3,getdistance()));
  }


}

unsigned int readingNum = 0;
bool sendMsg(unsigned int sensornum, int data)
{
  readingNum++;
  if (readingNum >= 65535) readingNum = 0;  // Isn't 65535 what you can store in an unsigned int?
  if (doSendMsg(data, sensornum, readingNum)) return true;
  else return false;
}

int getdistance() {
  // establish variables for duration of the ping, 
  // and the distance result in inches and centimeters:
  long duration, inches, cm;
 
  // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
 
  // Read the signal from the sensor: a HIGH pulse whose
  // duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  pinMode(echoPin, INPUT);
  duration = pulseIn(echoPin, HIGH);
 
  // convert the time into a distance
  inches = microsecondsToInches(duration);
  cm = microsecondsToCentimeters(duration);
  
  Serial.print(inches);
  Serial.print("in, ");
  Serial.print(cm);
  Serial.print("cm");
  Serial.println();
  return cm;
  delay(100);
}
 
long microsecondsToInches(long microseconds)
{
  // According to Parallax's datasheet for the PING))), there are
  // 73.746 microseconds per inch (i.e. sound travels at 1130 feet per
  // second).  This gives the distance travelled by the ping, outbound
  // and return, so we divide by 2 to get the distance of the obstacle.
  // See: http://www.parallax.com/dl/docs/prod/acc/28015-PING-v1.3.pdf
  return microseconds / 74 / 2;
}
 
long microsecondsToCentimeters(long microseconds)
{
  // The speed of sound is 340 m/s or 29 microseconds per centimeter.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds / 29 / 2;
}

int gettemp() {
  int inVal = analogRead(tempSensor);                // read the value from the sensor
  inVal /= 2;                               // convert 1024 steps into value referenced to +5 volts
  Serial.print(inVal);                      // print input value
  Serial.print(" Celsius,  ");              // print Celsius label
  Serial.print((inVal * 9)/ 5 + 32);        // convert Celsius to Fahrenheit
  Serial.println(" Fahrenheit");            // print Fahrenheit label

  return inVal;
}

bool doSendMsg(int xiData, unsigned int xiSensorNum, unsigned char xiMsgNum)
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

  
  if (network.write(header,&databuf,5)) return true;
  else return false;
}

