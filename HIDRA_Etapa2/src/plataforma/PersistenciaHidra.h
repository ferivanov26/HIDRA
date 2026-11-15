#pragma once
// =============================================================================
// HIDRA — Persistencia local (RNF_PERSISTENCIA, RF_REGISTRAR_EVENTOS)
//  - NVS (Preferences): parametros do cuidador, contador de boots, contador
//    de sessoes (session_order) e referencia da sessao aberta.
//  - LittleFS: registro de eventos com data/hora em /hidra_log.csv (rotativo,
//    ~48 KB + arquivo anterior), consultavel pelo comando "log".
// Particao: usar o esquema padrao "Default 4MB with spiffs" (o LittleFS usa
// a particao "spiffs"). Na 1a execucao o sistema de arquivos e formatado.
// =============================================================================
#include <Arduino.h>
#include "../orquestrador/Orquestrador.h"
#include "../orquestrador/Parametros.h"

class PersistenciaHidra {
 public:
  bool     iniciar();
  uint32_t incrementarBoot();
  bool     carregarParametros(ParametrosAtividade& p);
  bool     salvarParametros(const ParametrosAtividade& p);
  uint32_t proximaOrdemSessao();

  bool lerSessaoAberta(SessaoPersistida& s);
  void salvarSessaoAberta(const SessaoPersistida& s);
  void limparSessaoAberta();

  void registrar(const char* linha);
  void imprimirLog(Print& saida);
  void limparLog();
  bool logDisponivel() const { return fs_ok_; }
  bool nvsDisponivel() const { return nvs_ok_; }

 private:
  bool nvs_ok_ = false;
  bool fs_ok_ = false;
};
