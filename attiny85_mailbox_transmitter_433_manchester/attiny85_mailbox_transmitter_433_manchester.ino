// Node 0. Mailbox transmitter. 
// Tells me if the lid is open.
//
// Node 0, sensor 0 = Mailbox transmitter battery voltage
// Node 0, Sensor 1 = Mailbox lid (1 = Closed, 0= Open)

//
// Most of the code is "stolen" from mchr3k: 
// https://github.com/mchr3k/arduino/ and I'm using his manchester library: 
// https://github.com/mchr3k/arduino-libs-manchester
// Big thank you for making this available.
// 


#include <MANCHESTER.h> // Radio protocol 

// For the sleep mode/watchdog
#include <ATTinyWatchdog.h>
#include <avr/power.h>
#include <avr/sleep.h> 
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

// 433 radio
const int TXpin=3;
const int repeatMessage=3;
const int NODE_ID = 0; // the garage is node 1 in my (soon to be) sensor network

// The TXpwr pin provides power to the optocoupler that switches on/off the radio
const int TXpwr=4;

// sensor pins
const int tiltSensor=0;

// for how long must the sensor reading be stable before it's accepted?
const int buttonstatedelay= 500;

void setup() {
  // Prepare for random pauses
  randomSeed(analogRead(0));
  // Setup watchdog to notify us every 4 seconds
  ATTINYWATCHDOG.setup(8);
  // Turn off subsystems which we aren't using
  power_timer0_disable();
  // timer1 used by MANCHESTER
  power_usi_disable();
  // ADC used for reading a sensor
  // ATTINYWATCHDOG turns off ADC before sleep and
  // restores it when we wake up
  
  // Select the 1.1V internal ref voltage makes sense for temp sensors, but I don't have one of them in the garage
  analogReference(INTERNAL);
  pinMode(tiltSensor,INPUT);
  pinMode(TXpwr,OUTPUT);
  pinMode(TXpin,OUTPUT);

  digitalWrite(tiltSensor,LOW);  //disable internal pull-up resistor

  sbi(GIMSK,PCIE); // Turn on Pin Change interrupt
  sbi(PCMSK,PCINT1); // Which pins are affected by the Pin Change interrupt   
  sbi(PCMSK,PCINT0); // Which pins are affected by the Pin Change interrupt  
  delay(1); // it feels safe to delay before any Manchester command...
  MANCHESTER.SetTxPin(TXpin);
  
  // flash the led to show that I'm alive
  digitalWrite(TXpwr,HIGH);
  delay(500);
  digitalWrite(TXpwr,LOW);
  delay(500);
  digitalWrite(TXpwr,HIGH);
  delay(500);
  digitalWrite(TXpwr,LOW);
  
  // MANCHESTER.SetTimeOut(1000); should be used according to the readme but manchester.h does not mention it...
}

void loop() {
    // neccessary for the voltage meter to work
  sendMsg(1,getLidStatus());  // sensor #1
  deepsleep(70);    // wait a while after sending. 
  analogRead(0);
  sendMsg(1,getLidStatus());  // sensor #1
  system_sleep();   // "Normal" deep sleep - will wake up only on interrupts (tilt pin/light pin)
}


unsigned int getLidStatus() {
  if(stableReading(tiltSensor)==HIGH) {
    sendMsg(0,getBandgap());     // sensor #0 
    return 1;
  } // if the tilt sensor is high, and stable, then the lid is open
  else { return 0; }; // otherwise I assume it's closed.
}


void sleeptx()
{
  pinMode(TXpin, INPUT); // saves some battery power, apparently
  digitalWrite(TXpwr,LOW); // switch off the radio and LED
  pinMode(TXpwr, INPUT);   // more power saving tricks I guess
}

void waketx()
{
  pinMode(TXpin, OUTPUT); // we need these to be outputs again
  pinMode(TXpwr, OUTPUT);
  
  // Power up the radio
  digitalWrite(TXpwr, HIGH);  
  //digitalWrite(TXpin, HIGH);
}

unsigned int readingNum = 0;

void sendMsg(unsigned int sensornum, unsigned int data)
{
  readingNum++;
  if (readingNum >= 65535) readingNum = 0;  // Isn't 65535 what you can store in an unsigned int?
  for(int c=1; c <= repeatMessage; c++) {   // repeat the message a few times
  deepsleep(random(1,2)); //I don't know why, but sleeping before the transmit makes msg #1 much more reliable.
  doSendMsg(data, sensornum, readingNum);
  }
}

void deepsleep(unsigned int multiple)
{
  sleeptx(); 
  // deep sleep for multiple * 4 seconds
  cbi(GIMSK,PCIE); // Turn on Pin Change interrupt
  ATTINYWATCHDOG.sleep(multiple);
  sbi(GIMSK,PCIE); // Turn on Pin Change interrupt
  sbi(PCMSK,PCINT1); // Which pins are affected by the Pin Change interrupt   
  sbi(PCMSK,PCINT0); // Which pins are affected by the Pin Change interrupt  
  waketx();
}

void doSendMsg(unsigned int xiData, unsigned int xiSensorNum, unsigned char xiMsgNum)
{
  // Send a message with the following format
  // 5 bit node ID 
  // 5 bit reading number 
  // 4 bit sensor number - I struggled a lot to add this. Seems to work now.
  // 2 bits unused 
  // 16 bit data
  // 8 bit checksum
  
  // This is a total of 5 unsigned chars
  unsigned char databuf[5];
  unsigned char nodeID = (NODE_ID & 0b11111);
  unsigned char msgNum = (xiMsgNum & 0b11111);
  unsigned char sensorNum = (xiSensorNum & 0b1111);
  
  databuf[0] = (nodeID << 3) | (msgNum >> 2);
 // databuf[1] = ((msgNum & 0b00000011) << 6 ) | (sensorNum >> 2);
  databuf[1] = (msgNum  << 6  | (sensorNum & 0b00001111) << 2);
  databuf[2] = ((0xFF00 & xiData) >> 8);
  databuf[3] = (0x00FF & xiData);
  databuf[4] = databuf[0] | databuf[1] | databuf[2] | databuf[3];
  
  MANCHESTER.TransmitBytes(5, databuf);
}


boolean stableReading (int pin) {
  boolean state;
  boolean previousState;
    previousState = digitalRead(pin);
  for (int counter=0;counter < buttonstatedelay; counter++) {
    delay(1);
    state=digitalRead(pin);
    if (state!=previousState) {
      counter=0;
      previousState=state;
    }
  }
  return state;
}

void system_sleep() {
  cbi(ADCSRA,ADEN); // Switch Analog to Digital converter OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode
  sleep_mode(); // System sleeps here
  sbi(ADCSRA,ADEN);  // Switch Analog to Digital converter ON
}
ISR(PCINT0_vect) { // Must have a subroutine to call when the interrupts trigger
}
const long InternalReferenceVoltage = 1080;  // Adjust this value to your board's specific internal BG voltage
//
//// Code courtesy of "Coding Badly" and "Retrolefty" from the Arduino forum
//// results are Vcc * 100
//// So for example, 5V would be 500.
int getBandgap () 
{
  // REFS0 : Selects AVcc external reference
  // MUX3 MUX2 MUX1 : Selects 1.1V (VBG)  
  ADMUX = _BV (REFS0) | _BV (MUX3) | _BV (MUX2) | _BV (MUX1);
  ADCSRA |= _BV( ADSC );  // start conversion
  while (ADCSRA & _BV (ADSC))
  { 
  }  // wait for conversion to complete
  int results = (((InternalReferenceVoltage * 1024) / ADC) + 5) / 10; 
  return results;
} // end of getBandgap

// Find internal 1.1 reference voltage on AREF pin. Must remember to do this some day.
//void setup ()
//{
//  analogReference (INTERNAL);
//  analogRead (A0);  // force voltage reference to be turned on
//}
//void loop () { }
//Then use a voltmeter to measure the voltage on the AREF pin of the processor. Multiply that by 1000 and use it as the InternalReferenceVoltage variable above. For example, I got 1.080 volts when I tried it.

