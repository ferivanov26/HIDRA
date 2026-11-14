
#include <Arduino.h>
#include "HX711.h"

const uint8_t PINO_DT = 27;
const uint8_t PINO_SCK = 14;

const float MASSA_REFERENCIA_G = 220.0f;
const unsigned long INTERVALO_LEITURA_MS = 1000;

HX711 balanca;

float fatorCalibracao = 1.0f;
bool calibrada = false;
unsigned long ultimaLeitura = 0;

void realizarTara() {
  if (!balanca.is_ready()) {
    Serial.println("ERRO: HX711 indisponivel.");
    return;
  }

  balanca.tare(10);
  Serial.println("Tara concluida.");
}

void calibrarBalanca() {
  if (!balanca.is_ready()) {
    Serial.println("ERRO: HX711 indisponivel.");
    return;
  }

  // Media das contagens do ADC, descontada a tara
  float leituraLiquida = balanca.get_value(10);

  if (abs(leituraLiquida) < 100.0f) {
    Serial.println("ERRO: carga insuficiente.");
    return;
  }

  fatorCalibracao = leituraLiquida / MASSA_REFERENCIA_G;
  balanca.set_scale(fatorCalibracao);
  calibrada = true;

  Serial.print("Fator de calibracao: ");
  Serial.println(fatorCalibracao, 6);
  Serial.println("Calibracao concluida!");
}

void setup() {
  Serial.begin(115200);
  balanca.begin(PINO_DT, PINO_SCK);

  Serial.println("HIDRA - Calibracao HX711");
  Serial.println("Deixe a plataforma vazia.");

  delay(3000);

  if (!balanca.wait_ready_timeout(3000)) {
    Serial.println("ERRO: HX711 nao respondeu.");
    return;
  }

  realizarTara();

  Serial.println("Coloque a lata de 220 g.");
  Serial.println("Digite C para calibrar.");
  Serial.println("Digite T para refazer a tara.");
}

void loop() {
  if (Serial.available()) {
    char comando = Serial.read();

    if (comando == 'c' || comando == 'C') {
      calibrarBalanca();
    }

    if (comando == 't' || comando == 'T') {
      realizarTara();
    }
  }

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();

    if (balanca.is_ready() && calibrada) {
      float massaGramas = balanca.get_units(5);

      Serial.print("Massa: ");
      Serial.print(massaGramas, 2);
      Serial.println(" g");
    }
  }
}
