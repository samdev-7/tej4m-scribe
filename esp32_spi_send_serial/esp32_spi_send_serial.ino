/* Program used to send SPI bytes through serial.
 *
 * by Sam Liu
 */

#include <SPI.h>

static const int spiClk = 1000000; // clock speed set to 1 MHz

SPIClass *vspi;
byte msg[2] = {0b00000000, 0b00000000};

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

  vspi = new SPIClass(VSPI);
  vspi->begin(SCK, MISO, MOSI, SS);
  pinMode(vspi->pinSS(), OUTPUT);
  digitalWrite(vspi->pinSS(), HIGH);
}

void loop() {
  if (Serial.available() > 0) {
    bool valid = true;
    String str = Serial.readString();
    str.trim();
    if (str.length() != 16) {
      Serial.println("Invalid input length");
      valid = false;
    }
    for (int i = 0; i < 8 && valid; i++) {
      char c = str[i];
      if (c == '0') {
        msg[0] = msg[0] << 1;
      } else if (c == '1') {
        msg[0] = (msg[0] << 1) | 1;
      } else {
        Serial.println("Invalid character");
        valid = false;
        break;
      }
    }
    for (int i = 8; i < 16 && valid; i++) {
      char c = str[i];
      if (c == '0') {
        msg[1] = msg[1] << 1;
      } else if (c == '1') {
        msg[1] = msg[1] << 1 | 1;
      } else {
        Serial.println("Invalid character");
        valid = false;
        break;
      }
    }

    if (valid) {
      Serial.print("Byte 1: ");
      Serial.println(msg[0], BIN);
      Serial.print("Byte 2: ");
      Serial.println(msg[1], BIN);

      vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
      digitalWrite(vspi->pinSS(), LOW);
      vspi->transfer16(msg[0] << 8 | msg[1]);
      digitalWrite(vspi->pinSS(), HIGH);
      vspi->endTransaction();
    }
  }
}
