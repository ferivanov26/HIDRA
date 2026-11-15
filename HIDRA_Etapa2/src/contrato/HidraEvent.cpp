#include "HidraEvent.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

const char* hidra_event_type_name(EventType t) {
  switch (t) {
    case EventType::ActivityStarted:   return "activity_started";
    case EventType::StateChanged:      return "state_changed";
    case EventType::InstructionPlayed: return "instruction_played";
    case EventType::CaregiverAlert:    return "caregiver_alert";
    case EventType::AlertResolved:     return "alert_resolved";
    case EventType::ActivityCompleted: return "activity_completed";
    case EventType::ActivityAborted:   return "activity_aborted";
  }
  return "unknown";
}

const char* hidra_state_name(ActivityState s) {
  static const char* const nomes[] = {"E0", "E1", "E2", "E3", "E4", "E5"};
  const uint8_t i = (uint8_t)s;
  return i < 6 ? nomes[i] : "E?";
}

const char* hidra_trigger_name(StartTrigger t) {
  return t == StartTrigger::Schedule ? "schedule" : "button";
}

void hidra_event_init(HidraEvent& ev) {
  memset(&ev, 0, sizeof(ev));
  ev.schema_version = HIDRA_SCHEMA_VERSION;
}

void hidra_copy_text(char* dst, size_t cap, const char* src) {
  if (cap == 0) return;
  if (src == nullptr) { dst[0] = '\0'; return; }
  size_t n = strlen(src);
  if (n >= cap) n = cap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

namespace {
// Escritor com controle de estouro: ok vira false se o texto nao couber.
struct Escritor {
  char* buf; size_t cap; size_t len; bool ok;
  void add(const char* fmt, ...) {
    if (!ok) return;
    va_list ap; va_start(ap, fmt);
    const int n = vsnprintf(buf + len, cap - len, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - len) { ok = false; return; }
    len += (size_t)n;
  }
  void texto(const char* s) { // string JSON com escape minimo
    add("\"");
    for (const char* p = s; ok && *p; ++p) {
      const unsigned char c = (unsigned char)*p;
      if (c == '"' || c == '\\') add("\\%c", c);
      else if (c < 0x20) add("\\u%04x", c);
      else add("%c", c);
    }
    add("\"");
  }
};

void escreverData(Escritor& w, const EventData& d) {
  bool primeiro = true;
  auto chave = [&](const char* k) { w.add(primeiro ? "\"%s\":" : ",\"%s\":", k); primeiro = false; };
  w.add("{");
  if (d.has_previous_state) { chave("previous_state"); w.texto(hidra_state_name(d.previous_state)); }
  if (d.has_trigger)        { chave("trigger"); w.texto(hidra_trigger_name(d.trigger)); }
  if (d.has_instruction_id) { chave("instruction_id"); w.texto(d.instruction_id); }
  if (d.has_alert_id)       { chave("alert_id"); w.texto(d.alert_id); }
  if (d.has_reason)         { chave("reason"); w.texto(d.reason); }
  if (d.has_attempt)        { chave("attempt"); w.add("%u", (unsigned)d.attempt); }
  if (d.has_mass_g)         { chave("mass_g"); w.add("%.1f", (double)d.mass_g); }
  if (d.has_estimated_intake_ml) { chave("estimated_intake_ml"); w.add("%.0f", (double)d.estimated_intake_ml); }
  if (d.has_session_order)  { chave("session_order"); w.add("%lu", (unsigned long)d.session_order); }
  w.add("}");
}
} // namespace

size_t hidra_event_data_to_json(const EventData& d, char* buf, size_t cap) {
  if (cap == 0) return 0;
  Escritor w{buf, cap, 0, true};
  escreverData(w, d);
  if (!w.ok) { buf[0] = '\0'; return 0; }
  return w.len;
}

size_t hidra_event_to_json(const HidraEvent& ev, const char* device_id,
                           const char* event_id, const char* boot_id,
                           int64_t occurred_at_ms, uint32_t uptime_ms,
                           char* buf, size_t cap) {
  if (cap == 0) return 0;
  Escritor w{buf, cap, 0, true};
  w.add("{\"schema_version\":%u,\"device_id\":", (unsigned)ev.schema_version);
  w.texto(device_id ? device_id : "");
  w.add(",\"session_id\":"); w.texto(ev.session_id);
  w.add(",\"event_id\":");   w.texto(event_id ? event_id : "");
  w.add(",\"sequence\":%lu,\"type\":", (unsigned long)ev.sequence);
  w.texto(hidra_event_type_name(ev.type));
  w.add(",\"state\":"); w.texto(hidra_state_name(ev.state));
  if (occurred_at_ms >= 0) { // sem %lld: nem toda newlib (printf "nano") suporta 64 bits
    const unsigned long seg = (unsigned long)(occurred_at_ms / 1000);
    const unsigned ms = (unsigned)(occurred_at_ms % 1000);
    if (seg > 0) w.add(",\"occurred_at_ms\":%lu%03u", seg, ms);
    else         w.add(",\"occurred_at_ms\":%u", ms);
  }
  w.add(",\"boot_id\":"); w.texto(boot_id ? boot_id : "");
  w.add(",\"uptime_ms\":%lu,\"data\":", (unsigned long)uptime_ms);
  escreverData(w, ev.data);
  w.add("}");
  if (!w.ok) { buf[0] = '\0'; return 0; }
  return w.len;
}

// ---- boot_id -----------------------------------------------------------------
namespace { char g_boot_id[12] = "0000"; }

void hidra_set_boot_counter(uint32_t boot_counter) {
  snprintf(g_boot_id, sizeof(g_boot_id), "%04lx", (unsigned long)boot_counter);
}

const char* hidra_boot_id() { return g_boot_id; }
