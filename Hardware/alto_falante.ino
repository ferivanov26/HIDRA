
#include <Arduino.h>

constexpr uint8_t MP3_RX = 25;
constexpr uint8_t MP3_TX = 26;

HardwareSerial mp3(2);

// Envia um comando pelo protocolo serial do modulo
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

  Serial.println("HIDRA - Teste de audio");

  // Aguarda inicializacao e leitura do microSD
  delay(3000);

  // Volume minimo (escala de 0 a 30)
  enviarComando(0x06, 30);
  delay(1000);

  // Reproduz o primeiro arquivo MP3
  Serial.println("Solicitando reproducao...");
  enviarComando(0x03, 1);
}

void loop() {
  // Exibe eventuais respostas do modulo
  while (mp3.available()) {
    Serial.printf("MP3 RX: %02X\n", mp3.read());
  }
}
