// =============================================================================
// Stub do modulo IoT — SUBSTITUIR pela implementacao da Etapa 3.
// Imprime cada evento no Monitor Serial com o mesmo JSON que seria enviado
// ao Firebase (linha prefixada por "IOT> "). Util para:
//  - demonstrar a interface com o modulo IoT sem Wi-Fi;
//  - comparar com as fixtures/simulador Python da Etapa 3.
// =============================================================================
#include "HidraIot.h"

#if HIDRA_IOT_STUB
#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

namespace {
const char* g_device_id = "hidra_01";
uint32_t g_event_counter = 0;
HidraStatusSnapshot g_status = {};

int64_t utcAgoraMs() {
  const time_t t = time(nullptr);
  if (t < 1700000000) return -1; // relogio ainda nao sincronizado
  return (int64_t)t * 1000;
}
} // namespace

void iot_begin(const IotConfig& config) {
  if (config.device_id && config.device_id[0]) g_device_id = config.device_id;
  Serial.printf("IOT> stub ativo (device_id=%s, boot_id=%s). Eventos impressos, nao enviados.\n",
                g_device_id, hidra_boot_id());
}

bool iot_publish(const HidraEvent& event) {
  if (event.schema_version != HIDRA_SCHEMA_VERSION || event.session_id[0] == '\0' ||
      event.sequence == 0) {
    return false; // registro invalido
  }
  char event_id[24];
  snprintf(event_id, sizeof(event_id), "%s_e%05lu", hidra_boot_id(),
           (unsigned long)++g_event_counter);
  static char json[HIDRA_EVENT_JSON_MAX];
  const size_t n = hidra_event_to_json(event, g_device_id, event_id, hidra_boot_id(),
                                       utcAgoraMs(), millis(), json, sizeof(json));
  if (n == 0) {
    Serial.println("IOT> ERRO: evento excede 1024 bytes; recusado");
    return false;
  }
  Serial.print("IOT> ");
  Serial.println(json);
  return true;
}

IotStatus iot_get_status() {
  IotStatus s = {};
  strncpy(s.last_error, "stub: sem conectividade", sizeof(s.last_error) - 1);
  return s;
}

void iot_update_status(const HidraStatusSnapshot& snapshot) {
  g_status = snapshot; // o modulo real publicaria /status a cada 30 s
}
uint32_t iot_last_session_order() { return 0; }
IotRecoveredSession iot_recovered_session() { return {}; }
#endif
