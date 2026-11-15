// =============================================================================
// HIDRA — Testes de cenario do orquestrador (executados no computador)
// -----------------------------------------------------------------------------
// Compila o MESMO codigo do firmware (contrato + detector + orquestrador) com
// g++ e simula o mundo fisico: celula de carga com o filtro exponencial da
// Etapa 1 (alfa = 0,35, amostras a cada 200 ms), microchave e botao.
//
//   cd tests && make        (ou: ./run_tests.sh)
// =============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "../HIDRA_Etapa2/src/orquestrador/Detector.h"
#include "../HIDRA_Etapa2/src/orquestrador/Orquestrador.h"

static int g_falhas = 0, g_checks = 0;
#define CHECK(cond, ...) do { ++g_checks; if (!(cond)) { ++g_falhas; \
  std::printf("    FALHOU (%s:%d): %s | ", __FILE__, __LINE__, #cond); \
  std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ---------------------------------------------------------------------------
struct EventoGravado { uint32_t t; HidraEvent ev; std::string json; };
struct AudioGravado { uint32_t t; uint8_t faixa; uint32_t gatilho; };

struct FakeSaidas : Saidas {
  uint32_t* relogio = nullptr;
  std::vector<EventoGravado> eventos;
  std::vector<AudioGravado> audios;
  std::vector<std::string> registros;
  bool sessao_aberta = false;
  SessaoPersistida sessao = {};
  uint32_t ordem = 0;
  HidraStatusSnapshot status = {};
  bool verbose = false;

  void tocarFaixa(uint8_t f, uint32_t g) override { audios.push_back({*relogio, f, g}); }
  void publicarEvento(const HidraEvent& ev) override {
    char d[512]; hidra_event_data_to_json(ev.data, d, sizeof d);
    eventos.push_back({*relogio, ev, d});
    if (verbose) std::printf("      %7.1fs #%u %-18s %s %s\n", *relogio / 1000.0, (unsigned)ev.sequence,
                             hidra_event_type_name(ev.type), hidra_state_name(ev.state), d);
  }
  void registrarLocal(const char* t) override {
    registros.push_back(t);
    if (verbose) std::printf("      %7.1fs   [local] %s\n", *relogio / 1000.0, t);
  }
  void salvarSessaoAberta(const SessaoPersistida& s) override { sessao = s; sessao_aberta = true; }
  void encerrarSessaoPersistida() override { sessao_aberta = false; }
  uint32_t proximaOrdemSessao() override { return ++ordem; }
  void statusAlterado(const HidraStatusSnapshot& s) override { status = s; }

  int contar(EventType t) const { int n = 0; for (auto& e : eventos) if (e.ev.type == t) ++n; return n; }
  const EventoGravado* ultimo(EventType t) const {
    for (auto it = eventos.rbegin(); it != eventos.rend(); ++it) if (it->ev.type == t) return &*it;
    return nullptr;
  }
  std::string trilha() const { // ex.: "E0>E1>E2"
    std::string s;
    for (auto& e : eventos) if (e.ev.type == EventType::StateChanged) {
      if (!s.empty()) s += ">";
      s += hidra_state_name(e.ev.state);
    }
    return s;
  }
  std::string faixas() const {
    std::string s; for (auto& a : audios) { if (!s.empty()) s += ","; s += std::to_string(a.faixa); } return s;
  }
};

// ---------------------------------------------------------------------------
// Simulador do mundo fisico + firmware (equivalente ao loop() do .ino)
struct Sim {
  uint32_t t = 0;
  ParametrosAtividade p;
  FakeSaidas out;
  Detector det{p};
  Orquestrador orq{p, out};
  FilaOcorrencias fila;

  // mundo fisico
  float massa_real = 0.0f, filtrada = 0.0f;
  bool filtro_ini = false, sensor_ok = true, chave = true; // copo na base
  uint32_t prox_amostra = 200, ultima_valida = 0;
  bool indisp_reportada = false;
  float ruido_g = 0.0f;
  std::mt19937 rng{42};
  // despejo
  bool despejando = false; float alvo = 0, taxa_g_ms = 0;

  explicit Sim(bool demo = false) {
    if (demo) parametrosDemonstracao(p); else parametrosPadrao(p);
    out.relogio = &t;
    det.iniciar(chave, false, 0);
  }
  void fisico(EntradaFisica tipo, float m = 0) {
    det.entrada({tipo, t, m}, fila);
  }
  void drenar() {
    Ocorrencia oc;
    while (fila.pop(oc)) orq.processar(oc, det.estado());
  }
  void passo() { // 10 ms
    t += 10;
    if (despejando) {
      massa_real += taxa_g_ms * 10;
      if (massa_real >= alvo) { massa_real = alvo; despejando = false; }
    }
    if ((int32_t)(t - prox_amostra) >= 0) {
      prox_amostra += 200;
      if (sensor_ok) {
        std::normal_distribution<float> n(0.0f, ruido_g);
        const float amostra = massa_real + (ruido_g > 0 ? n(rng) : 0.0f);
        filtrada = filtro_ini ? 0.35f * amostra + 0.65f * filtrada : amostra;
        filtro_ini = true; ultima_valida = t; indisp_reportada = false;
        fisico(EntradaFisica::MassaAtualizada, filtrada);
      } else if (!indisp_reportada && t - ultima_valida > 1500) {
        indisp_reportada = true;
        fisico(EntradaFisica::MassaIndisponivel);
      }
    }
    det.tick(t, fila);
    drenar();
    orq.tick(t, det.estado());
  }
  void avancar(uint32_t ms) { for (uint32_t i = 0; i < ms / 10; ++i) passo(); }
  void setChave(bool copo_na_base) {
    chave = copo_na_base;
    fisico(copo_na_base ? EntradaFisica::ChaveAcionada : EntradaFisica::ChaveLiberada);
  }
  void botao(uint32_t duracao_ms) {
    fisico(EntradaFisica::BotaoPressionado); drenar();
    avancar(duracao_ms);
    fisico(EntradaFisica::BotaoLiberado); drenar();
  }
  void colocar(float m) { massa_real = m; }
  void retirar() { massa_real = 0; }
  void despejar(float total_g, uint32_t duracao_ms) {
    despejando = true; alvo = massa_real + total_g; taxa_g_ms = total_g / duracao_ms;
  }
  // tempo ate a primeira ocorrencia de um evento apos t0
  long tempoAte(EventType tipo, ActivityState st, uint32_t t0) const {
    for (auto& e : out.eventos) if (e.t >= t0 && e.ev.type == tipo && e.ev.state == st) return (long)(e.t - t0);
    return -1;
  }
};

static void titulo(const char* s) { std::printf("\n== %s\n", s); }

// ---------------------------------------------------------------------------
static void teste_fluxo_normal() {
  titulo("1. Fluxo normal E0..E5 com estimativa de volume (copo 200 g, +250 g de agua, bebe 150 g)");
  Sim s; s.out.verbose = true;
  s.avancar(2000);
  s.botao(150);                              // E0
  s.avancar(4000); s.setChave(false);        // pega o copo
  uint32_t t0 = s.t;
  s.avancar(3000); s.colocar(200);           // posiciona
  uint32_t t_pos = s.t;
  s.avancar(4000); s.despejar(250, 3000);    // enche
  uint32_t t_desp = s.t;
  s.avancar(8000); s.retirar();              // retira
  uint32_t t_ret = s.t;
  s.avancar(20000); s.colocar(300);          // bebeu 150 g e devolveu na plataforma
  s.avancar(5000);

  CHECK(s.out.trilha() == "E1>E2>E3>E4>E5", "trilha=%s", s.out.trilha().c_str());
  CHECK(s.out.faixas() == "1,6", "faixas=%s (esperado so inicio e parabens)", s.out.faixas().c_str());
  const EventoGravado* c = s.out.ultimo(EventType::ActivityCompleted);
  CHECK(c && c->ev.data.has_estimated_intake_ml && std::fabs(c->ev.data.estimated_intake_ml - 150) < 12,
        "volume=%.1f", c ? c->ev.data.estimated_intake_ml : -1.0f);
  CHECK(s.out.contar(EventType::CaregiverAlert) == 0, "sem alertas");
  CHECK(!s.orq.ativo() && !s.out.sessao_aberta, "sessao encerrada e marcador limpo");
  CHECK(s.out.eventos.front().ev.type == EventType::ActivityStarted &&
        s.out.eventos.front().ev.sequence == 1, "primeiro evento activity_started seq 1");
  for (size_t i = 0; i < s.out.eventos.size(); ++i)
    CHECK(s.out.eventos[i].ev.sequence == i + 1, "sequence continua");
  long d1 = s.tempoAte(EventType::StateChanged, ActivityState::E1, t0 - 1);
  long d2 = s.tempoAte(EventType::StateChanged, ActivityState::E2, t_pos);
  long d3 = s.tempoAte(EventType::StateChanged, ActivityState::E3, t_desp);
  long d4 = s.tempoAte(EventType::StateChanged, ActivityState::E4, t_ret);
  std::printf("    latencias de deteccao: E1 %ld ms | E2 %ld ms | E3 %ld ms (apos inicio do despejo de 3 s) | E4 %ld ms\n",
              d1, d2, d3, d4);
  CHECK(d1 >= 0 && d1 <= 400, "E1 %ld", d1);
  CHECK(d2 >= 0 && d2 <= 2500, "E2 %ld", d2);
  CHECK(d4 >= 0 && d4 <= 2500, "E4 %ld", d4);
  const EventoGravado* e2 = nullptr;
  for (auto& e : s.out.eventos) if (e.ev.type == EventType::StateChanged && e.ev.state == ActivityState::E2) e2 = &e;
  CHECK(e2 && e2->ev.data.has_mass_g && std::fabs(e2->ev.data.mass_g - 200) < 8, "mass_g E2");
}

static void teste_nao_iniciacao_e_alerta() {
  titulo("2. Nao iniciacao: instrucao, 2 repeticoes, alerta ao cuidador e resolucao por progresso");
  Sim s; s.out.verbose = true;
  s.avancar(1000); s.botao(100);
  uint32_t t0 = s.t;
  s.avancar(60000 + 30000 * 3 + 2000);
  CHECK(s.out.faixas() == "1,2,2,2,8", "faixas=%s", s.out.faixas().c_str());
  CHECK(s.out.contar(EventType::CaregiverAlert) == 1, "1 alerta");
  const EventoGravado* a = s.out.ultimo(EventType::CaregiverAlert);
  CHECK(a && std::strcmp(a->ev.data.reason, "no_start") == 0 && a->ev.data.attempt == 3, "reason/attempt");
  long dt = (long)(s.out.audios[1].t - t0);
  CHECK(dt >= 60000 && dt <= 60020, "1a instrucao em %ld ms", dt);
  for (auto& au : s.out.audios) CHECK(au.t - au.gatilho <= 20, "latencia decisao->audio %u ms", au.t - au.gatilho);
  CHECK(s.orq.padraoLed(s.t, s.det.estado()) == PadraoLed::Alerta, "LED alerta");
  s.avancar(60000);
  CHECK(s.out.audios.size() == 5, "sem novos audios durante o alerta (%zu)", s.out.audios.size());
  s.setChave(false); s.avancar(1000);
  CHECK(s.out.contar(EventType::AlertResolved) == 1 &&
        std::strcmp(s.out.ultimo(EventType::AlertResolved)->ev.data.reason, "progress") == 0, "resolvido por progresso");
  CHECK(s.orq.estado() == ActivityState::E1, "E1");
}

static void teste_estagnacao_com_movimento() {
  titulo("3. Estagnacao em E2; despejo lento adia o lembrete (movimento)");
  Sim s;
  s.avancar(1000); s.botao(100); s.avancar(2000); s.setChave(false);
  s.avancar(2000); s.colocar(180); s.avancar(3000);
  CHECK(s.orq.estado() == ActivityState::E2, "E2");
  uint32_t t_e2 = s.t;
  s.avancar(50000);
  s.despejar(200, 20000);                     // despejo lento de 20 s a partir de ~53 s em E2
  s.avancar(25000);
  bool lembrete_agua = false;
  for (auto& a : s.out.audios) if (a.faixa == FAIXA_AGUA && a.t > t_e2) lembrete_agua = true;
  CHECK(!lembrete_agua, "nenhum 'coloque agua' enquanto o usuario despejava");
  CHECK(s.orq.estado() == ActivityState::E3, "E3 apos despejo (%s)", hidra_state_name(s.orq.estado()));
  // Agora nao retira o copo: lembrete 'retire o copo' (faixa 5) apos 45 s
  s.avancar(47000);
  CHECK(s.out.audios.back().faixa == FAIXA_RETIRAR, "faixa=%u", s.out.audios.back().faixa);
}

static void teste_desvio_copo_devolvido_vazio() {
  titulo("4. Desvio (exemplo da apostila): copo devolvido vazio a base antes do enchimento");
  Sim s; s.out.verbose = true;
  s.avancar(1000); s.botao(100); s.avancar(3000);
  s.setChave(false); s.avancar(3000);
  s.setChave(true);  s.avancar(1000);
  CHECK(s.orq.estado() == ActivityState::E0, "voltou a E0");
  const EventoGravado* sc = s.out.ultimo(EventType::StateChanged);
  CHECK(sc && sc->ev.data.previous_state == ActivityState::E1, "previous_state E1");
  CHECK(s.out.audios.back().faixa == FAIXA_POSICIONAR, "instrucao 'coloque o copo na plataforma'");
  // Repete o desvio mais 2 vezes => alerta por desvios
  for (int i = 0; i < 2; ++i) { s.setChave(false); s.avancar(2000); s.setChave(true); s.avancar(4000); }
  const EventoGravado* a = s.out.ultimo(EventType::CaregiverAlert);
  CHECK(a && std::strcmp(a->ev.data.reason, "deviation") == 0, "alerta por desvios");
}

static void teste_retirou_sem_agua_e_enchimento_rapido() {
  titulo("5. Copo retirado sem agua (correcao E2->E1) e enchimento+retirada rapidos (pico)");
  Sim s;
  s.avancar(1000); s.botao(100); s.avancar(2000); s.setChave(false);
  s.avancar(2000); s.colocar(200); s.avancar(3000);
  s.retirar(); s.avancar(3000);
  CHECK(s.orq.estado() == ActivityState::E1, "E1 (%s)", hidra_state_name(s.orq.estado()));
  CHECK(s.out.audios.back().faixa == FAIXA_POSICIONAR, "instrucao posicionar");
  s.colocar(200); s.avancar(3000);
  CHECK(s.orq.estado() == ActivityState::E2, "E2 de novo");
  s.despejar(220, 1500); s.avancar(1600); s.retirar(); // retira logo apos encher
  s.avancar(3000);
  CHECK(s.out.trilha() == "E1>E2>E1>E2>E3>E4", "trilha=%s", s.out.trilha().c_str());
}

static void teste_retorno_sem_beber_e_pela_base() {
  titulo("6. Recolocou o copo cheio logo apos retirar (E4->E3) e depois devolveu pela base");
  Sim s;
  s.avancar(1000); s.botao(100); s.avancar(2000); s.setChave(false);
  s.avancar(2000); s.colocar(200); s.avancar(3000);
  s.despejar(250, 2000); s.avancar(5000);
  s.retirar(); s.avancar(3000);
  s.colocar(450); s.avancar(3000);  // devolveu apos 3 s, mesma massa (nao bebeu)
  CHECK(s.orq.estado() == ActivityState::E3, "voltou a E3 (%s)", hidra_state_name(s.orq.estado()));
  s.retirar(); s.avancar(15000);
  s.setChave(true); s.avancar(1000); // devolveu na base (sem pesagem)
  const EventoGravado* c = s.out.ultimo(EventType::ActivityCompleted);
  CHECK(c && !c->ev.data.has_estimated_intake_ml, "concluida sem estimativa de volume");
  CHECK(s.out.trilha() == "E1>E2>E3>E4>E3>E4>E5", "trilha=%s", s.out.trilha().c_str());
}

static void teste_cancelamento_e_reconhecimento() {
  titulo("7. Botao curto reconhece alerta; botao curto sem alerta repete instrucao; botao longo cancela");
  Sim s(true); s.out.verbose = true; // modo demonstracao (tempos curtos)
  s.avancar(1000); s.botao(100);
  s.avancar(15000 + 12000 * 3 + 500);
  CHECK(s.out.contar(EventType::CaregiverAlert) == 1, "alerta");
  s.botao(200);
  CHECK(s.out.contar(EventType::AlertResolved) == 1 &&
        std::strcmp(s.out.ultimo(EventType::AlertResolved)->ev.data.reason, "caregiver_ack") == 0, "ack");
  size_t n = s.out.audios.size();
  s.avancar(500); s.botao(200);
  CHECK(s.out.audios.size() == n + 1 && s.out.audios.back().faixa == FAIXA_PEGAR, "repeticao a pedido");
  s.avancar(15000 + 12000 * 3 + 500);       // novo alerta
  s.botao(3500);                             // longo => cancela
  CHECK(!s.orq.ativo(), "cancelada");
  const EventoGravado* ab = s.out.ultimo(EventType::ActivityAborted);
  CHECK(ab && std::strcmp(ab->ev.data.reason, "local_cancel") == 0, "local_cancel");
  CHECK(s.out.eventos[s.out.eventos.size() - 2].ev.type == EventType::AlertResolved, "alerta resolvido antes do abort");
  CHECK(!s.out.sessao_aberta, "marcador limpo");
}

static void teste_reinicio() {
  titulo("8. Reinicio com sessao aberta => alert_resolved + activity_aborted(restart)");
  SessaoPersistida salva;
  {
    Sim s;
    s.avancar(1000); s.botao(100); s.avancar(60000 + 30000 * 3 + 1000);
    CHECK(s.out.sessao_aberta && s.out.sessao.n_alertas == 1, "sessao e alerta persistidos");
    salva = s.out.sessao;
  }
  hidra_set_boot_counter(2);
  Sim s2; s2.out.verbose = true;
  s2.orq.recuperarSessao(salva);
  CHECK(s2.out.eventos.size() == 2, "2 eventos");
  CHECK(s2.out.eventos[0].ev.type == EventType::AlertResolved && s2.out.eventos[1].ev.type == EventType::ActivityAborted, "ordem");
  CHECK(s2.out.eventos[1].ev.sequence == salva.ultima_sequencia + 2, "sequence continua (%u)", s2.out.eventos[1].ev.sequence);
  CHECK(std::strcmp(s2.out.eventos[1].ev.session_id, salva.session_id) == 0, "mesma sessao");
  s2.avancar(500); s2.botao(100);
  CHECK(std::strcmp(s2.orq.sessaoId(), "0002_s001") == 0, "nova sessao com novo boot_id: %s", s2.orq.sessaoId());
  hidra_set_boot_counter(0);
}

static void teste_sensor_indisponivel() {
  titulo("9. Celula de carga indisponivel durante a atividade");
  Sim s; s.out.verbose = true;
  s.avancar(1000); s.botao(100); s.avancar(2000); s.setChave(false); s.avancar(2000);
  s.sensor_ok = false; s.avancar(10000);
  const EventoGravado* a = s.out.ultimo(EventType::CaregiverAlert);
  CHECK(a && std::strcmp(a->ev.data.reason, "sensor_unavailable") == 0, "alerta de sensor");
  CHECK(s.out.faixas() == "1", "sem audio ao usuario: %s", s.out.faixas().c_str());
  CHECK(s.orq.padraoLed(s.t, s.det.estado()) == PadraoLed::Alerta, "LED");
  s.avancar(120000);
  CHECK(s.out.faixas() == "1", "nenhum lembrete enquanto a massa nao e observavel");
  s.sensor_ok = true; s.avancar(1000);
  CHECK(s.out.ultimo(EventType::AlertResolved) &&
        std::strcmp(s.out.ultimo(EventType::AlertResolved)->ev.data.reason, "sensor_recovered") == 0, "resolvido");
}

static void teste_sincronizacao_inicial() {
  titulo("10. Inicio com o copo fora da base e ja na plataforma (sincronizacao)");
  Sim s;
  s.setChave(false); s.avancar(1000); s.colocar(210); s.avancar(3000);
  s.botao(100);
  CHECK(s.orq.estado() == ActivityState::E2, "E2 (%s)", hidra_state_name(s.orq.estado()));
  CHECK(s.out.trilha() == "E1>E2", "trilha=%s", s.out.trilha().c_str());
}

static void teste_copo_ja_com_agua() {
  titulo("11. massa_copo_g conhecida: copo chega a plataforma ja com agua => E3");
  Sim s; s.p.massa_copo_g = 200;
  s.avancar(1000); s.botao(100); s.avancar(1000); s.setChave(false);
  s.avancar(1000); s.colocar(420); s.avancar(3000);
  CHECK(s.out.trilha() == "E1>E2>E3", "trilha=%s", s.out.trilha().c_str());
}

static void teste_ruido() {
  titulo("12. Ruido de +-3 g na celula: nenhuma transicao espuria em 10 min ocioso e em E2");
  Sim s; s.ruido_g = 2.0f;
  s.avancar(600000);
  s.botao(100); s.avancar(1000); s.setChave(false); s.avancar(1000); s.colocar(200); s.avancar(4000);
  size_t n = s.out.eventos.size();
  s.avancar(30000);
  int sc = 0; for (size_t i = n; i < s.out.eventos.size(); ++i) if (s.out.eventos[i].ev.type == EventType::StateChanged) ++sc;
  CHECK(s.orq.estado() == ActivityState::E2 && sc == 0, "estavel em E2");
}

static void teste_contrato_json() {
  titulo("13. Serializacao JSON do contrato (exemplo da apostila)");
  HidraEvent ev; hidra_event_init(ev);
  std::strcpy(ev.session_id, "b7f2_s001"); ev.sequence = 4; ev.type = EventType::StateChanged; ev.state = ActivityState::E3;
  ev.data.has_previous_state = true; ev.data.previous_state = ActivityState::E2;
  ev.data.has_mass_g = true; ev.data.mass_g = 185.0f;
  char buf[1024];
  size_t n = hidra_event_to_json(ev, "hidra_01", "b7f2_e00004", "b7f2", 1793484000000LL, 45210, buf, sizeof buf);
  std::printf("    %s\n", buf);
  CHECK(n > 0 && std::strcmp(buf,
    "{\"schema_version\":1,\"device_id\":\"hidra_01\",\"session_id\":\"b7f2_s001\",\"event_id\":\"b7f2_e00004\","
    "\"sequence\":4,\"type\":\"state_changed\",\"state\":\"E3\",\"occurred_at_ms\":1793484000000,"
    "\"boot_id\":\"b7f2\",\"uptime_ms\":45210,\"data\":{\"previous_state\":\"E2\",\"mass_g\":185.0}}") == 0, "json");
  n = hidra_event_to_json(ev, "hidra_01", "x", "b7f2", -1, 1, buf, 40);
  CHECK(n == 0 && buf[0] == '\0', "estouro recusado sem truncar");
}


static void teste_ruido_nao_adia_lembrete() {
  titulo("14. Ruido (sigma 3 g) e deriva lenta (0,05 g/s) nao adiam o lembrete de E2");
  Sim s; s.ruido_g = 3.0f;
  s.avancar(1000); s.botao(100); s.avancar(1000); s.setChave(false); s.avancar(1000); s.colocar(200); s.avancar(4000);
  CHECK(s.orq.estado() == ActivityState::E2, "E2");
  uint32_t t_e2 = s.t;
  for (int i = 0; i < 700; ++i) { s.massa_real += 0.005f; s.avancar(100); } // 70 s com deriva
  long t_lembrete = -1;
  for (auto& a : s.out.audios) if (a.faixa == FAIXA_AGUA && a.t > t_e2) { t_lembrete = (long)(a.t - t_e2); break; }
  std::printf("    lembrete 'coloque agua' %ld ms apos a entrada em E2 (timeout 60000 ms)\n", t_lembrete);
  CHECK(t_lembrete > 0 && t_lembrete < 66000, "lembrete nao foi adiado indevidamente");
}

int main() {
  teste_contrato_json();
  teste_fluxo_normal();
  teste_nao_iniciacao_e_alerta();
  teste_estagnacao_com_movimento();
  teste_desvio_copo_devolvido_vazio();
  teste_retirou_sem_agua_e_enchimento_rapido();
  teste_retorno_sem_beber_e_pela_base();
  teste_cancelamento_e_reconhecimento();
  teste_reinicio();
  teste_sensor_indisponivel();
  teste_sincronizacao_inicial();
  teste_copo_ja_com_agua();
  teste_ruido();
  teste_ruido_nao_adia_lembrete();
  std::printf("\n%d verificacoes, %d falhas\n", g_checks, g_falhas);
  return g_falhas ? 1 : 0;
}
