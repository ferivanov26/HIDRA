#pragma once
// =============================================================================
// HIDRA — Contrato de eventos entre a Etapa 2 (orquestrador) e a Etapa 3 (IoT)
// -----------------------------------------------------------------------------
// Este arquivo segue a secao "Estrutura dos eventos da atividade" e
// "Estrutura interna do Firmware" da apostila (Etapa 3). Ele e C++ puro
// (sem Arduino.h) para poder ser compilado e testado tambem no computador.
//
// Responsabilidades (conforme apostila):
//  - O ORQUESTRADOR preenche: schema_version, session_id, sequence, type,
//    state e data.
//  - O MODULO IoT acrescenta na entrada: device_id, event_id, boot_id,
//    occurred_at_ms (quando o relogio UTC for valido) e uptime_ms.
//
// Campos com "has_" indicam presenca: campos ausentes NAO devem ser
// serializados (null remove o campo no Realtime Database).
// =============================================================================
#include <stdint.h>
#include <stddef.h>

constexpr uint8_t HIDRA_SCHEMA_VERSION = 1;
constexpr size_t  HIDRA_EVENT_JSON_MAX = 1024; // limite serializado por evento

enum class EventType : uint8_t {
  ActivityStarted,    // "activity_started"   data: trigger (+ session_order*)
  StateChanged,       // "state_changed"      data: previous_state, mass_g?
  InstructionPlayed,  // "instruction_played" data: instruction_id, attempt
  CaregiverAlert,     // "caregiver_alert"    data: alert_id, reason, attempt
  AlertResolved,      // "alert_resolved"     data: alert_id, reason
  ActivityCompleted,  // "activity_completed" data: estimated_intake_ml?
  ActivityAborted     // "activity_aborted"   data: reason (restart|local_cancel)
};

// Estado atual da atividade (E0..E5). Semantica adotada pela Etapa 2:
// "Ek" = ultima etapa observada/concluida. Ex.: E2 = copo posicionado na
// plataforma, aguardando a adicao de agua (E3).
enum class ActivityState : uint8_t { E0 = 0, E1, E2, E3, E4, E5 };

enum class StartTrigger : uint8_t { Button, Schedule };

struct EventData {
  bool has_previous_state;      ActivityState previous_state;
  bool has_trigger;             StartTrigger  trigger;
  bool has_instruction_id;      char instruction_id[24];
  bool has_alert_id;            char alert_id[16];
  bool has_reason;              char reason[24];
  bool has_attempt;             uint8_t attempt;
  bool has_mass_g;              float mass_g;
  bool has_estimated_intake_ml; float estimated_intake_ml;
  // (*) PROPOSTA DA ETAPA 2 — ainda nao consta na tabela de campos da apostila.
  // O resumo da sessao (summary/session_order) precisa de um contador
  // persistente por dispositivo; o orquestrador ja o mantem e o envia apenas
  // em activity_started. Combinar com a Etapa 3 (regras de validacao).
  bool has_session_order;       uint32_t session_order;
};

struct HidraEvent {
  uint8_t       schema_version;
  char          session_id[64];
  uint32_t      sequence;        // 1, 2, 3... dentro da sessao
  EventType     type;
  ActivityState state;
  EventData     data;
};

// Snapshot atual do orquestrador para o no "status" (publicado pela Etapa 3
// a cada 30 s). Entregue por iot_update_status() sempre que mudar.
struct HidraStatusSnapshot {
  bool          activity_active;   // false = sistema ocioso, entre sessoes
  char          session_id[64];    // sessao atual ou ultima sessao ("" se nenhuma)
  ActivityState state;             // estado atual ou ultimo estado
  uint32_t      sequence;          // ultima sequence emitida na sessao
  uint8_t       attempt;           // tentativas sem progresso na etapa atual
  uint8_t       active_alerts;     // alertas ativos
  bool          mass_available;    // celula de carga fornecendo leituras
  float         mass_g;            // ultima massa filtrada (informativo)
};

// ---- Utilitarios ------------------------------------------------------------
const char* hidra_event_type_name(EventType t);
const char* hidra_state_name(ActivityState s);
const char* hidra_trigger_name(StartTrigger t);

void hidra_event_init(HidraEvent& ev);           // zera e define schema_version
void hidra_copy_text(char* dst, size_t cap, const char* src);

// Serializa somente o objeto "data" (campos presentes). Retorna o tamanho
// escrito ou 0 se nao couber (nunca trunca silenciosamente).
size_t hidra_event_data_to_json(const EventData& d, char* buf, size_t cap);

// Serializa o evento completo no formato do exemplo da apostila.
// occurred_at_ms < 0 => campo omitido (relogio UTC ainda invalido).
size_t hidra_event_to_json(const HidraEvent& ev, const char* device_id,
                           const char* event_id, const char* boot_id,
                           int64_t occurred_at_ms, uint32_t uptime_ms,
                           char* buf, size_t cap);

// ---- Identidade da inicializacao (boot_id) ----------------------------------
// boot_id e compartilhado pelo orquestrador (para formar session_id) e pelo
// modulo IoT (para formar event_id). Gerado a partir de um contador de boots
// persistente => nao se repete entre reinicializacoes (contador completo de 32 bits).
void        hidra_set_boot_counter(uint32_t boot_counter);
const char* hidra_boot_id();
