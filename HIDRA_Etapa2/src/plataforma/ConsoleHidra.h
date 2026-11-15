#pragma once
// =============================================================================
// HIDRA — Console serial do cuidador / tecnico (115200 baud, "Nova linha")
// Configuracao (RF_CONFIGURAR_PARAMETROS), consulta do registro
// (RF_REGISTRAR_EVENTOS), comandos de teste e diagnostico.
// =============================================================================
#include <Arduino.h>
#include "../orquestrador/Detector.h"
#include "../orquestrador/Orquestrador.h"
#include "AtuadoresHidra.h"
#include "PersistenciaHidra.h"
#include "SaidasFirmware.h"

struct ContextoConsole {
  ParametrosAtividade* param;
  PersistenciaHidra*   pers;
  FilaAudio*           audio;
  Detector*            detector;
  Orquestrador*        orq;
  SaidasFirmware*      saidas;
};

class ConsoleHidra {
 public:
  explicit ConsoleHidra(const ContextoConsole& c) : c_(c) {}
  void processar();      // chamar no loop
  void ajuda();

 private:
  void executar(char* linha);
  void mostrarEstado();
  void mostrarParametros();
  bool definir(const char* nome, const char* valor);
  bool definirHorarios(char* resto);
  bool exigirOcioso(const char* comando);
  void salvar();

  ContextoConsole c_;
  char buf_[96] = {};
  uint8_t len_ = 0;
};
