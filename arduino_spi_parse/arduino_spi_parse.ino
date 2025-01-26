/* Program used to parse SPI bytes on Arduino.
 *
 * by Sam Liu
 */

#include <SPI.h>

SPIClass *vspi;
byte msg[2] = {0b00000000, 0b00000000};
int bytesReciev = 0;

void setup() {
  Serial.begin(115200);
  Serial.print("MOSI: ");
  Serial.println(MOSI);
  Serial.print("MISO: ");
  Serial.println(MISO);
  Serial.print("SCK: ");
  Serial.println(SCK);
  Serial.print("SS: ");
  Serial.println(SS);

  SPCR |= _BV(SPE); // set to slave mode
  SPI.attachInterrupt();
}

void loop() {
  if (bytesReciev == 2){
    bytesReciev = 0;
    Serial.print("Byte 1: ");
    Serial.println(msg[0], BIN);
    Serial.print("Byte 2: ");
    Serial.println(msg[1], BIN);

    if (msg[0] >> 6 == 0b00) {
      Serial.print("Target X position set to: ");
      short pos = (msg[0] & 0b00111111) << 8 | msg[1];
      Serial.println(pos);
    } else if (msg[0] >> 6 == 0b01) {
      Serial.print("Target Y position set to: ");
      short pos = (msg[0] & 0b00111111) << 8 | msg[1];
      Serial.println(pos);
    } else if (msg[0] >> 5 == 0b100) {
      Serial.println("Moving to target position");
    } else if (msg[0] >> 5 == 0b101) {
      Serial.println("Setting current position to target");
    } else if (msg[0] >> 6 == 0b11) {
      if (msg[0] >> 3 == 0b11111) {
        Serial.println("Lifting all tools");
      } else {
        Serial.print("Switching to tool: ");
        short id = (msg[0] & 0b00111000 ) >> 3;
        Serial.println(id+1);
      }
    } else {
      Serial.println("Invalid command");
    }
  }
}

ISR (SPI_STC_vect) {
  msg[bytesReciev] = SPDR;
  bytesReciev++;
}
