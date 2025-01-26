/* Program used to log all SPI bytes to serial.
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
    Serial.println(msg[0], BIN);
    Serial.println(msg[1], BIN);
  }
}

ISR (SPI_STC_vect) {
  msg[bytesReciev] = SPDR;
  bytesReciev++;
}
