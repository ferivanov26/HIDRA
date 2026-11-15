#pragma once
// =============================================================================
// HIDRA — Observacoes significativas (saida do Detector, entrada do
// Orquestrador). Correspondem a camada "Aquisicao: transformar medicoes
// fisicas em eventos significativos" do Projeto de solucao da apostila.
// =============================================================================
#include <stdint.h>

enum class Observacao : uint8_t {
  BotaoCurto,              // botao pressionado e solto em menos de botao_longo_ms
  BotaoLongo,              // botao mantido por botao_longo_ms
  CopoSaiuDaBase,          // microchave confirmou ausencia do copo na base
  CopoVoltouABase,         // microchave confirmou copo na base
  PlataformaOcupada,       // massa estavel >= limiar_ocupada (massa_g)
  MassaEstavel,            // nova massa estavel com plataforma ocupada (massa_g)
  PlataformaVazia,         // massa <= limiar_vazia por confirmacao_vazia_ms
  MassaEmMovimento,        // massa variando (usuario agindo; adia lembretes)
  SensorMassaIndisponivel, // HX711 sem leituras validas
  SensorMassaRecuperado    // HX711 voltou a fornecer leituras
};

const char* nomeObservacao(Observacao o);

struct Ocorrencia {
  Observacao tipo;
  uint32_t   t_ms;     // millis() do instante da deteccao
  float      massa_g;  // significativo para PlataformaOcupada / MassaEstavel
};

// Estado fisico interpretado (consultado pelo orquestrador)
struct EstadoFisico {
  bool  copo_na_base;
  bool  plataforma_ocupada;
  bool  massa_disponivel;
  float massa_atual_g;      // ultima amostra filtrada recebida
  bool  massa_estavel;
  float massa_estavel_g;    // media da janela quando estavel
  bool  tara_suspeita;      // massa estavel muito negativa => tara feita com carga
  bool  botao_pressionado;
};

// Fila circular pequena e sem alocacao dinamica
class FilaOcorrencias {
 public:
  static constexpr uint8_t CAPACIDADE = 16;
  bool push(const Ocorrencia& o) {
    if (n_ == CAPACIDADE) { ++perdidas_; return false; }
    itens_[(ini_ + n_) % CAPACIDADE] = o; ++n_; return true;
  }
  bool pop(Ocorrencia& o) {
    if (n_ == 0) return false;
    o = itens_[ini_]; ini_ = (uint8_t)((ini_ + 1) % CAPACIDADE); --n_; return true;
  }
  uint8_t  tamanho() const { return n_; }
  uint32_t perdidas() const { return perdidas_; }
 private:
  Ocorrencia itens_[CAPACIDADE] = {};
  uint8_t ini_ = 0, n_ = 0;
  uint32_t perdidas_ = 0;
};
