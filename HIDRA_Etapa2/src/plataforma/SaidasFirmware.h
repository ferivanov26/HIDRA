#pragma once
// =============================================================================
// HIDRA — Implementacao das Saidas do orquestrador no ESP32
//  - audio -> FilaAudio (MP3 da Etapa 1)
//  - evento -> registro local (LittleFS + Serial) e iot_publish() (Etapa 3)
//  - sessao aberta / contadores -> NVS
//  - snapshot -> iot_update_status() (Etapa 3)
// =============================================================================
#include <Arduino.h>
#include "../orquestrador/Orquestrador.h"
#include "AtuadoresHidra.h"
#include "PersistenciaHidra.h"

class SaidasFirmware : public Saidas {
 public:
  SaidasFirmware(FilaAudio& audio, PersistenciaHidra& pers, const ParametrosAtividade& p)
    : audio_(audio), pers_(pers), p_(p) {}

  void tocarFaixa(uint8_t faixa, uint32_t t_gatilho_ms) override;
  void publicarEvento(const HidraEvent& ev) override;
  void registrarLocal(const char* texto) override;
  void salvarSessaoAberta(const SessaoPersistida& s) override { pers_.salvarSessaoAberta(s); }
  void encerrarSessaoPersistida() override { pers_.limparSessaoAberta(); }
  uint32_t proximaOrdemSessao() override { return pers_.proximaOrdemSessao(); }
  void statusAlterado(const HidraStatusSnapshot& s) override;

  uint32_t eventosRecusadosIot() const { return recusados_iot_; }

 private:
  FilaAudio& audio_;
  PersistenciaHidra& pers_;
  const ParametrosAtividade& p_;
  uint32_t recusados_iot_ = 0;
};
