#pragma once
// =============================================================================
// HIDRA — Definicao da atividade "preparar um copo de agua"
// -----------------------------------------------------------------------------
// RNF_EXTENSIBILIDADE: etapas, eventos (observacoes) e acoes (faixas de
// audio, registros) ficam descritos em TABELAS, separadas do motor do
// orquestrador. Para adaptar a outra atividade, altera-se esta definicao
// (e, se preciso, as guardas/acoes), sem reescrever o motor.
// =============================================================================
#include "../contrato/HidraEvent.h"
#include "Observacoes.h"

// ---- Faixas de audio (/mp3/000N.mp3, documentacao_hardware.md 9.2) ---------
constexpr uint8_t FAIXA_INICIO     = 1; // "Ola! Vamos beber um pouco de agua?"
constexpr uint8_t FAIXA_PEGAR      = 2; // "Pegue o copo que esta na sua frente."
constexpr uint8_t FAIXA_POSICIONAR = 3; // "Coloque o copo na plataforma indicada."
constexpr uint8_t FAIXA_AGUA       = 4; // "Agora, coloque agua no copo."
constexpr uint8_t FAIXA_RETIRAR    = 5; // "Muito bem! Agora retire o copo."
constexpr uint8_t FAIXA_PARABENS   = 6; // "Parabens! Voce concluiu a atividade."
constexpr uint8_t FAIXA_CONTINUAR  = 7; // "Vamos continuar? Ainda falta uma etapa."
constexpr uint8_t FAIXA_AJUDA      = 8; // "Se precisar de ajuda, chame alguem de confianca."
constexpr const char* ID_INSTRUCAO_AJUDA = "ask_help";

// ---- Motivos (campo reason) -------------------------------------------------
namespace Motivo {
constexpr const char* NAO_INICIOU        = "no_start";
constexpr const char* ESTAGNACAO         = "stalled";
constexpr const char* COPO_NAO_DEVOLVIDO = "cup_not_returned";
constexpr const char* DESVIOS            = "deviation";
constexpr const char* SENSOR             = "sensor_unavailable";
constexpr const char* PROGRESSO          = "progress";        // alert_resolved
constexpr const char* CUIDADOR           = "caregiver_ack";   // alert_resolved
constexpr const char* SENSOR_RECUPERADO  = "sensor_recovered";// alert_resolved
constexpr const char* SESSAO_ENCERRADA   = "session_closed";  // alert_resolved
constexpr const char* REINICIO           = "restart";         // activity_aborted
constexpr const char* CANCELAMENTO_LOCAL = "local_cancel";    // activity_aborted
}

// ---- Etapas ------------------------------------------------------------------
struct DefinicaoEtapa {
  ActivityState estado;
  const char*   descricao;
  uint8_t       faixa_entrada;     // audio ao entrar na etapa (0 = nenhum)
  const char*   id_entrada;
  uint8_t       faixa_instrucao;   // instrucao da PROXIMA acao (timeout/repeticao)
  const char*   id_instrucao;
  const char*   motivo_alerta;     // reason do caregiver_alert por falta de progresso
  bool          depende_massa;     // progresso exige a celula de carga
};

// ---- Regras de transicao -------------------------------------------------------
enum class Guarda : uint8_t {
  Nenhuma,
  AguaSuficiente,    // massa >= massa_copo_vazio + agua_minima
  PicoIndicaAgua,    // pico de massa em E2 >= massa_copo_vazio + agua_minima
  RetornoSemBeber,   // voltou rapido e com ~ a mesma massa do copo cheio
  CopoNaBase         // microchave indica copo na posicao inicial
};

enum class TipoRegra : uint8_t {
  Avanco,      // progresso esperado: zera tentativas, resolve alertas de progresso
  Correcao,    // estado volta para refletir o mundo fisico (state_changed)
  Desvio,      // evento incompativel sem mudanca de estado
  Atualizacao  // apenas atualiza dados internos (sem evento)
};

enum class AcaoRegistro : uint8_t {
  Nenhuma,
  GravarMassaVazio,          // massa de referencia do copo vazio
  GravarMassaCheio,          // massa do copo cheio = massa observada
  GravarMassaCheioPico,      // massa do copo cheio = pico observado em E2
  AtualizarMassaCheioSeMaior,
  MarcarRetirada,            // instante em que o copo cheio saiu da plataforma
  EstimarIngestao            // volume = massa cheio - massa no retorno
};

struct RegraTransicao {
  const char*   id;
  ActivityState origem;
  Observacao    observacao;
  Guarda        guarda;
  ActivityState destino;
  TipoRegra     tipo;
  AcaoRegistro  acao;
  uint8_t       faixa;          // instrucao emitida pela regra (0 = nenhuma)
  const char*   id_instrucao;
  bool          reprocessar;    // reaplica a mesma observacao no novo estado
  const char*   descricao;
};

extern const DefinicaoEtapa ETAPAS[6];
extern const RegraTransicao REGRAS[];
extern const uint8_t        NUM_REGRAS;

const DefinicaoEtapa& etapa(ActivityState s);
