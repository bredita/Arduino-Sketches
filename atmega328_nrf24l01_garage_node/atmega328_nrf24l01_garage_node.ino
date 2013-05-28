/*
 Garage node version twelvtythree
 This one can both send and receive.
 It has a relay which will be used to trigger the garage door motor.
 It sends the status of the sensors to the raspberry pi every 30 seconds.
 
 The NRF24L01 radio is connected like so:
 Pin 1 (GND/black)  -> GND
 Pin 2 (VCC/red)    -> 3V (if you feed it 5v it powers off and you have to 
 power cycle the board to bring it back)
 Pin 3 (CE/orange)   -> PB1/D9 (can be changed)
 Pin 4 (CSN/green)  -> PB2/D10 (can be changed)
 Pin 5 (SCK/yellow) -> SCK/PB5/D13 (fixed)
 Pin 6 (MOSI/blue)  -> MOSI/PB3/D11 (fixed)
 Pin 7 (MISO/brown) -> MISO/PB4/D12 (fixed)
 
 */
#define CEpin 9        // CE pin of the radio
#define CSNpin 10      // CSN pin of the radio

#define lightSensor A5 // the pin the light sensor (and the resistor 
// to ground) is connected to. The other leg 
// of the sensor goes to ground.
#define tempSensor A4  // The middle pin of the temp sensor (and the 
// 18K resistor) goes here. 
// the other two legs go to 5V/GND
#define relayPin 8     // The relay "IN1" pin goes here.
// The relay´s VCC and GND go to 5v and GND
#define trigPin 7      // The ping sensors trig pin
#define echoPin 6      // the ping sensors echo pin
// the other two pins on the ping sensor 
// go to 5v/GND

#define NODE_ID 4      // The garage was #1 but I´l give it a new ID 
// in order to be able to run both the new and 
// old box at the same time, for a while.

#include <RF24.h>      // Get it from: https://github.com/stanleyseow/RF24
#include <SPI.h>
#include "printf.h"    // needed for the printDetails function. Copy it to 
// your sketch folder.


RF24 radio(CEpin,CSNpin); // nRF24L01(+) radio 


//Must use same pipes on both nodes, one of them should have them swapped around)
const uint64_t pipes[2] = { 
  0xF0F0F0F0D2LL, 0xF0F0F0F0E1LL };

// How often to send sensor values
const unsigned long interval = 30000; //ms

// When did we last send?
unsigned long last_sent;

// How many have we sent already
unsigned long packets_sent;

// The configured time offset Do I need this for anything?
unsigned long timeoffset = 0;

unsigned long lastTrigger=0;
//using the same 5-byte message as on the 433 radios
typedef struct
{
  unsigned int readings[2];
  unsigned long times[2];
  byte lastindex;
  byte lastmsgnum;
  byte errorcount;
  byte retranscount;
}
NodeData;

// Per node data
const int MAX_NODE_ID = 31;
NodeData nodes[MAX_NODE_ID];

void setup(void)
{
  digitalWrite(relayPin,HIGH);
  Serial.begin(9600);
  printf_begin();
  printf("Hello, I´m bredos new garage node");
  pinMode(lightSensor,INPUT);
  digitalWrite(lightSensor,LOW);
  pinMode(tempSensor,INPUT);
  pinMode(relayPin,OUTPUT);
  digitalWrite(A0,HIGH); // The non-functioning test LED ;)
  delay(500);
  digitalWrite(A0,LOW);

  SPI.begin();
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(5);
  radio.setRetries(15,15);
  radio.setChannel(0x18); // channel 24 = my house number and is also
  // one of the less polluted channels I found :)
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(true);
  radio.openWritingPipe(pipes[1]);   // first pipe for writing,
  radio.openReadingPipe(1,pipes[0]); // second for reading
  radio.powerUp() ;
  radio.printDetails(); // If you get all zeroes or ff`s check your radio wiring 
  radio.startListening();
  for (int i = 0; i < MAX_NODE_ID; i++)
  {
    nodes[i].lastindex = 0;
    nodes[i].lastmsgnum = 255;
    nodes[i].errorcount = 0;
    nodes[i].retranscount = 0;

  }    
}

void loop(void)
{

  // If it's time to send the sensor readings, send it!
  unsigned long now = millis();
  if ( now - last_sent >= interval  )
  {
    last_sent = now;

    radio.stopListening(); //can`t speak and listen at the same time
    radio.powerUp();
    unsigned int lightReading=analogRead(lightSensor);
    unsigned int tempReading=gettemp();
    unsigned int distanceReading=getdistance();
    if (sendMsg(1,lightReading))
      Serial.println("Sending lightReading failed.");
    else
      printf("Sent lightReading: %i \r\n",lightReading);

    if (sendMsg(2,tempReading))
      Serial.println("Sending tempReading failed.");
    else
      printf("Sent tempReading: %i \r\n",tempReading);

    if (sendMsg(3,distanceReading))
      Serial.println("Sending distanceReading failed.");
    else
      printf("Sent distanceReading: %i \r\n",distanceReading);

    radio.startListening(); // better start listening again
  }

  if ( radio.available() )
  {
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

      if (nodeID==5 && sensorNum==24  && (millis() - lastTrigger) > 10000) {
        digitalWrite(relayPin,LOW);
        delay(reading);
        digitalWrite(relayPin,HIGH);
        Serial.print("Triggered relay for ");
        Serial.print(reading);
        Serial.println("ms.");
        lastTrigger=millis();
      }
    }

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

unsigned int getdistance() {
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

  //  Serial.print(inches);
  //  Serial.print("in, ");
  //  Serial.print(cm);
  //  Serial.print("cm");
  //  Serial.println();
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

unsigned int gettemp() {
  unsigned int inVal = analogRead(tempSensor);                // read the value from the sensor
  inVal /= 2;                               // convert 1024 steps into value referenced to +5 volts
  //    Serial.print(inVal);                      // print input value
  //    Serial.print(" Celsius,  ");              // print Celsius label
  //  Serial.print((inVal * 9)/ 5 + 32);        // convert Celsius to Fahrenheit
  //  Serial.println(" Fahrenheit");            // print Fahrenheit label

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


  // printf("Now sending message\r\n");
  bool ok = radio.write( &databuf, sizeof(databuf) );
  return ok;

}





