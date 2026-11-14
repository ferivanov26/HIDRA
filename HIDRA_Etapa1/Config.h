#pragma once
#include <Arduino.h>

namespace HidraConfig {
constexpr uint8_t PINO_BOTAO_INICIO = 18;     // LOW = pressionado (INPUT_PULLUP)
constexpr uint8_t PINO_COPO_INICIAL = 19;    // KW11-7 C/NO; LOW = chave acionada
constexpr uint8_t PINO_HX711_DOUT = 27;
constexpr uint8_t PINO_HX711_SCK = 14;
constexpr uint8_t PINO_MP3_RX_ESP32 = 25;    // ESP32 recebe do TX do MP3
constexpr uint8_t PINO_MP3_TX_ESP32 = 26;    // ESP32 envia ao RX do MP3
constexpr uint8_t PINO_LED_VERMELHO = 23;

constexpr uint32_t SERIAL_DIAGNOSTICO_BAUD = 115200;
constexpr uint32_t MP3_BAUD = 9600;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t INTERVALO_MASSA_MS = 200;
constexpr uint32_t TIMEOUT_MASSA_MS = 1500;
constexpr uint32_t ESPERA_INICIAL_HX711_MS = 3000;

// Fator experimental determinado usando massa de referencia de 220 g.
// Unidade: contagens ADC / grama. Exige tara com a plataforma vazia.
constexpr float FATOR_HX711_CONTAGENS_POR_GRAMA = 425.631805f;
constexpr uint8_t AMOSTRAS_TARA = 10;
constexpr float FILTRO_MASSA_ALPHA = 0.35f; // 1 = sem filtro; 0 = sem atualizacao

constexpr uint8_t VOLUME_MP3 = 30; // Escala 0..30; limitar devido ao alto-falante 0,5 W
constexpr uint8_t PRIMEIRA_FAIXA_MP3 = 1;
constexpr uint8_t ULTIMA_FAIXA_MP3 = 8;
}
