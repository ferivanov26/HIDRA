#include "HidraHardware.h"
#include "Config.h"
#include <HX711.h>

namespace {
struct EntradaComDebounce {
  uint8_t gpio;
  bool bruto;
  bool estavel;
  uint32_t desde_mudanca_ms;
};

EntradaComDebounce botao = {HidraConfig::PINO_BOTAO_INICIO, HIGH, HIGH, 0};
EntradaComDebounce chave = {HidraConfig::PINO_COPO_INICIAL, HIGH, HIGH, 0};
HX711 balanca;
HardwareSerial serialMp3(2);
Hidra::LeituraHardware leitura = {};
Hidra::ReceptorEvento receptor = nullptr;
uint32_t ultima_tentativa_massa_ms = 0;
uint32_t inicio_monitoramento_massa_ms = 0;
bool massa_falha_reportada = false;
bool filtro_inicializado = false;

void publicar(Hidra::TipoEventoFisico tipo, uint32_t instante,
              float massa_g = 0.0f, bool valida = false) {
  if (receptor != nullptr) {
    const Hidra::EventoFisico evento = {tipo, instante, massa_g, valida};
    receptor(evento);
  }
}

void prepararEntrada(EntradaComDebounce &entrada) {
  pinMode(entrada.gpio, INPUT_PULLUP);
  entrada.bruto = digitalRead(entrada.gpio);
  entrada.estavel = entrada.bruto;
  entrada.desde_mudanca_ms = millis();
}

// Retorna true somente quando ha transicao eletrica estavel.
bool detectarTransicao(EntradaComDebounce &entrada, uint32_t agora) {
  const bool novo_bruto = digitalRead(entrada.gpio);
  if (novo_bruto != entrada.bruto) {
    entrada.bruto = novo_bruto;
    entrada.desde_mudanca_ms = agora;
  }
  if (entrada.estavel != entrada.bruto &&
      (uint32_t)(agora - entrada.desde_mudanca_ms) >= HidraConfig::DEBOUNCE_MS) {
    entrada.estavel = entrada.bruto;
    return true;
  }
  return false;
}

void enviarComandoMp3(uint8_t comando, uint16_t parametro) {
  uint8_t pacote[10] = {0x7E, 0xFF, 0x06, comando, 0x00,
                        (uint8_t)(parametro >> 8), (uint8_t)(parametro & 0xFF),
                        0x00, 0x00, 0xEF};
  uint16_t soma = 0;
  for (uint8_t i = 1; i <= 6; ++i) soma += pacote[i];
  uint16_t checksum = (uint16_t)(0u - soma);
  pacote[7] = (uint8_t)(checksum >> 8);
  pacote[8] = (uint8_t)(checksum & 0xFF);
  serialMp3.write(pacote, sizeof(pacote));
}
} // namespace

namespace Hidra {
void iniciar(ReceptorEvento callback) {
  receptor = callback;
  prepararEntrada(botao);
  prepararEntrada(chave);
  leitura.botao_pressionado = (botao.estavel == LOW);
  leitura.chave_inicial_acionada = (chave.estavel == LOW);
  leitura.massa_valida = false;
  leitura.massa_g = 0.0f;
  leitura.instante_massa_ms = 0;
  inicio_monitoramento_massa_ms = millis();

  pinMode(HidraConfig::PINO_LED_VERMELHO, OUTPUT);
  digitalWrite(HidraConfig::PINO_LED_VERMELHO, LOW);
  serialMp3.begin(HidraConfig::MP3_BAUD, SERIAL_8N1,
                  HidraConfig::PINO_MP3_RX_ESP32, HidraConfig::PINO_MP3_TX_ESP32);
  definirVolume(HidraConfig::VOLUME_MP3);

  balanca.begin(HidraConfig::PINO_HX711_DOUT, HidraConfig::PINO_HX711_SCK);
  balanca.set_scale(HidraConfig::FATOR_HX711_CONTAGENS_POR_GRAMA);
  if (balanca.wait_ready_timeout(HidraConfig::ESPERA_INICIAL_HX711_MS)) {
    realizarTara(); // IMPORTANTE: plataforma deve estar vazia durante a inicializacao
  } else {
    publicar(TipoEventoFisico::MassaIndisponivel, millis());
    massa_falha_reportada = true;
  }
}

bool realizarTara() {
  if (!balanca.wait_ready_timeout(2000)) return false;
  balanca.tare(HidraConfig::AMOSTRAS_TARA);
  leitura.massa_valida = false;
  leitura.instante_massa_ms = 0;
  filtro_inicializado = false;
  massa_falha_reportada = false;
  ultima_tentativa_massa_ms = millis();
  inicio_monitoramento_massa_ms = millis();
  return true;
}

void atualizar() {
  const uint32_t agora = millis();
  if (detectarTransicao(botao, agora)) {
    leitura.botao_pressionado = (botao.estavel == LOW);
    publicar(leitura.botao_pressionado ? TipoEventoFisico::BotaoPressionado
                                      : TipoEventoFisico::BotaoLiberado, agora);
  }
  if (detectarTransicao(chave, agora)) {
    leitura.chave_inicial_acionada = (chave.estavel == LOW);
    publicar(leitura.chave_inicial_acionada ? TipoEventoFisico::ChaveInicialAcionada
                                           : TipoEventoFisico::ChaveInicialLiberada, agora);
  }

  if ((uint32_t)(agora - ultima_tentativa_massa_ms) >= HidraConfig::INTERVALO_MASSA_MS) {
    ultima_tentativa_massa_ms = agora;
    if (balanca.is_ready()) {
      const float amostra_g = balanca.get_units(1); // A conversao ja esta pronta.
      if (isfinite(amostra_g)) {
        leitura.massa_g = filtro_inicializado
          ? HidraConfig::FILTRO_MASSA_ALPHA * amostra_g
              + (1.0f - HidraConfig::FILTRO_MASSA_ALPHA) * leitura.massa_g
          : amostra_g;
        filtro_inicializado = true;
        leitura.massa_valida = true;
        leitura.instante_massa_ms = millis();
        massa_falha_reportada = false;
        publicar(TipoEventoFisico::MassaAtualizada, leitura.instante_massa_ms,
                 leitura.massa_g, true);
      }
    }
  }

  if (filtro_inicializado && leitura.massa_valida &&
      (uint32_t)(millis() - leitura.instante_massa_ms) > HidraConfig::TIMEOUT_MASSA_MS) {
    leitura.massa_valida = false;
  }
  // A ausencia de novas amostras validas deve ser avaliada a partir da
  // ultima amostra, nao da ultima tentativa (atualizada a cada 200 ms).
  const uint32_t referencia_massa_ms = filtro_inicializado
    ? leitura.instante_massa_ms : inicio_monitoramento_massa_ms;
  if (!leitura.massa_valida && !massa_falha_reportada &&
      (uint32_t)(millis() - referencia_massa_ms) > HidraConfig::TIMEOUT_MASSA_MS) {
    publicar(TipoEventoFisico::MassaIndisponivel, millis());
    massa_falha_reportada = true;
  }
  // Consumir respostas UART sem misturar o protocolo MP3 ao diagnostico de sensores.
  while (serialMp3.available()) serialMp3.read();
}

LeituraHardware obterLeitura() { return leitura; }

void definirLed(bool ligado) {
  digitalWrite(HidraConfig::PINO_LED_VERMELHO, ligado ? HIGH : LOW);
}

bool reproduzirFaixa(uint8_t numero) {
  if (numero < HidraConfig::PRIMEIRA_FAIXA_MP3 ||
      numero > HidraConfig::ULTIMA_FAIXA_MP3) return false;
  enviarComandoMp3(0x12, numero); // /mp3/000N.mp3
  return true; // Indica comando UART aceito, NAO reproduzido com sucesso.
}

void definirVolume(uint8_t nivel) {
  if (nivel > 30) nivel = 30;
  enviarComandoMp3(0x06, nivel);
}
} // namespace Hidra
