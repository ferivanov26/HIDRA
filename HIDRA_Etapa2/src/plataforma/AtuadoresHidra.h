#pragma once
// =============================================================================
// HIDRA — Atuadores locais (camada "Atuacao" da apostila)
//  - FilaAudio: entrega as instrucoes ao modulo MP3 sem bloquear o loop.
//  - LedHidra : padroes de sinalizacao visual (RNF_INDICACAO_VISUAL).
// Usam somente as funcoes publicas de HidraHardware.h (Etapa 1).
// =============================================================================
#include <Arduino.h>
#include "../orquestrador/Orquestrador.h"

class FilaAudio {
 public:
  // O comando "tocar" do MP3 interrompe a faixa atual. Como a Etapa 1 nao
  // informa o termino da reproducao, garante-se um intervalo minimo entre
  // comandos para nao cortar uma mensagem recem-iniciada. Se chegar outra
  // instrucao nesse intervalo, a MAIS RECENTE e a que sera tocada.
  static constexpr uint32_t INTERVALO_MIN_MS = 2500;
  static constexpr uint32_t REENVIO_VOLUME_MS = 2500; // modulo pode ignorar o 1o comando

  void iniciar(uint8_t volume, uint32_t agora);
  void definirVolume(uint8_t volume);
  void solicitar(uint8_t faixa, uint32_t t_gatilho, uint32_t agora);
  void tick(uint32_t agora);

  uint32_t ultimaLatenciaMs() const { return ultima_latencia_; }
  uint32_t maiorLatenciaMs() const { return maior_latencia_; }
  uint32_t enviadas() const { return enviadas_; }
  uint32_t substituidas() const { return substituidas_; }

 private:
  void enviar(uint32_t agora);
  uint8_t  volume_ = 30;
  uint8_t  pendente_ = 0;
  uint32_t gatilho_ = 0;
  bool     ja_enviou_ = false;
  uint32_t t_ultimo_envio_ = 0;
  uint32_t t_inicio_ = 0;
  bool     volume_reenviado_ = false;
  uint32_t ultima_latencia_ = 0, maior_latencia_ = 0, enviadas_ = 0, substituidas_ = 0;
};

class LedHidra {
 public:
  void atualizar(PadraoLed padrao, uint32_t agora);
  static const char* nome(PadraoLed p);
 private:
  PadraoLed atual_ = PadraoLed::Ocioso;
  bool      iniciado_ = false;
  uint32_t  t_inicio_ = 0;
  bool      aceso_ = false;
};
