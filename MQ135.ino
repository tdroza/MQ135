#include "MQ135.h"

#define ANALOG_PIN A0

MQ135 gasSensor = MQ135(A0);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

void loop() {
  
  Serial.println("/--- RZero ---/--- Raw ---/--- Resist ---/--- PPM ---/");
  float rzero = gasSensor.getRZero();
  float ppm = gasSensor.getPPM();
  int analogRaw = analogRead(A0);
  float resistance = gasSensor.getResistance();
  Serial.print("/   ");
  Serial.print(rzero);
  Serial.print("    /    ");
  Serial.print(analogRaw);
  Serial.print("     /    ");
  Serial.print(resistance);
  Serial.print("    /   ");
  Serial.print(ppm);
  Serial.println("  /");
  
  delay(1000);
}
