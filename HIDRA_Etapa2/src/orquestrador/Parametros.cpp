#include "Parametros.h"
#include <string.h>

void parametrosPadrao(ParametrosAtividade& p) {
  memset(&p, 0, sizeof(p));
  p.versao = PARAMETROS_VERSAO;

  p.timeout_etapa_ms[0] = 60000;   // E0: nao iniciacao (pegar o copo)
  p.timeout_etapa_ms[1] = 45000;   // E1: posicionar na plataforma
  p.timeout_etapa_ms[2] = 60000;   // E2: adicionar agua
  p.timeout_etapa_ms[3] = 45000;   // E3: retirar o copo cheio
  p.timeout_etapa_ms[4] = 300000;  // E4: devolver o copo (tempo para beber)
  p.repeticao_ms        = 30000;
  p.alerta_sensor_ms    = 5000;
  p.tempo_min_beber_ms  = 5000;

  p.max_tentativas = 3;
  p.max_desvios    = 3;
  p.volume         = 30;  // valor validado pela Etapa 1 (documentacao_hardware 6.1)

  p.agua_minima_g        = 50.0f;
  p.limiar_ocupada_g     = 20.0f;
  p.limiar_vazia_g       = 10.0f;
  p.tolerancia_estavel_g = 6.0f;
  p.passo_estavel_g      = 10.0f;
  p.movimento_g          = 5.0f;
  p.tolerancia_retorno_g = 15.0f;
  p.massa_copo_g         = 0.0f;
  p.janela_estavel_ms    = 600;
  p.confirmacao_vazia_ms = 400;
  p.confirmacao_chave_ms = 300;
  p.botao_longo_ms       = 3000;
  p.chave_acionada_copo_na_base = true;

  p.fuso_minutos = -180;
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) p.horarios_min[i] = HORARIO_VAZIO;
}

void parametrosDemonstracao(ParametrosAtividade& p) {
  parametrosPadrao(p);
  p.timeout_etapa_ms[0] = 15000;
  p.timeout_etapa_ms[1] = 15000;
  p.timeout_etapa_ms[2] = 15000;
  p.timeout_etapa_ms[3] = 15000;
  p.timeout_etapa_ms[4] = 30000;
  p.repeticao_ms        = 12000;
  p.tempo_min_beber_ms  = 3000;
}

bool parametrosValidos(const ParametrosAtividade& p) {
  if (p.versao != PARAMETROS_VERSAO) return false;
  for (uint8_t i = 0; i < 5; ++i) {
    if (p.timeout_etapa_ms[i] < 3000 || p.timeout_etapa_ms[i] > 3600000UL) return false;
  }
  if (p.repeticao_ms < 3000 || p.repeticao_ms > 3600000UL) return false;
  if (p.max_tentativas < 1 || p.max_tentativas > 10) return false;
  if (p.max_desvios < 1 || p.max_desvios > 20) return false;
  if (p.volume > 30) return false;
  if (!(p.limiar_vazia_g >= 0.0f && p.limiar_vazia_g < p.limiar_ocupada_g)) return false;
  if (!(p.agua_minima_g > 0.0f) || !(p.tolerancia_estavel_g > 0.0f)) return false;
  if (!(p.massa_copo_g >= 0.0f)) return false;
  if (p.janela_estavel_ms < 200 || p.janela_estavel_ms > 5000) return false;
  if (p.fuso_minutos < -720 || p.fuso_minutos > 840) return false;
  for (uint8_t i = 0; i < MAX_HORARIOS; ++i) {
    if (p.horarios_min[i] != HORARIO_VAZIO &&
        (p.horarios_min[i] < 0 || p.horarios_min[i] > 1439)) return false;
  }
  return true;
}
