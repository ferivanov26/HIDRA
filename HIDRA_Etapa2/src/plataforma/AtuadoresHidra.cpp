#include "AtuadoresHidra.h"
#include "../hardware/HidraHardware.h"

// ---------------------------------------------------------------- FilaAudio --
void FilaAudio::iniciar(uint8_t volume, uint32_t agora) {
  volume_ = volume > 30 ? 30 : volume;
  t_inicio_ = agora;
  volume_reenviado_ = false;
}

void FilaAudio::definirVolume(uint8_t volume) {
  volume_ = volume > 30 ? 30 : volume;
  Hidra::definirVolume(volume_);
}

void FilaAudio::solicitar(uint8_t faixa, uint32_t t_gatilho, uint32_t agora) {
  if (pendente_ != 0) ++substituidas_;
  pendente_ = faixa;
  gatilho_ = t_gatilho;
  tick(agora); // envia imediatamente se o intervalo minimo ja passou
}

void FilaAudio::tick(uint32_t agora) {
  if (!volume_reenviado_ && (uint32_t)(agora - t_inicio_) >= REENVIO_VOLUME_MS) {
    volume_reenviado_ = true;
    Hidra::definirVolume(volume_);
  }
  if (pendente_ == 0) return;
  if (ja_enviou_ && (uint32_t)(agora - t_ultimo_envio_) < INTERVALO_MIN_MS) return;
  enviar(agora);
}

void FilaAudio::enviar(uint32_t agora) {
  const uint8_t faixa = pendente_;
  pendente_ = 0;
  if (!Hidra::reproduzirFaixa(faixa)) {
    Serial.printf("AUDIO: faixa %u invalida\n", faixa);
    return;
  }
  ja_enviou_ = true;
  t_ultimo_envio_ = agora;
  ++enviadas_;
  ultima_latencia_ = agora - gatilho_;
  if (ultima_latencia_ > maior_latencia_) maior_latencia_ = ultima_latencia_;
  // RNF_LATENCIA: tempo entre deteccao/timeout e o envio do comando ao MP3.
  Serial.printf("AUDIO: faixa %04u.mp3 enviada (latencia %lu ms)\n", faixa,
                (unsigned long)ultima_latencia_);
}

// ----------------------------------------------------------------- LedHidra --
const char* LedHidra::nome(PadraoLed p) {
  switch (p) {
    case PadraoLed::Ocioso:    return "ocioso (pulso a cada 3 s)";
    case PadraoLed::Ativo:     return "atividade (1 s aceso / 1 s apagado)";
    case PadraoLed::Alerta:    return "ALERTA (piscar rapido)";
    case PadraoLed::Falha:     return "falha de sensor (piscada dupla)";
    case PadraoLed::Concluido: return "concluido (aceso 3 s)";
  }
  return "?";
}

void LedHidra::atualizar(PadraoLed padrao, uint32_t agora) {
  if (!iniciado_ || padrao != atual_) {
    atual_ = padrao; t_inicio_ = agora; iniciado_ = true;
  }
  const uint32_t fase = agora - t_inicio_;
  bool aceso = false;
  switch (atual_) {
    case PadraoLed::Ocioso:    aceso = (fase % 3000) < 80; break;
    case PadraoLed::Ativo:     aceso = (fase % 2000) < 1000; break;
    case PadraoLed::Alerta:    aceso = (fase % 300) < 150; break;
    case PadraoLed::Falha: {
      const uint32_t f = fase % 2000;
      aceso = f < 120 || (f >= 300 && f < 420);
      break;
    }
    case PadraoLed::Concluido: aceso = true; break;
  }
  if (aceso != aceso_ || fase == 0) {
    aceso_ = aceso;
    Hidra::definirLed(aceso);
  }
}
