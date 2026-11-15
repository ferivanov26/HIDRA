#include "ConsoleHidra.h"
#include "RelogioAgenda.h"
#include "../hardware/HidraHardware.h"
#include "../iot/HidraIot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ConsoleHidra::processar() {
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      buf_[len_] = '\0';
      executar(buf_);
      len_ = 0;
    } else if (len_ < sizeof(buf_) - 1) {
      buf_[len_++] = ch;
    } else {
      len_ = 0;
      Serial.println("Comando muito longo; descartado.");
    }
  }
}

void ConsoleHidra::ajuda() {
  Serial.println(F(
    "\n=== HIDRA - console (115200, final de linha 'Nova linha') ===\n"
    " Operacao:   iniciar | cancelar | s (estado) | log | log limpar\n"
    " Agenda:     relogio AAAA-MM-DD HH:MM[:SS] | horarios 08:00 14:30 ... | horarios limpar\n"
    " Config:     cfg | set <nome> <valor> | demo | padrao\n"
    "   nomes: e0 e1 e2 e3 e4 repeticao beber sensor (segundos) | tentativas desvios volume\n"
    "          agua ocupada vazia tol copo (gramas) | fuso (minutos, -180 = SP) | chave (1/0)\n"
    " Manutencao (somente sem atividade): t (tara, plataforma VAZIA) | copo (pesa copo vazio)\n"
    "             p1..p8 (testar audio)\n"
    " Botao fisico: curto = iniciar / repetir instrucao / reconhecer alerta;"
    " longo (3 s) = cancelar"));
}

bool ConsoleHidra::exigirOcioso(const char* comando) {
  if (!c_.orq->ativo()) return true;
  Serial.printf("'%s' indisponivel durante a atividade (use 'cancelar' antes).\n", comando);
  return false;
}

void ConsoleHidra::salvar() {
  Serial.println(c_.pers->salvarParametros(*c_.param) ? "Configuracao salva na NVS."
                                                       : "ERRO ao salvar na NVS.");
}

void ConsoleHidra::executar(char* linha) {
  char* cmd = strtok(linha, " ");
  if (cmd == nullptr) return;
  char* a1 = strtok(nullptr, " ");
  const uint32_t agora = millis();

  if (!strcmp(cmd, "h") || !strcmp(cmd, "ajuda")) { ajuda(); return; }
  if (!strcmp(cmd, "s") || !strcmp(cmd, "estado")) { mostrarEstado(); return; }
  if (!strcmp(cmd, "cfg")) { mostrarParametros(); return; }

  if (!strcmp(cmd, "iniciar")) {
    if (!c_.orq->iniciarAtividade(StartTrigger::Button, c_.detector->estado(), agora))
      Serial.println("Ja existe atividade em andamento.");
    return;
  }
  if (!strcmp(cmd, "cancelar")) {
    if (c_.orq->ativo()) c_.orq->cancelar(agora); else Serial.println("Nenhuma atividade.");
    return;
  }
  if (!strcmp(cmd, "log")) {
    if (a1 && !strcmp(a1, "limpar")) { c_.pers->limparLog(); Serial.println("Registro apagado."); }
    else c_.pers->imprimirLog(Serial);
    return;
  }
  if (!strcmp(cmd, "relogio")) {
    char* a2 = strtok(nullptr, " ");
    int ano, mes, dia, h, mi, s = 0;
    if (a1 && a2 && sscanf(a1, "%d-%d-%d", &ano, &mes, &dia) == 3 &&
        sscanf(a2, "%d:%d:%d", &h, &mi, &s) >= 2 &&
        Relogio::definirLocal(ano, mes, dia, h, mi, s, c_.param->fuso_minutos)) {
      char t[24]; Relogio::formatarLocal(c_.param->fuso_minutos, t, sizeof(t));
      Serial.printf("Relogio ajustado: %s (fuso %d min)\n", t, c_.param->fuso_minutos);
    } else {
      Serial.println("Uso: relogio 2026-11-14 19:30[:00]");
    }
    return;
  }
  if (!strcmp(cmd, "horarios")) {
    char resto[80] = "";
    if (a1) {
      strncpy(resto, a1, sizeof(resto) - 1);
      for (char* t = strtok(nullptr, " "); t; t = strtok(nullptr, " ")) {
        strncat(resto, " ", sizeof(resto) - strlen(resto) - 1);
        strncat(resto, t, sizeof(resto) - strlen(resto) - 1);
      }
    }
    if (definirHorarios(resto)) salvar();
    mostrarParametros();
    return;
  }
  if (!strcmp(cmd, "set")) {
    char* a2 = strtok(nullptr, " ");
    if (!a1 || !a2) { Serial.println("Uso: set <nome> <valor>  (ver 'h')"); return; }
    if (!exigirOcioso("set")) return;
    if (definir(a1, a2)) { salvar(); if (!strcmp(a1, "volume")) c_.audio->definirVolume(c_.param->volume); }
    return;
  }
  if (!strcmp(cmd, "demo") || !strcmp(cmd, "padrao")) {
    if (!exigirOcioso(cmd)) return;
    ParametrosAtividade novo;
    if (!strcmp(cmd, "demo")) parametrosDemonstracao(novo); else parametrosPadrao(novo);
    // preserva o que e especifico da montagem/cuidador
    novo.volume = c_.param->volume;
    novo.massa_copo_g = c_.param->massa_copo_g;
    novo.fuso_minutos = c_.param->fuso_minutos;
    novo.chave_acionada_copo_na_base = c_.param->chave_acionada_copo_na_base;
    memcpy(novo.horarios_min, c_.param->horarios_min, sizeof(novo.horarios_min));
    *c_.param = novo;
    Serial.printf("Tempos do modo '%s' aplicados.\n", cmd);
    salvar();
    return;
  }
  if (!strcmp(cmd, "t")) {
    if (!exigirOcioso("t")) return;
    Serial.println("Tara: a plataforma deve estar VAZIA.");
    Serial.println(Hidra::realizarTara() ? "TARA OK" : "ERRO: HX711 nao disponivel");
    return;
  }
  if (!strcmp(cmd, "copo")) {
    if (!exigirOcioso("copo")) return;
    const EstadoFisico& f = c_.detector->estado();
    if (!f.massa_disponivel || !f.massa_estavel || f.massa_estavel_g < c_.param->limiar_ocupada_g) {
      Serial.println("Coloque o copo VAZIO na plataforma e aguarde a massa estabilizar.");
      return;
    }
    c_.param->massa_copo_g = f.massa_estavel_g;
    Serial.printf("Massa do copo vazio registrada: %.1f g\n", c_.param->massa_copo_g);
    salvar();
    return;
  }
  if (cmd[0] == 'p' && cmd[1] >= '1' && cmd[1] <= '8' && cmd[2] == '\0') {
    if (!exigirOcioso(cmd)) return;
    c_.audio->solicitar((uint8_t)(cmd[1] - '0'), agora, agora);
    return;
  }
  Serial.println("Comando invalido. Digite h.");
}

bool ConsoleHidra::definir(const char* nome, const char* valor) {
  ParametrosAtividade p = *c_.param;
  char* fim = nullptr;
  const double v = strtod(valor, &fim);
  if (fim == valor || *fim != '\0') { Serial.println("Valor numerico invalido."); return false; }
  const uint32_t ms = (uint32_t)(v * 1000.0 + 0.5);

  if      (nome[0] == 'e' && nome[1] >= '0' && nome[1] <= '4' && nome[2] == '\0')
                                        p.timeout_etapa_ms[nome[1] - '0'] = ms;
  else if (!strcmp(nome, "repeticao"))  p.repeticao_ms = ms;
  else if (!strcmp(nome, "beber"))      p.tempo_min_beber_ms = ms;
  else if (!strcmp(nome, "sensor"))     p.alerta_sensor_ms = ms;
  else if (!strcmp(nome, "tentativas")) p.max_tentativas = (uint8_t)v;
  else if (!strcmp(nome, "desvios"))    p.max_desvios = (uint8_t)v;
  else if (!strcmp(nome, "volume"))     p.volume = (uint8_t)v;
  else if (!strcmp(nome, "agua"))       p.agua_minima_g = (float)v;
  else if (!strcmp(nome, "ocupada"))    p.limiar_ocupada_g = (float)v;
  else if (!strcmp(nome, "vazia"))      p.limiar_vazia_g = (float)v;
  else if (!strcmp(nome, "tol"))        p.tolerancia_estavel_g = (float)v;
  else if (!strcmp(nome, "copo"))       p.massa_copo_g = (float)v;
  else if (!strcmp(nome, "fuso"))       p.fuso_minutos = (int16_t)v;
  else if (!strcmp(nome, "chave"))      p.chave_acionada_copo_na_base = v != 0.0;
  else { Serial.println("Parametro desconhecido (ver 'h')."); return false; }

  if (!parametrosValidos(p)) {
    Serial.println("Valor fora da faixa permitida; configuracao mantida.");
    return false;
  }
  *c_.param = p;
  Serial.printf("%s = %s\n", nome, valor);
  return true;
}

bool ConsoleHidra::definirHorarios(char* resto) {
  if (resto[0] == '\0') return false; // apenas mostra
  ParametrosAtividade p = *c_.param;
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) p.horarios_min[i] = HORARIO_VAZIO;
  if (strcmp(resto, "limpar") != 0) {
    uint8_t n = 0;
    for (char* t = strtok(resto, " "); t; t = strtok(nullptr, " ")) {
      int h, m;
      if (n >= MAX_HORARIOS || sscanf(t, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) {
        Serial.printf("Horario invalido ou excesso (max %u): %s\n", MAX_HORARIOS, t);
        return false;
      }
      p.horarios_min[n++] = (int16_t)(h * 60 + m);
    }
  }
  *c_.param = p;
  if (!Relogio::valido()) Serial.println("ATENCAO: relogio nao ajustado; horarios so funcionam apos 'relogio' ou NTP.");
  return true;
}

void ConsoleHidra::mostrarParametros() {
  const ParametrosAtividade& p = *c_.param;
  Serial.printf("Timeouts (s): E0 %.0f | E1 %.0f | E2 %.0f | E3 %.0f | E4 %.0f | repeticao %.0f\n",
                p.timeout_etapa_ms[0] / 1000.0, p.timeout_etapa_ms[1] / 1000.0,
                p.timeout_etapa_ms[2] / 1000.0, p.timeout_etapa_ms[3] / 1000.0,
                p.timeout_etapa_ms[4] / 1000.0, p.repeticao_ms / 1000.0);
  Serial.printf("Tentativas antes do alerta %u | desvios %u | volume %u | beber %.0f s | sensor %.0f s\n",
                p.max_tentativas, p.max_desvios, p.volume, p.tempo_min_beber_ms / 1000.0,
                p.alerta_sensor_ms / 1000.0);
  Serial.printf("Massa: agua_min %.0f g | ocupada %.0f g | vazia %.0f g | tol %.0f g | copo %.0f g | chave %u\n",
                p.agua_minima_g, p.limiar_ocupada_g, p.limiar_vazia_g, p.tolerancia_estavel_g,
                p.massa_copo_g, p.chave_acionada_copo_na_base);
  Serial.printf("Fuso %d min | horarios:", p.fuso_minutos);
  bool algum = false;
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) {
    if (p.horarios_min[i] == HORARIO_VAZIO) continue;
    Serial.printf(" %02d:%02d", p.horarios_min[i] / 60, p.horarios_min[i] % 60);
    algum = true;
  }
  Serial.println(algum ? "" : " (nenhum)");
}

void ConsoleHidra::mostrarEstado() {
  const Hidra::LeituraHardware l = Hidra::obterLeitura();
  const EstadoFisico& f = c_.detector->estado();
  const uint32_t agora = millis();
  char t[24]; Relogio::formatarLocal(c_.param->fuso_minutos, t, sizeof(t));
  Serial.printf("Relogio: %s | boot_id %s | uptime %lu s\n", t, hidra_boot_id(), (unsigned long)(agora / 1000));
  Serial.printf("Hardware: botao=%u chave=%u massa_valida=%u massa=%.1f g\n",
                l.botao_pressionado, l.chave_inicial_acionada, l.massa_valida, l.massa_g);
  Serial.printf("Interpretado: copo_na_base=%u plataforma_ocupada=%u estavel=%u (%.1f g)%s\n",
                f.copo_na_base, f.plataforma_ocupada, f.massa_estavel, f.massa_estavel_g,
                f.tara_suspeita ? " TARA SUSPEITA: refazer 't' com plataforma vazia" : "");
  const Orquestrador& o = *c_.orq;
  Serial.printf("Orquestrador: %s | sessao %s | estado %s (%s) | tentativas %u | alertas %u\n",
                o.ativo() ? "ATIVO" : "ocioso", o.sessaoId()[0] ? o.sessaoId() : "-",
                hidra_state_name(o.estado()), etapa(o.estado()).descricao, o.tentativas(),
                o.alertasAtivos());
  Serial.printf("LED: %s | audio: %lu enviados, ultima latencia %lu ms, maior %lu ms\n",
                LedHidra::nome(o.padraoLed(agora, f)), (unsigned long)c_.audio->enviadas(),
                (unsigned long)c_.audio->ultimaLatenciaMs(), (unsigned long)c_.audio->maiorLatenciaMs());
  const IotStatus is = iot_get_status();
  Serial.printf("IoT: wifi=%u auth=%u pendentes=%lu perdidos=%lu recusados=%lu erro='%s'\n",
                is.wifi_connected, is.authenticated, (unsigned long)is.pending_count,
                (unsigned long)is.dropped_count, (unsigned long)c_.saidas->eventosRecusadosIot(),
                is.last_error);
  Serial.printf("Persistencia: NVS=%u LittleFS=%u\n", c_.pers->nvsDisponivel(), c_.pers->logDisponivel());
}
