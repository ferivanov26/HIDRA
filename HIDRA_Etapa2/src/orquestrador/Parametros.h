#pragma once
// =============================================================================
// HIDRA — Parametros configuraveis pelo cuidador (RF_CONFIGURAR_PARAMETROS)
// -----------------------------------------------------------------------------
// Todos os tempos, limiares, numero de repeticoes, volume e horarios ficam
// aqui, separados da logica do orquestrador. Os valores sao salvos na NVS do
// ESP32 (RNF_PERSISTENCIA) e alterados pelo console serial (comando "set").
// =============================================================================
#include <stdint.h>

constexpr uint16_t PARAMETROS_VERSAO = 1;
constexpr uint8_t  MAX_HORARIOS = 6;
constexpr int16_t  HORARIO_VAZIO = -1;

struct ParametrosAtividade {
  uint16_t versao;

  // ---- Temporizadores (ms) --------------------------------------------------
  // Tempo sem progresso em cada etapa ate a 1a instrucao. Indice = E0..E4.
  // E0 = nao iniciacao; E1..E3 = estagnacao; E4 = copo nao devolvido.
  uint32_t timeout_etapa_ms[5];
  uint32_t repeticao_ms;          // intervalo entre repeticoes da instrucao
  uint32_t alerta_sensor_ms;      // celula indisponivel por este tempo => alerta
  uint32_t tempo_min_beber_ms;    // retorno a plataforma antes disso e com a
                                  // mesma massa => "recolocou", nao "bebeu"

  // ---- Tentativas ----------------------------------------------------------
  uint8_t  max_tentativas;        // instrucoes sem progresso antes do alerta
  uint8_t  max_desvios;           // desvios na sessao antes do alerta

  // ---- Audio ---------------------------------------------------------------
  uint8_t  volume;                // 0..30 (montagem atual validada em 30)

  // ---- Interpretacao da massa (g) -----------------------------------------
  float    agua_minima_g;         // aumento minimo para considerar "agua adicionada"
  float    limiar_ocupada_g;      // massa estavel >= isto => plataforma ocupada
  float    limiar_vazia_g;        // massa <= isto => plataforma vazia (histerese)
  float    tolerancia_estavel_g;  // variacao maxima na janela para "estavel"
  float    passo_estavel_g;       // mudanca minima para nova "massa estavel"
  float    movimento_g;           // salto entre amostras => usuario agindo
  float    tolerancia_retorno_g;  // margem para "copo voltou com a mesma massa"
  float    massa_copo_g;          // massa do copo vazio (0 = desconhecida)
  uint32_t janela_estavel_ms;     // janela de estabilidade
  uint32_t confirmacao_vazia_ms;  // tempo abaixo de limiar_vazia para confirmar
  uint32_t confirmacao_chave_ms;  // tempo da microchave estavel para confirmar
  uint32_t botao_longo_ms;        // pressionamento longo => cancelar atividade
  bool     chave_acionada_copo_na_base; // true: microchave acionada = copo na base

  // ---- Agenda --------------------------------------------------------------
  int16_t  fuso_minutos;          // -180 = Sao Paulo (UTC-3)
  int16_t  horarios_min[MAX_HORARIOS]; // minutos do dia (0..1439) ou HORARIO_VAZIO
};

void parametrosPadrao(ParametrosAtividade& p);        // uso real
void parametrosDemonstracao(ParametrosAtividade& p);  // tempos curtos p/ apresentacao
bool parametrosValidos(const ParametrosAtividade& p);
