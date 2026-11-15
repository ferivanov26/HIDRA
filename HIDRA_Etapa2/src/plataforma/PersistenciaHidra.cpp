#include "PersistenciaHidra.h"
#include "../iot/HidraIot.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <string.h>

namespace {
Preferences prefs;
constexpr const char* NAMESPACE   = "hidra";
constexpr const char* K_BOOT      = "boot";
constexpr const char* K_ORDEM     = "ordem";
constexpr const char* K_PARAM     = "param";
constexpr const char* K_SESSAO    = "sessao";
constexpr const char* LOG_ATUAL    = "/hidra_log.csv";
constexpr const char* LOG_ANTERIOR = "/hidra_log_old.csv";
constexpr size_t LOG_MAX_BYTES = 48 * 1024;
constexpr const char* CABECALHO =
  "data_hora_local;boot_id;uptime_ms;session_id;sequence;tipo;estado;dados";
}

bool PersistenciaHidra::iniciar() {
  nvs_ok_ = prefs.begin(NAMESPACE, false);
#if HIDRA_IOT_STUB
  fs_ok_ = LittleFS.begin(true); // diagnóstico standalone da etapa 2
#else
  fs_ok_ = LittleFS.begin(false); // preservar diário IoT; preparo inicial explícito
#endif
  if (fs_ok_ && !LittleFS.exists(LOG_ATUAL)) {
    File f = LittleFS.open(LOG_ATUAL, FILE_WRITE);
    if (f) { f.println(CABECALHO); f.close(); }
  }
  return nvs_ok_ && fs_ok_;
}

uint32_t PersistenciaHidra::incrementarBoot() {
  if (!nvs_ok_) return 0;
  const uint32_t n = prefs.getUInt(K_BOOT, 0) + 1;
  prefs.putUInt(K_BOOT, n);
  return n;
}

bool PersistenciaHidra::carregarParametros(ParametrosAtividade& p) {
  if (!nvs_ok_ || prefs.getBytesLength(K_PARAM) != sizeof(ParametrosAtividade)) return false;
  ParametrosAtividade tmp;
  if (prefs.getBytes(K_PARAM, &tmp, sizeof(tmp)) != sizeof(tmp)) return false;
  if (!parametrosValidos(tmp)) return false; // versao antiga/corrompida => padrao
  p = tmp;
  return true;
}

bool PersistenciaHidra::salvarParametros(const ParametrosAtividade& p) {
  if (!nvs_ok_) return false;
  return prefs.putBytes(K_PARAM, &p, sizeof(p)) == sizeof(p);
}

uint32_t PersistenciaHidra::proximaOrdemSessao() {
  if (!nvs_ok_) return 0;
  const uint32_t n = prefs.getUInt(K_ORDEM, 0) + 1;
  prefs.putUInt(K_ORDEM, n);
  return n;
}

void PersistenciaHidra::garantirOrdemSessaoMinima(uint32_t ordem) {
  if (nvs_ok_ && prefs.getUInt(K_ORDEM, 0) < ordem) prefs.putUInt(K_ORDEM, ordem);
}

bool PersistenciaHidra::lerSessaoAberta(SessaoPersistida& s) {
  if (!nvs_ok_ || prefs.getBytesLength(K_SESSAO) != sizeof(SessaoPersistida)) return false;
  if (prefs.getBytes(K_SESSAO, &s, sizeof(s)) != sizeof(s)) return false;
  s.session_id[sizeof(s.session_id) - 1] = '\0';
  for (auto& a : s.alertas) a[sizeof(a) - 1] = '\0';
  if (s.n_alertas > MAX_ALERTAS_ATIVOS) s.n_alertas = MAX_ALERTAS_ATIVOS;
  return s.session_id[0] != '\0';
}

void PersistenciaHidra::salvarSessaoAberta(const SessaoPersistida& s) {
  if (nvs_ok_) prefs.putBytes(K_SESSAO, &s, sizeof(s));
}

void PersistenciaHidra::limparSessaoAberta() {
  if (nvs_ok_) prefs.remove(K_SESSAO);
}

void PersistenciaHidra::registrar(const char* linha) {
  if (!fs_ok_) return;
  File f = LittleFS.open(LOG_ATUAL, FILE_APPEND);
  if (!f) return;
  if (f.size() > LOG_MAX_BYTES) { // rotacao: mantem o arquivo atual e o anterior
    f.close();
    LittleFS.remove(LOG_ANTERIOR);
    LittleFS.rename(LOG_ATUAL, LOG_ANTERIOR);
    f = LittleFS.open(LOG_ATUAL, FILE_WRITE);
    if (!f) return;
    f.println(CABECALHO);
  }
  f.println(linha);
  f.close();
}

void PersistenciaHidra::imprimirLog(Print& saida) {
  if (!fs_ok_) { saida.println("LittleFS indisponivel"); return; }
  const char* arquivos[] = {LOG_ANTERIOR, LOG_ATUAL};
  uint8_t buf[128];
  for (const char* nome : arquivos) {
    if (!LittleFS.exists(nome)) continue;
    File f = LittleFS.open(nome, FILE_READ);
    if (!f) continue;
    while (f.available()) {
      const size_t n = f.read(buf, sizeof(buf));
      if (n == 0) break;
      saida.write(buf, n);
    }
    f.close();
  }
}

void PersistenciaHidra::limparLog() {
  if (!fs_ok_) return;
  LittleFS.remove(LOG_ANTERIOR);
  File f = LittleFS.open(LOG_ATUAL, FILE_WRITE);
  if (f) { f.println(CABECALHO); f.close(); }
}
