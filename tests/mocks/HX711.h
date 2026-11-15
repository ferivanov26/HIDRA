#pragma once
#include <cstdint>
namespace mock { extern float massa_real_g; extern bool hx711_ok; extern uint32_t agora_ms; }

class HX711 {
 public:
  void begin(uint8_t, uint8_t) {}
  void set_scale(float) {}
  bool wait_ready_timeout(unsigned long) { return mock::hx711_ok; }
  bool is_ready() { return mock::hx711_ok; }
  void tare(uint8_t) {}
  float get_units(uint8_t) { return mock::massa_real_g; }
};
