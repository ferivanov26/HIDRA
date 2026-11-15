#pragma once
// Redireciona o relogio do sistema para um relogio simulado (nao altera o PC).
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
namespace mock { extern int64_t epoca_s; extern uint32_t agora_ms; }
inline time_t mock_time(time_t* t) {
  time_t v = (time_t)(mock::epoca_s + mock::agora_ms / 1000); if (t) *t = v; return v;
}
inline int mock_gettimeofday(struct timeval* tv, void*) {
  tv->tv_sec = (time_t)(mock::epoca_s + mock::agora_ms / 1000); tv->tv_usec = (mock::agora_ms % 1000) * 1000; return 0;
}
inline int mock_settimeofday(const struct timeval* tv, const void*) {
  mock::epoca_s = (int64_t)tv->tv_sec - mock::agora_ms / 1000; return 0;
}
#define time(x) mock_time(x)
#define gettimeofday(a, b) mock_gettimeofday(a, b)
#define settimeofday(a, b) mock_settimeofday(a, b)
