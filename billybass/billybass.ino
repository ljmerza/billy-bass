/*
  Make a DC Motor Move to Sound.
   This example code is in the public domain.
   Created by Donald Bell, Maker Project Lab (2016).
   Based on Sound to Servo by Cenk Özdemir (2012) 
   and DCMotorTest by Adafruit
*/
// include the Adafruit motor shield library
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor *myMouth = AFMS.getMotor(1);
Adafruit_DCMotor *myHead = AFMS.getMotor(2);
int SoundInPin = A0;
unsigned long previousMillis = 0; // save last millis when motors were ran
const long interval = 3000; // how long to wait till head is released

void setup() {
  AFMS.begin();
  
  myMouth->setSpeed(0);
  myMouth->run(FORWARD);
  myMouth->run(RELEASE);

  myHead->setSpeed(254);
  myHead->run(FORWARD);
  myHead->run(RELEASE);
  
  pinMode(SoundInPin, INPUT);
  Serial.begin(9600);

}

// the loop routine runs over and over again forever:
void loop() {
  uint8_t i;

  int sensorValue = analogRead(SoundInPin); 
  sensorValue = map(sensorValue,0,512,0,180);

  // get current milis
  unsigned long currentMillis = millis();

  if (sensorValue > 30) {
    
    previousMillis = currentMillis;
    myHead->run(FORWARD);
    myMouth->run(FORWARD);
    
    for (i=140; i<255; i++) {
      myMouth->setSpeed(i);
    }

    myMouth->run(RELEASE);
  }

  // if too much time passed without voice then stop head
  if(currentMillis - previousMillis >= interval){
    myHead->run(RELEASE);
  }
}

