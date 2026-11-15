#include "DefinicaoAtividade.h"

using S = ActivityState;
using O = Observacao;
using G = Guarda;
using T = TipoRegra;
using A = AcaoRegistro;

// Semantica: Ek = ultima etapa observada. A "instrucao" de cada linha orienta
// a PROXIMA acao esperada e e emitida em timeout/repeticao.
const DefinicaoEtapa ETAPAS[6] = {
  // estado descricao                         entrada            instrucao (proxima acao)                 alerta                      massa?
  {S::E0, "atividade iniciada",              FAIXA_INICIO, "start", FAIXA_PEGAR,      "take_cup",   Motivo::NAO_INICIOU,        false},
  {S::E1, "copo retirado da base",           0, nullptr,            FAIXA_POSICIONAR, "place_cup",  Motivo::ESTAGNACAO,         true},
  {S::E2, "copo posicionado na plataforma",  0, nullptr,            FAIXA_AGUA,       "add_water",  Motivo::ESTAGNACAO,         true},
  {S::E3, "agua adicionada",                 0, nullptr,            FAIXA_RETIRAR,    "remove_cup", Motivo::ESTAGNACAO,         true},
  {S::E4, "copo cheio retirado",             0, nullptr,            FAIXA_CONTINUAR,  "return_cup", Motivo::COPO_NAO_DEVOLVIDO, false},
  {S::E5, "atividade concluida",             FAIXA_PARABENS, "completed", 0, nullptr,            nullptr,                    false},
};

// A primeira regra que casar (origem + observacao + guarda) e aplicada.
const RegraTransicao REGRAS[] = {
  // id    origem  observacao             guarda              destino tipo           acao                           faixa             instrucao     reproc descricao
  {"R01", S::E0, O::CopoSaiuDaBase,     G::Nenhuma,         S::E1, T::Avanco,     A::Nenhuma,                    0,                nullptr,      false, "copo retirado da base"},
  {"R02", S::E0, O::PlataformaOcupada,  G::CopoNaBase,      S::E0, T::Desvio,     A::Nenhuma,                    FAIXA_PEGAR,      "take_cup",   false, "objeto na plataforma com o copo ainda na base"},
  {"R03", S::E1, O::PlataformaOcupada,  G::Nenhuma,         S::E2, T::Avanco,     A::GravarMassaVazio,           0,                nullptr,      true,  "copo posicionado na plataforma"},
  {"R04", S::E1, O::CopoVoltouABase,    G::Nenhuma,         S::E0, T::Correcao,   A::Nenhuma,                    FAIXA_POSICIONAR, "place_cup",  false, "copo devolvido vazio a base antes do enchimento"},
  {"R05", S::E2, O::PlataformaOcupada,  G::AguaSuficiente,  S::E3, T::Avanco,     A::GravarMassaCheio,           0,                nullptr,      false, "copo ja chegou com agua (massa_copo conhecida)"},
  {"R06", S::E2, O::MassaEstavel,       G::AguaSuficiente,  S::E3, T::Avanco,     A::GravarMassaCheio,           0,                nullptr,      false, "aumento sustentado de massa: agua adicionada"},
  {"R07", S::E2, O::PlataformaVazia,    G::PicoIndicaAgua,  S::E3, T::Avanco,     A::GravarMassaCheioPico,       0,                nullptr,      true,  "encheu e retirou antes da massa estabilizar"},
  {"R08", S::E2, O::PlataformaVazia,    G::Nenhuma,         S::E1, T::Correcao,   A::Nenhuma,                    FAIXA_POSICIONAR, "place_cup",  false, "copo retirado da plataforma sem agua"},
  {"R09", S::E2, O::CopoVoltouABase,    G::Nenhuma,         S::E0, T::Correcao,   A::Nenhuma,                    FAIXA_POSICIONAR, "place_cup",  false, "copo voltou a base sem agua (massa nao observada)"},
  {"R10", S::E3, O::MassaEstavel,       G::Nenhuma,         S::E3, T::Atualizacao,A::AtualizarMassaCheioSeMaior, 0,                nullptr,      false, "mais agua adicionada"},
  {"R11", S::E3, O::PlataformaVazia,    G::Nenhuma,         S::E4, T::Avanco,     A::MarcarRetirada,             0,                nullptr,      false, "copo cheio retirado da plataforma"},
  {"R12", S::E3, O::CopoVoltouABase,    G::Nenhuma,         S::E4, T::Avanco,     A::MarcarRetirada,             0,                nullptr,      true,  "copo cheio levado direto a base (massa nao observada)"},
  {"R13", S::E4, O::PlataformaOcupada,  G::RetornoSemBeber, S::E3, T::Correcao,   A::GravarMassaCheio,           0,                nullptr,      false, "copo recolocado na plataforma logo apos a retirada"},
  {"R14", S::E4, O::PlataformaOcupada,  G::Nenhuma,         S::E5, T::Avanco,     A::EstimarIngestao,            0,                nullptr,      false, "copo devolvido a plataforma: estimativa de volume"},
  {"R15", S::E4, O::CopoVoltouABase,    G::Nenhuma,         S::E5, T::Avanco,     A::Nenhuma,                    0,                nullptr,      false, "copo devolvido a base"},
};

const uint8_t NUM_REGRAS = (uint8_t)(sizeof(REGRAS) / sizeof(REGRAS[0]));

const DefinicaoEtapa& etapa(ActivityState s) {
  const uint8_t i = (uint8_t)s;
  return ETAPAS[i < 6 ? i : 0];
}

const char* nomeObservacao(Observacao o) {
  switch (o) {
    case O::BotaoCurto:              return "BotaoCurto";
    case O::BotaoLongo:              return "BotaoLongo";
    case O::CopoSaiuDaBase:          return "CopoSaiuDaBase";
    case O::CopoVoltouABase:         return "CopoVoltouABase";
    case O::PlataformaOcupada:       return "PlataformaOcupada";
    case O::MassaEstavel:            return "MassaEstavel";
    case O::PlataformaVazia:         return "PlataformaVazia";
    case O::MassaEmMovimento:        return "MassaEmMovimento";
    case O::SensorMassaIndisponivel: return "SensorMassaIndisponivel";
    case O::SensorMassaRecuperado:   return "SensorMassaRecuperado";
  }
  return "?";
}
