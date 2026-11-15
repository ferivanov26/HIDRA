#include "Orquestrador.h"
#include <stdio.h>
#include <string.h>

namespace {
inline uint32_t desde(uint32_t agora, uint32_t t) { return agora - t; } // seguro no overflow
}

// ============================================================================
// Ciclo de vida da sessao
// ============================================================================
void Orquestrador::recuperarSessao(const SessaoPersistida& s) {
  ativo_ = false;
  hidra_copy_text(session_id_, sizeof(session_id_), s.session_id);
  sequencia_ = s.ultima_sequencia;
  estado_ = (ActivityState)(s.estado <= 5 ? s.estado : 0);

  // Alertas que estavam ativos deixam de existir com o fim da sessao.
  for (uint8_t i = 0; i < s.n_alertas && i < MAX_ALERTAS_ATIVOS; ++i) {
    HidraEvent ev; novoEvento(ev, EventType::AlertResolved);
    ev.data.has_alert_id = true; hidra_copy_text(ev.data.alert_id, sizeof(ev.data.alert_id), s.alertas[i]);
    ev.data.has_reason = true;   hidra_copy_text(ev.data.reason, sizeof(ev.data.reason), Motivo::SESSAO_ENCERRADA);
    emitir(ev);
  }
  HidraEvent ev; novoEvento(ev, EventType::ActivityAborted);
  ev.data.has_reason = true; hidra_copy_text(ev.data.reason, sizeof(ev.data.reason), Motivo::REINICIO);
  emitir(ev);

  char txt[160];
  snprintf(txt, sizeof(txt), "sessao %s interrompida por reinicio (ultimo estado %s)",
           session_id_, hidra_state_name(estado_));
  out_.registrarLocal(txt);
  out_.encerrarSessaoPersistida();
  notificarStatus();
}

bool Orquestrador::iniciarAtividade(StartTrigger gatilho, const EstadoFisico& fis, uint32_t agora) {
  if (ativo_) {
    out_.registrarLocal("inicio ignorado: ja existe atividade em andamento");
    return false;
  }
  ativo_ = true;
  estado_ = ActivityState::E0;
  concluiu_ = false;
  ++sessoes_neste_boot_;
  snprintf(session_id_, sizeof(session_id_), "%s_s%03u", hidra_boot_id(), (unsigned)sessoes_neste_boot_);
  sequencia_ = 0;
  tentativas_ = 0; desvios_ = 0;
  t_atividade_ = agora; t_ultima_instrucao_ = agora;
  tem_vazio_ = tem_cheio_ = tem_ingestao_ = false;
  massa_vazio_g_ = massa_cheio_g_ = pico_e2_g_ = ingestao_ml_ = 0.0f;
  memset(alertas_, 0, sizeof(alertas_));
  contador_alertas_ = 0;
  if (!massa_disponivel_) t_massa_indisponivel_ = agora;

  HidraEvent ev; novoEvento(ev, EventType::ActivityStarted);
  ev.data.has_trigger = true; ev.data.trigger = gatilho;
  ev.data.has_session_order = true; ev.data.session_order = out_.proximaOrdemSessao();
  emitir(ev);

  const DefinicaoEtapa& e0 = etapa(ActivityState::E0);
  instruir(e0.faixa_entrada, e0.id_entrada, 1, agora, agora);

  // Sincronizacao com o mundo fisico: o arranjo esperado e copo na base e
  // plataforma vazia. Caso contrario, as regras sao aplicadas imediatamente.
  if (!fis.copo_na_base) {
    aplicarRegras({Observacao::CopoSaiuDaBase, agora, 0.0f}, fis, 0);
  }
  if (ativo_ && fis.plataforma_ocupada && fis.massa_disponivel) {
    const float m = fis.massa_estavel ? fis.massa_estavel_g : fis.massa_atual_g;
    aplicarRegras({Observacao::PlataformaOcupada, agora, m}, fis, 0);
  }
  return true;
}

void Orquestrador::cancelar(uint32_t agora) {
  (void)agora;
  if (!ativo_) return;
  resolverAlertas(Motivo::SESSAO_ENCERRADA, false, false);
  HidraEvent ev; novoEvento(ev, EventType::ActivityAborted);
  ev.data.has_reason = true; hidra_copy_text(ev.data.reason, sizeof(ev.data.reason), Motivo::CANCELAMENTO_LOCAL);
  emitir(ev);
  out_.registrarLocal("atividade cancelada localmente (botao longo/console)");
  encerrarSessao();
}

void Orquestrador::concluir(uint32_t agora, float massa_retorno_g, bool tem_massa_retorno) {
  mudarEstado(ActivityState::E5, agora, tem_massa_retorno, massa_retorno_g);
  resolverAlertas(Motivo::SESSAO_ENCERRADA, false, false);
  const DefinicaoEtapa& e5 = etapa(ActivityState::E5);
  instruir(e5.faixa_entrada, e5.id_entrada, 1, agora, agora); // RF_REFORCO_CONCLUSAO

  HidraEvent ev; novoEvento(ev, EventType::ActivityCompleted);
  if (tem_ingestao_) {
    ev.data.has_estimated_intake_ml = true;
    ev.data.estimated_intake_ml = ingestao_ml_;
  }
  emitir(ev);
  concluiu_ = true;
  t_conclusao_ = agora;
  encerrarSessao();
}

void Orquestrador::encerrarSessao() {
  ativo_ = false;
  out_.encerrarSessaoPersistida();
  notificarStatus();
}

// ============================================================================
// Entrada de observacoes
// ============================================================================
void Orquestrador::processar(const Ocorrencia& oc, const EstadoFisico& fis) {
  ultima_massa_g_ = fis.massa_atual_g;
  if (ativo_ && estado_ == ActivityState::E2 && fis.massa_disponivel &&
      fis.massa_atual_g > pico_e2_g_) {
    pico_e2_g_ = fis.massa_atual_g;
  }

  switch (oc.tipo) {
    case Observacao::BotaoCurto:
      if (!ativo_) {
        iniciarAtividade(StartTrigger::Button, fis, oc.t_ms);
      } else if (alertasAtivos() > 0) {
        // Cuidador reconhece o alerta no proprio dispositivo.
        resolverAlertas(Motivo::CUIDADOR, false, false);
        tentativas_ = 0; desvios_ = 0;
        t_atividade_ = oc.t_ms;
        t_massa_indisponivel_ = oc.t_ms;
        out_.registrarLocal("alertas reconhecidos pelo botao; ciclo de instrucoes reiniciado");
      } else if (estado_ <= ActivityState::E4) {
        // Pedido de ajuda do usuario: repete a instrucao da etapa atual.
        const DefinicaoEtapa& e = etapa(estado_);
        instruir(e.faixa_instrucao, e.id_instrucao, tentativas_ ? tentativas_ : 1, oc.t_ms, oc.t_ms);
        out_.registrarLocal("instrucao repetida a pedido (botao)");
      }
      break;

    case Observacao::BotaoLongo:
      if (ativo_) cancelar(oc.t_ms);
      break;

    case Observacao::SensorMassaIndisponivel:
      massa_disponivel_ = false;
      t_massa_indisponivel_ = oc.t_ms;
      out_.registrarLocal("celula de carga indisponivel");
      notificarStatus();
      break;

    case Observacao::SensorMassaRecuperado:
      massa_disponivel_ = true;
      out_.registrarLocal("celula de carga recuperada");
      if (ativo_) {
        resolverAlertas(Motivo::SENSOR_RECUPERADO, false, true);
        t_atividade_ = oc.t_ms;
      }
      notificarStatus();
      break;

    case Observacao::MassaEmMovimento:
      // O usuario esta agindo (ex.: despejando agua): adia lembretes.
      if (ativo_) t_atividade_ = oc.t_ms;
      break;

    default:
      if (!ativo_) break;
      // Nova massa estavel tambem indica que o usuario esta agindo.
      if (oc.tipo == Observacao::MassaEstavel) t_atividade_ = oc.t_ms;
      aplicarRegras(oc, fis, 0);
      break;
  }
}

void Orquestrador::aplicarRegras(const Ocorrencia& oc, const EstadoFisico& fis, uint8_t profundidade) {
  if (profundidade > 3 || !ativo_) return;
  for (uint8_t i = 0; i < NUM_REGRAS; ++i) {
    const RegraTransicao& r = REGRAS[i];
    if (r.origem != estado_ || r.observacao != oc.tipo) continue;
    if (!guardaOk(r.guarda, oc, fis)) continue;

    char txt[192];
    snprintf(txt, sizeof(txt), "%s %s->%s: %s (%s, %.1f g)", r.id, hidra_state_name(r.origem),
             hidra_state_name(r.destino), r.descricao, nomeObservacao(oc.tipo), (double)oc.massa_g);
    out_.registrarLocal(txt);

    executarAcao(r.acao, oc);

    switch (r.tipo) {
      case TipoRegra::Atualizacao:
        break;

      case TipoRegra::Avanco: {
        if (r.destino == ActivityState::E5) {
          const bool com_massa = (oc.tipo == Observacao::PlataformaOcupada);
          concluir(oc.t_ms, oc.massa_g, com_massa);
          return;
        }
        const bool com_massa = r.destino >= ActivityState::E2;
        const float m = r.destino == ActivityState::E2 ? massa_vazio_g_ : massa_cheio_g_;
        mudarEstado(r.destino, oc.t_ms, com_massa, m);
        tentativas_ = 0;
        resolverAlertas(Motivo::PROGRESSO, true, false);
        break;
      }

      case TipoRegra::Correcao:
        if (r.destino != estado_) {
          const bool com_massa = r.destino == ActivityState::E3;
          mudarEstado(r.destino, oc.t_ms, com_massa, massa_cheio_g_);
        }
        tentativas_ = 0;
        if (r.faixa != 0) {
          tentativas_ = 1;
          instruir(r.faixa, r.id_instrucao, 1, oc.t_ms, oc.t_ms);
          contarDesvio(oc.t_ms);
        }
        break;

      case TipoRegra::Desvio:
        if (r.faixa != 0) {
          if (tentativas_ < 255) ++tentativas_;
          instruir(r.faixa, r.id_instrucao, tentativas_, oc.t_ms, oc.t_ms);
        }
        contarDesvio(oc.t_ms);
        break;
    }
    if (r.reprocessar) aplicarRegras(oc, fis, (uint8_t)(profundidade + 1));
    return; // somente a primeira regra aplicavel
  }
}

bool Orquestrador::guardaOk(Guarda g, const Ocorrencia& oc, const EstadoFisico& fis) const {
  switch (g) {
    case Guarda::Nenhuma:        return true;
    case Guarda::AguaSuficiente: return tem_vazio_ && oc.massa_g >= massa_vazio_g_ + p_.agua_minima_g;
    case Guarda::PicoIndicaAgua: return tem_vazio_ && pico_e2_g_ >= massa_vazio_g_ + p_.agua_minima_g;
    case Guarda::RetornoSemBeber:
      return tem_cheio_ && oc.massa_g >= massa_cheio_g_ - p_.tolerancia_retorno_g &&
             desde(oc.t_ms, t_retirada_) < p_.tempo_min_beber_ms;
    case Guarda::CopoNaBase:     return fis.copo_na_base;
  }
  return false;
}

void Orquestrador::executarAcao(AcaoRegistro a, const Ocorrencia& oc) {
  switch (a) {
    case AcaoRegistro::Nenhuma: break;
    case AcaoRegistro::GravarMassaVazio:
      // Se a massa do copo vazio for conhecida (comando "copo"), ela e usada
      // como referencia: permite reconhecer um copo que ja chegou com agua.
      massa_vazio_g_ = (p_.massa_copo_g > 0.0f &&
                        oc.massa_g >= p_.massa_copo_g - p_.tolerancia_retorno_g)
                         ? p_.massa_copo_g : oc.massa_g;
      tem_vazio_ = true;
      pico_e2_g_ = oc.massa_g;
      break;
    case AcaoRegistro::GravarMassaCheio:
      massa_cheio_g_ = oc.massa_g; tem_cheio_ = true; break;
    case AcaoRegistro::GravarMassaCheioPico:
      massa_cheio_g_ = pico_e2_g_; tem_cheio_ = true; break;
    case AcaoRegistro::AtualizarMassaCheioSeMaior:
      if (oc.massa_g > massa_cheio_g_) massa_cheio_g_ = oc.massa_g;
      break;
    case AcaoRegistro::MarcarRetirada:
      t_retirada_ = oc.t_ms; break;
    case AcaoRegistro::EstimarIngestao:
      if (tem_cheio_) {
        const float v = massa_cheio_g_ - oc.massa_g; // 1 g de agua ~ 1 ml
        ingestao_ml_ = v > 0.0f ? v : 0.0f;
        tem_ingestao_ = true;
      }
      break;
  }
}

void Orquestrador::mudarEstado(ActivityState novo, uint32_t agora, bool com_massa, float massa_g) {
  const ActivityState anterior = estado_;
  estado_ = novo;
  t_atividade_ = agora;
  if (novo == ActivityState::E2) pico_e2_g_ = massa_vazio_g_;
  HidraEvent ev; novoEvento(ev, EventType::StateChanged);
  ev.data.has_previous_state = true; ev.data.previous_state = anterior;
  if (com_massa) { ev.data.has_mass_g = true; ev.data.mass_g = massa_g; }
  emitir(ev);
}

// ============================================================================
// Temporizadores: nao iniciacao, estagnacao, repeticao e alerta
// ============================================================================
void Orquestrador::tick(uint32_t agora, const EstadoFisico& fis) {
  ultima_massa_g_ = fis.massa_atual_g;
  if (!ativo_) return;
  if (estado_ == ActivityState::E2 && fis.massa_disponivel && fis.massa_atual_g > pico_e2_g_) {
    pico_e2_g_ = fis.massa_atual_g;
  }
  if (estado_ > ActivityState::E4) return;
  const DefinicaoEtapa& e = etapa(estado_);

  if (!massa_disponivel_) {
    bool tem_sensor = false;
    for (const Alerta& a : alertas_) if (a.ativo && a.sensor) tem_sensor = true;
    if (!tem_sensor && desde(agora, t_massa_indisponivel_) >= p_.alerta_sensor_ms) {
      levantarAlerta(Motivo::SENSOR, 1, true, agora);
    }
    // Sem a celula de carga o progresso nao pode ser observado: nao insistir
    // com o usuario (RNF_NAO_INTRUSIVIDADE); o cuidador ja foi sinalizado.
    if (e.depende_massa) { t_atividade_ = agora; return; }
  }

  if (temAlertaNaoSensor()) return; // aguardando cuidador ou progresso

  if (tentativas_ == 0) {
    const uint32_t limite = p_.timeout_etapa_ms[(uint8_t)estado_];
    if (desde(agora, t_atividade_) >= limite) {
      tentativas_ = 1;
      instruir(e.faixa_instrucao, e.id_instrucao, 1, t_atividade_ + limite, agora);
    }
    return;
  }
  // Referencia = o mais recente entre a ultima instrucao e o ultimo sinal de acao.
  const uint32_t ref = desde(agora, t_ultima_instrucao_) < desde(agora, t_atividade_)
                         ? t_ultima_instrucao_ : t_atividade_;
  if (desde(agora, ref) < p_.repeticao_ms) return;
  if (tentativas_ < p_.max_tentativas) {
    ++tentativas_;
    instruir(e.faixa_instrucao, e.id_instrucao, tentativas_, ref + p_.repeticao_ms, agora);
  } else {
    levantarAlerta(e.motivo_alerta, tentativas_, false, agora);
  }
}

// ============================================================================
// Acoes
// ============================================================================
void Orquestrador::instruir(uint8_t faixa, const char* id, uint8_t tentativa,
                            uint32_t t_gatilho, uint32_t agora) {
  if (faixa == 0) return;
  out_.tocarFaixa(faixa, t_gatilho);
  HidraEvent ev; novoEvento(ev, EventType::InstructionPlayed);
  ev.data.has_instruction_id = true;
  hidra_copy_text(ev.data.instruction_id, sizeof(ev.data.instruction_id), id ? id : "unknown");
  ev.data.has_attempt = true; ev.data.attempt = tentativa;
  emitir(ev);
  t_ultima_instrucao_ = agora;
}

void Orquestrador::levantarAlerta(const char* motivo, uint8_t tentativa, bool sensor, uint32_t agora) {
  Alerta* livre = nullptr;
  for (Alerta& a : alertas_) if (!a.ativo) { livre = &a; break; }
  if (livre == nullptr) { out_.registrarLocal("limite de alertas ativos atingido"); return; }
  ++contador_alertas_;
  livre->ativo = true;
  livre->sensor = sensor;
  snprintf(livre->id, sizeof(livre->id), "a%u", (unsigned)contador_alertas_);
  hidra_copy_text(livre->motivo, sizeof(livre->motivo), motivo);

  HidraEvent ev; novoEvento(ev, EventType::CaregiverAlert);
  ev.data.has_alert_id = true; hidra_copy_text(ev.data.alert_id, sizeof(ev.data.alert_id), livre->id);
  ev.data.has_reason = true;   hidra_copy_text(ev.data.reason, sizeof(ev.data.reason), motivo);
  ev.data.has_attempt = true;  ev.data.attempt = tentativa;
  emitir(ev);

  char txt[96];
  snprintf(txt, sizeof(txt), "ALERTA %s ao cuidador: %s (tentativa %u)", livre->id, motivo, (unsigned)tentativa);
  out_.registrarLocal(txt);
  // Falha de sensor nao e culpa do usuario: apenas LED/registro, sem audio.
  if (!sensor) instruir(FAIXA_AJUDA, ID_INSTRUCAO_AJUDA, 1, agora, agora);
}

void Orquestrador::resolverAlertas(const char* motivo, bool somente_progresso, bool somente_sensor) {
  for (Alerta& a : alertas_) {
    if (!a.ativo) continue;
    if (somente_sensor && !a.sensor) continue;
    if (somente_progresso && a.sensor) continue;
    a.ativo = false; // antes de emitir: o evento ja persiste a lista sem ele
    HidraEvent ev; novoEvento(ev, EventType::AlertResolved);
    ev.data.has_alert_id = true; hidra_copy_text(ev.data.alert_id, sizeof(ev.data.alert_id), a.id);
    ev.data.has_reason = true;   hidra_copy_text(ev.data.reason, sizeof(ev.data.reason), motivo);
    emitir(ev);
  }
}

bool Orquestrador::temAlertaComMotivo(const char* motivo) const {
  for (const Alerta& a : alertas_) if (a.ativo && strcmp(a.motivo, motivo) == 0) return true;
  return false;
}

bool Orquestrador::temAlertaNaoSensor() const {
  for (const Alerta& a : alertas_) if (a.ativo && !a.sensor) return true;
  return false;
}

uint8_t Orquestrador::alertasAtivos() const {
  uint8_t n = 0;
  for (const Alerta& a : alertas_) if (a.ativo) ++n;
  return n;
}

void Orquestrador::contarDesvio(uint32_t agora) {
  if (desvios_ < 255) ++desvios_;
  if (desvios_ >= p_.max_desvios && !temAlertaComMotivo(Motivo::DESVIOS)) {
    levantarAlerta(Motivo::DESVIOS, desvios_, false, agora);
  }
}

// ============================================================================
// Eventos, persistencia e status
// ============================================================================
void Orquestrador::novoEvento(HidraEvent& ev, EventType t) const {
  hidra_event_init(ev);
  hidra_copy_text(ev.session_id, sizeof(ev.session_id), session_id_);
  ev.type = t;
  ev.state = estado_;
}

void Orquestrador::emitir(HidraEvent& ev) {
  ev.sequence = ++sequencia_;
  out_.publicarEvento(ev);
  if (ativo_) persistir();
  notificarStatus();
}

void Orquestrador::persistir() {
  SessaoPersistida s;
  memset(&s, 0, sizeof(s));
  hidra_copy_text(s.session_id, sizeof(s.session_id), session_id_);
  s.ultima_sequencia = sequencia_;
  s.estado = (uint8_t)estado_;
  for (const Alerta& a : alertas_) {
    if (a.ativo && s.n_alertas < MAX_ALERTAS_ATIVOS) {
      hidra_copy_text(s.alertas[s.n_alertas++], sizeof(s.alertas[0]), a.id);
    }
  }
  out_.salvarSessaoAberta(s);
}

HidraStatusSnapshot Orquestrador::snapshot() const {
  HidraStatusSnapshot s;
  memset(&s, 0, sizeof(s));
  s.activity_active = ativo_;
  hidra_copy_text(s.session_id, sizeof(s.session_id), session_id_);
  s.state = estado_;
  s.sequence = sequencia_;
  s.attempt = tentativas_;
  s.active_alerts = alertasAtivos();
  s.mass_available = massa_disponivel_;
  s.mass_g = ultima_massa_g_;
  return s;
}

void Orquestrador::notificarStatus() { out_.statusAlterado(snapshot()); }

PadraoLed Orquestrador::padraoLed(uint32_t agora, const EstadoFisico& fis) const {
  if (ativo_ && alertasAtivos() > 0) return PadraoLed::Alerta;
  if (!fis.massa_disponivel || fis.tara_suspeita) return PadraoLed::Falha;
  if (ativo_) return PadraoLed::Ativo;
  if (concluiu_ && desde(agora, t_conclusao_) < 3000) return PadraoLed::Concluido;
  return PadraoLed::Ocioso;
}
