#include "RelogioAgenda.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

namespace {
constexpr time_t EPOCA_MINIMA = 1700000000; // 2023-11: antes disso = nao sincronizado

// Dias desde 1970-01-01 (algoritmo de H. Hinnant, calendario gregoriano)
int64_t diasDeCivil(int64_t a, unsigned m, unsigned d) {
  a -= m <= 2;
  const int64_t era = (a >= 0 ? a : a - 399) / 400;
  const unsigned yoe = (unsigned)(a - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

void civilDeDias(int64_t z, int& a, int& m, int& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  a = (int)((int64_t)yoe + era * 400 + (m <= 2));
}
} // namespace

namespace Relogio {
bool valido() { return time(nullptr) >= EPOCA_MINIMA; }

int64_t utcMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < EPOCA_MINIMA) return -1;
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

bool definirLocal(int ano, int mes, int dia, int hora, int minuto, int segundo, int16_t fuso_min) {
  if (ano < 2024 || ano > 2099 || mes < 1 || mes > 12 || dia < 1 || dia > 31 ||
      hora < 0 || hora > 23 || minuto < 0 || minuto > 59 || segundo < 0 || segundo > 59) return false;
  const int64_t local = diasDeCivil(ano, (unsigned)mes, (unsigned)dia) * 86400 +
                        hora * 3600 + minuto * 60 + segundo;
  struct timeval tv;
  tv.tv_sec = (time_t)(local - (int64_t)fuso_min * 60);
  tv.tv_usec = 0;
  return settimeofday(&tv, nullptr) == 0;
}

bool local(int16_t fuso_min, int& ano, int& mes, int& dia, int& hora, int& minuto, int& segundo,
           int32_t& dia_absoluto) {
  const time_t t = time(nullptr);
  if (t < EPOCA_MINIMA) return false;
  const int64_t l = (int64_t)t + (int64_t)fuso_min * 60;
  const int64_t dias = l >= 0 ? l / 86400 : (l - 86399) / 86400;
  const int64_t seg = l - dias * 86400;
  civilDeDias(dias, ano, mes, dia);
  hora = (int)(seg / 3600); minuto = (int)((seg % 3600) / 60); segundo = (int)(seg % 60);
  dia_absoluto = (int32_t)dias;
  return true;
}

void formatarLocal(int16_t fuso_min, char* buf, size_t n) {
  int a, m, d, h, mi, s; int32_t da;
  if (!local(fuso_min, a, m, d, h, mi, s, da)) { snprintf(buf, n, "sem-relogio"); return; }
  snprintf(buf, n, "%04d-%02d-%02d %02d:%02d:%02d", a, m, d, h, mi, s);
}
} // namespace Relogio

Agenda::Agenda() {
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) ultimo_dia_[i] = -1;
}

bool Agenda::verificar(const ParametrosAtividade& p, uint32_t agora_ms) {
  if ((uint32_t)(agora_ms - ultima_verificacao_) < 1000) return false;
  ultima_verificacao_ = agora_ms;
  int a, m, d, h, mi, s; int32_t dia;
  if (!Relogio::local(p.fuso_minutos, a, m, d, h, mi, s, dia)) return false;
  const int16_t minuto_do_dia = (int16_t)(h * 60 + mi);
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) {
    if (p.horarios_min[i] == minuto_do_dia && ultimo_dia_[i] != dia) {
      ultimo_dia_[i] = dia;
      return true;
    }
  }
  return false;
}
