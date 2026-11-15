#pragma once
// =============================================================================
// HIDRA — Detector: converte eventos fisicos da Etapa 1 em Observacoes
// -----------------------------------------------------------------------------
// RNF_TRATAMENTO_RUIDO: confirmacao temporal da microchave, janela de
// estabilidade e histerese da massa, deteccao de botao curto/longo.
// C++ puro: recebe LeituraFisica (espelho de Hidra::EventoFisico) para que
// possa ser testado fora do ESP32.
// =============================================================================
#include "Observacoes.h"
#include "Parametros.h"

enum class EntradaFisica : uint8_t {
  BotaoPressionado, BotaoLiberado, ChaveAcionada, ChaveLiberada,
  MassaAtualizada, MassaIndisponivel
};

struct LeituraFisica {
  EntradaFisica tipo;
  uint32_t      t_ms;
  float         massa_g;
};

class Detector {
 public:
  explicit Detector(const ParametrosAtividade& p) : p_(p) {}

  // Estado inicial a partir do snapshot da Etapa 1 (Hidra::obterLeitura()).
  void iniciar(bool chave_acionada, bool botao_pressionado, uint32_t agora);
  void entrada(const LeituraFisica& e, FilaOcorrencias& saida);
  void tick(uint32_t agora, FilaOcorrencias& saida); // chamar a cada loop
  const EstadoFisico& estado() const { return fis_; }

 private:
  struct Amostra { uint32_t t; float m; };
  static constexpr uint8_t JANELA = 32;

  void novaMassa(float m, uint32_t t, FilaOcorrencias& saida);
  bool copoNaBaseDe(bool chave_acionada) const {
    return chave_acionada == p_.chave_acionada_copo_na_base;
  }

  const ParametrosAtividade& p_;
  EstadoFisico fis_ = {};

  // microchave
  bool     chave_bruta_copo_na_base_ = false;
  uint32_t t_chave_bruta_ = 0;
  // botao
  uint32_t t_botao_ = 0;
  bool     longo_emitido_ = false;
  // massa
  Amostra  janela_[JANELA] = {};
  uint8_t  ini_ = 0, n_ = 0;
  bool     tem_anterior_ = false;
  float    anterior_g_ = 0.0f;       // referencia do detector de movimento
  uint32_t t_ultimo_movimento_ = 0;
  bool     abaixo_vazia_ = false;
  uint32_t t_abaixo_vazia_ = 0;
  float    ultimo_estavel_reportado_g_ = 0.0f;
  bool     indisponibilidade_reportada_ = false;
};
