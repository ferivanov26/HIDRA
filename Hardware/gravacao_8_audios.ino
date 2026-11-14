
#include <Arduino.h>

constexpr uint8_t MP3_RX = 25;
constexpr uint8_t MP3_TX = 26;

HardwareSerial mp3(2);

void enviarComando(uint8_t comando, uint16_t parametro) {
  uint8_t pacote[10] = {
    0x7E, 0xFF, 0x06, comando, 0x00,
    (uint8_t)(parametro >> 8),
    (uint8_t)(parametro & 0xFF),
    0x00, 0x00, 0xEF
  };

  uint16_t soma = 0;
  for (int i = 1; i <= 6; i++) {
    soma += pacote[i];
  }

  uint16_t checksum = 0 - soma;
  pacote[7] = checksum >> 8;
  pacote[8] = checksum & 0xFF;

  mp3.write(pacote, 10);
  mp3.flush();
}

void setup() {
  Serial.begin(115200);
  mp3.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);

  Serial.println("HIDRA - Teste das 8 instrucoes");

  delay(3000);

  // Volume reduzido para o alto-falante de 0,5 W
  enviarComando(0x06, 30);

  Serial.println("Digite um numero de 1 a 8.");
}

void loop() {
  if (Serial.available()) {
    char tecla = Serial.read();

    if (tecla >= '1' && tecla <= '8') {
      uint8_t faixa = tecla - '0';

      Serial.printf("Reproduzindo faixa %u\n", faixa);

      // Reproduz arquivo da pasta /mp3
      enviarComando(0x12, faixa);
    }
  }

  // Diagnostico da comunicacao com o modulo
  while (mp3.available()) {
    Serial.printf("MP3 RX: %02X\n", mp3.read());
  }
}
