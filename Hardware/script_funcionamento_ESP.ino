#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("HIDRA - ESP32 inicializado!");
}

void loop() {
  Serial.println("ESP32 funcionando corretamente.");
  delay(1000);
}
