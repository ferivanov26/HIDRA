// =============================================================================
// HIDRA — Teste de fumaca do FIRMWARE COMPLETO (sketch .ino + Etapa 1 + 2)
// compilado no PC com mocks de Arduino/ESP32. Verifica: boot, console,
// agenda por horario, sessao completa pelos sensores, comandos UART do MP3,
// registro em LittleFS, persistencia NVS e recuperacao apos reinicio.
//   fase a: sessao agendada completa + nova sessao deixada aberta
//   fase b: "religa" com a NVS da fase a => activity_aborted(restart)
// =============================================================================
#include "mocks/Arduino.h"
#include "mocks/Preferences.h"
#include "mocks/LittleFS.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <vector>

namespace mock {
uint32_t agora_ms = 0;
int pinos[64] = {};
std::string saida_serial;
std::deque<char> entrada_serial;
std::deque<uint8_t> uart2_tx;
std::map<std::string, std::vector<uint8_t>> nvs;
std::map<std::string, std::string> arquivos;
float massa_real_g = 0.0f;
bool hx711_ok = true;
int64_t epoca_s = 0;
}
HardwareSerial Serial(0);
FSMock LittleFS;

#include "../HIDRA_Etapa2/HIDRA_Etapa2.ino"

static int falhas = 0;
static void check(bool ok, const char* o) { std::printf("  [%s] %s\n", ok ? "ok" : "FALHOU", o); if (!ok) ++falhas; }
static bool contem(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }
static void rodar(uint32_t ms) { for (uint32_t i = 0; i < ms / 5; ++i) { mock::agora_ms += 5; loop(); } }
static void cmd(const char* s) { for (const char* p = s; *p; ++p) mock::entrada_serial.push_back(*p); mock::entrada_serial.push_back('\n'); rodar(20); }
static bool tocou(uint8_t faixa) { // pacote 7E FF 06 12 00 00 NN .. .. EF
  auto& b = mock::uart2_tx;
  for (size_t i = 0; i + 9 < b.size(); ++i)
    if (b[i] == 0x7E && b[i + 3] == 0x12 && b[i + 6] == faixa && b[i + 9] == 0xEF) return true;
  return false;
}
static const char* ESTADO = "/tmp/hidra_smoke_estado.txt";
static void salvarEstado() {
  std::ofstream f(ESTADO, std::ios::binary);
  f << mock::nvs.size() << "\n";
  for (auto& kv : mock::nvs) { f << kv.first << "\n" << kv.second.size() << "\n"; f.write((char*)kv.second.data(), kv.second.size()); f << "\n"; }
  f << mock::arquivos.size() << "\n";
  for (auto& kv : mock::arquivos) { f << kv.first << "\n" << kv.second.size() << "\n" << kv.second << "\n"; }
}
static void carregarEstado() {
  std::ifstream f(ESTADO, std::ios::binary); size_t n; f >> n; f.get();
  for (size_t i = 0; i < n; ++i) { std::string k; std::getline(f, k); size_t sz; f >> sz; f.get();
    std::vector<uint8_t> v(sz); f.read((char*)v.data(), sz); f.get(); mock::nvs[k] = v; }
  f >> n; f.get();
  for (size_t i = 0; i < n; ++i) { std::string k; std::getline(f, k); size_t sz; f >> sz; f.get();
    std::string v(sz, '\0'); f.read(&v[0], sz); f.get(); mock::arquivos[k] = v; }
}

int main(int argc, char** argv) {
  const bool fase_b = argc > 1 && argv[1][0] == 'b';
  mock::pinos[18] = HIGH;  // botao solto
  mock::pinos[19] = LOW;   // microchave acionada = copo na base
  if (fase_b) carregarEstado();
  setup();
  rodar(3000);

  if (!fase_b) {
    std::printf("Fase a: boot, agenda e sessao completa\n");
    check(contem(mock::saida_serial, "boot_id=0001"), "boot_id 0001 no primeiro boot");
    g_pers.garantirOrdemSessaoMinima(37);
    check(g_pers.proximaOrdemSessao() == 38, "migracao respeita ordem ja reservada pelo IoT");
    g_pers.garantirOrdemSessaoMinima(2);
    check(g_pers.proximaOrdemSessao() == 39, "ordem NVS nao retrocede ao receber piso antigo");
    cmd("relogio 2026-11-14 08:59:50");
    cmd("horarios 09:00");
    cmd("set e2 20");
    check(contem(mock::saida_serial, "Relogio ajustado: 2026-11-14 08:59:50"), "relogio ajustado pelo console");
    rodar(12000);
    check(contem(mock::saida_serial, "\"type\":\"activity_started\"") &&
          contem(mock::saida_serial, "\"trigger\":\"schedule\""), "inicio pelo horario 09:00");
    check(contem(mock::saida_serial, "\"occurred_at_ms\":"), "occurred_at_ms presente com relogio valido");
    mock::pinos[19] = HIGH; rodar(2000);       // pega o copo
    mock::massa_real_g = 200; rodar(4000);     // posiciona
    mock::massa_real_g = 450; rodar(4000);     // agua
    mock::massa_real_g = 0;   rodar(10000);    // retira e bebe
    mock::massa_real_g = 300; rodar(4000);     // devolve na plataforma
    {
      const size_t k = mock::saida_serial.find("\"estimated_intake_ml\":");
      const int ml = k == std::string::npos ? -1 : std::atoi(mock::saida_serial.c_str() + k + 22);
      std::printf("    volume estimado: %d ml (real: 150 ml)\n", ml);
      check(contem(mock::saida_serial, "\"type\":\"activity_completed\"") && ml >= 145 && ml <= 155,
            "sessao concluida com ~150 ml estimados");
    }
    check(tocou(1) && tocou(6), "UART do MP3 recebeu faixas 0001 e 0006");
    check(!tocou(4), "nenhum lembrete desnecessario (faixa 0004)");
    cmd("s");
    check(contem(mock::saida_serial, "Orquestrador: ocioso"), "comando 's'");
    mock::saida_serial.clear();
    cmd("log");
    check(contem(mock::saida_serial, "data_hora_local;boot_id") &&
          contem(mock::saida_serial, "2026-11-14 09:00") &&
          contem(mock::saida_serial, ";activity_completed;E5;"), "registro LittleFS com data/hora");
    cmd("set volume 99");
    check(contem(mock::saida_serial, "fora da faixa"), "validacao de parametro");
    // nova sessao pelo botao e "queda de energia" no meio
    mock::massa_real_g = 0; mock::pinos[19] = LOW; rodar(3000);
    mock::pinos[18] = LOW; rodar(100); mock::pinos[18] = HIGH; rodar(500);
    mock::pinos[19] = HIGH; rodar(2000);
    check(mock::nvs.count("sessao") == 1, "sessao aberta persistida na NVS");
    salvarEstado();
  } else {
    std::printf("Fase b: religado apos queda de energia com sessao aberta\n");
    check(contem(mock::saida_serial, "boot_id=0002"), "boot_id 0002");
    check(contem(mock::saida_serial, "\"session_id\":\"0001_s002\"") &&
          contem(mock::saida_serial, "\"type\":\"activity_aborted\"") &&
          contem(mock::saida_serial, "\"reason\":\"restart\""), "activity_aborted(restart) da sessao anterior");
    check(mock::nvs.count("sessao") == 0, "marcador de sessao limpo");
    mock::saida_serial.clear();
    cmd("cfg");
    check(contem(mock::saida_serial, "E2 20"), "parametro alterado na fase a persistiu (E2 = 20 s)");
  }
  std::printf("%s\n", falhas ? "FALHAS" : "OK");
  return falhas ? 1 : 0;
}
