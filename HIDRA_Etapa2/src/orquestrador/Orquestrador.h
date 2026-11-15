#pragma once
// =============================================================================
// HIDRA — Orquestrador de eventos (Etapa 2)
// -----------------------------------------------------------------------------
// Maquina de estados E0..E5 + temporizadores + desvios + alertas.
// C++ puro: todas as acoes saem pela interface "Saidas" (audio, eventos,
// registro, persistencia), o que permite testar a logica no computador e
// manter a separacao Aquisicao / Orquestracao / Atuacao da apostila.
// =============================================================================
#include "../contrato/HidraEvent.h"
#include "DefinicaoAtividade.h"
#include "Observacoes.h"
#include "Parametros.h"

constexpr uint8_t MAX_ALERTAS_ATIVOS = 4;

// Referencia da sessao aberta, salva na NVS a cada evento (RNF_PERSISTENCIA).
// Apos reinicio, permite emitir activity_aborted(reason=restart).
struct SessaoPersistida {
  char     session_id[64];
  uint32_t ultima_sequencia;
  uint8_t  estado;                       // ActivityState
  uint8_t  n_alertas;
  char     alertas[MAX_ALERTAS_ATIVOS][16];
};

enum class PadraoLed : uint8_t {
  Ocioso,     // pulso curto a cada 3 s: sistema ligado
  Ativo,      // 1 s aceso / 1 s apagado: aguardando etapa
  Alerta,     // piscar rapido: cuidador deve verificar
  Falha,      // piscada dupla: sensor de massa indisponivel / tara suspeita
  Concluido   // aceso continuo por 3 s apos a conclusao
};

class Saidas {
 public:
  virtual ~Saidas() {}
  // t_gatilho_ms: instante da deteccao/timeout (para medir RNF_LATENCIA).
  virtual void tocarFaixa(uint8_t faixa, uint32_t t_gatilho_ms) = 0;
  // Evento interpretado: registrar localmente E entregar ao modulo IoT.
  virtual void publicarEvento(const HidraEvent& ev) = 0;
  // Diagnostico somente local (desvios, sensor, decisoes ignoradas...).
  virtual void registrarLocal(const char* texto) = 0;
  virtual void salvarSessaoAberta(const SessaoPersistida& s) = 0;
  virtual void encerrarSessaoPersistida() = 0;
  virtual uint32_t proximaOrdemSessao() = 0; // contador persistente
  virtual void statusAlterado(const HidraStatusSnapshot& s) = 0;
};

class Orquestrador {
 public:
  Orquestrador(const ParametrosAtividade& p, Saidas& s) : p_(p), out_(s) {}

  // Boot: se havia sessao aberta, encerra-a com activity_aborted(restart).
  void recuperarSessao(const SessaoPersistida& sessao);

  // Inicia E0 (botao ou horario). Retorna false se ja houver atividade.
  bool iniciarAtividade(StartTrigger gatilho, const EstadoFisico& fis, uint32_t agora);
  void processar(const Ocorrencia& oc, const EstadoFisico& fis);
  void tick(uint32_t agora, const EstadoFisico& fis);  // chamar a cada loop
  void cancelar(uint32_t agora);                       // activity_aborted(local_cancel)

  bool          ativo() const { return ativo_; }
  ActivityState estado() const { return estado_; }
  uint8_t       tentativas() const { return tentativas_; }
  uint8_t       alertasAtivos() const;
  const char*   sessaoId() const { return session_id_; }
  HidraStatusSnapshot snapshot() const;
  PadraoLed     padraoLed(uint32_t agora, const EstadoFisico& fis) const;

 private:
  struct Alerta { bool ativo; bool sensor; char id[16]; char motivo[24]; };

  void aplicarRegras(const Ocorrencia& oc, const EstadoFisico& fis, uint8_t profundidade);
  bool guardaOk(Guarda g, const Ocorrencia& oc, const EstadoFisico& fis) const;
  void executarAcao(AcaoRegistro a, const Ocorrencia& oc);
  void mudarEstado(ActivityState novo, uint32_t agora, bool com_massa, float massa_g);
  void concluir(uint32_t agora, float massa_retorno_g, bool tem_massa_retorno);
  void encerrarSessao();

  void instruir(uint8_t faixa, const char* id, uint8_t tentativa, uint32_t t_gatilho, uint32_t agora);
  void levantarAlerta(const char* motivo, uint8_t tentativa, bool sensor, uint32_t agora);
  void resolverAlertas(const char* motivo, bool somente_progresso, bool somente_sensor);
  bool temAlertaComMotivo(const char* motivo) const;
  bool temAlertaNaoSensor() const;
  void contarDesvio(uint32_t agora);

  void emitir(HidraEvent& ev);
  void novoEvento(HidraEvent& ev, EventType t) const;
  void persistir();
  void notificarStatus();

  const ParametrosAtividade& p_;
  Saidas& out_;

  bool          ativo_ = false;
  ActivityState estado_ = ActivityState::E0;
  char          session_id_[64] = "";
  uint32_t      sequencia_ = 0;
  uint16_t      sessoes_neste_boot_ = 0;

  uint8_t  tentativas_ = 0;      // instrucoes sem progresso na etapa atual
  uint8_t  desvios_ = 0;         // desvios na sessao
  uint32_t t_atividade_ = 0;     // ultimo progresso ou sinal de acao do usuario
  uint32_t t_ultima_instrucao_ = 0;

  bool     tem_vazio_ = false, tem_cheio_ = false;
  float    massa_vazio_g_ = 0.0f, massa_cheio_g_ = 0.0f, pico_e2_g_ = 0.0f;
  uint32_t t_retirada_ = 0;
  float    ingestao_ml_ = 0.0f;
  bool     tem_ingestao_ = false;

  Alerta   alertas_[MAX_ALERTAS_ATIVOS] = {};
  uint16_t contador_alertas_ = 0;

  bool     massa_disponivel_ = true;
  uint32_t t_massa_indisponivel_ = 0;
  bool     concluiu_ = false;
  uint32_t t_conclusao_ = 0;
  float    ultima_massa_g_ = 0.0f;
};
