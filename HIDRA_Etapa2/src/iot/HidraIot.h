#pragma once
// =============================================================================
// HIDRA — Interface com o modulo IoT (Etapa 3)
// -----------------------------------------------------------------------------
// As tres primeiras funcoes sao exatamente as da apostila (secao "Interface
// com orquestrador"). iot_update_status() e uma EXTENSAO proposta pela
// Etapa 2 para entregar o "snapshot atual para status" citado em
// "Interface necessaria das etapas 1 e 2".
//
// Enquanto o modulo real da Etapa 3 nao existir, o arquivo HidraIotStub.cpp
// implementa esta interface imprimindo os eventos em JSON no Monitor Serial.
// Para integrar: remover/desativar o stub (HIDRA_IOT_STUB = 0) e adicionar
// os arquivos do modulo IoT que implementam estas mesmas funcoes.
//
// Regras do contrato (apostila):
//  - iot_publish() COPIA o evento para uma fila e retorna imediatamente;
//    true = aceito em memoria, false = fila cheia ou registro invalido.
//    NAO faz chamada de rede nem escrita em Flash. NAO confirma entrega.
//  - O orquestrador nunca depende do retorno para continuar a atividade.
// =============================================================================
#include "../contrato/HidraEvent.h"

#ifndef HIDRA_IOT_STUB
#define HIDRA_IOT_STUB 1   // 1 = usar stub serial; 0 = modulo IoT real
#endif

struct IotConfig {
  const char* device_id;        // ex.: "hidra_01"
  const char* wifi_ssid;
  const char* wifi_password;
  const char* database_url;
  const char* api_key;          // chave de identificacao do projeto
  const char* device_email;     // credenciais exclusivas do dispositivo
  const char* device_password;
};

struct IotStatus {
  bool     wifi_connected;
  bool     authenticated;
  uint32_t pending_count;
  uint32_t dropped_count;
  char     last_error[64];
};

void      iot_begin(const IotConfig& config);
bool      iot_publish(const HidraEvent& event);
IotStatus iot_get_status();

// Snapshot do orquestrador; publicado pelo adapter da etapa 3.
void      iot_update_status(const HidraStatusSnapshot& snapshot);

// Contador já reservado no diário IoT e referência recuperada após reinício.
uint32_t iot_last_session_order();
struct IotRecoveredSession {
  char session_id[64];
  uint32_t last_sequence;
  ActivityState state;
  bool open;
};
IotRecoveredSession iot_recovered_session();
