#pragma once
// =============================================================================
// HIDRA — Relogio e agenda (RF_INICIAR_ATIVIDADE por horario programado)
// -----------------------------------------------------------------------------
// O ESP32 nao tem RTC com bateria. O relogio fica valido quando:
//  (a) o modulo IoT (Etapa 3, time_service) sincronizar por NTP — usa o
//      relogio do sistema (time()), entao nada precisa mudar aqui; ou
//  (b) o cuidador acertar a hora pelo console: "relogio 2026-11-14 19:30".
// Sem relogio valido, o inicio por horario fica desabilitado (o botao segue
// funcionando) e os registros usam boot_id + uptime.
// =============================================================================
#include <stdint.h>
#include <stddef.h>
#include "../orquestrador/Parametros.h"

namespace Relogio {
bool    valido();
int64_t utcMs();   // -1 se invalido
bool    definirLocal(int ano, int mes, int dia, int hora, int minuto, int segundo, int16_t fuso_min);
// Data/hora local. Retorna false se o relogio for invalido.
bool    local(int16_t fuso_min, int& ano, int& mes, int& dia, int& hora, int& minuto, int& segundo,
              int32_t& dia_absoluto);
void    formatarLocal(int16_t fuso_min, char* buf, size_t n); // "2026-11-14 19:30:05" ou "sem-relogio"
}

class Agenda {
 public:
  Agenda();
  // Retorna true no minuto programado (uma vez por dia por horario).
  bool verificar(const ParametrosAtividade& p, uint32_t agora_ms);
 private:
  int32_t  ultimo_dia_[MAX_HORARIOS];
  uint32_t ultima_verificacao_ = 0;
};
