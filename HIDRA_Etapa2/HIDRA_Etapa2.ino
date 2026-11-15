// =============================================================================
// HIDRA — Firmware integrado: Etapa 1 (hardware) + Etapa 2 (orquestrador)
//          + interface da Etapa 3 (IoT, por enquanto stub serial)
// -----------------------------------------------------------------------------
// Arduino IDE: placa "ESP32 Dev Module" (core Espressif 3.3.x), biblioteca
// HX711 (bogde), Partition Scheme "Default 4MB with spiffs". Monitor Serial
// em 115200 baud com final de linha "Nova linha". Ligar com a PLATAFORMA
// VAZIA (tara automatica) e com o COPO NA BASE (microchave).
//
// Organizacao (Projeto de solucao da apostila):
//   Aquisicao   : src/hardware (Etapa 1, sem alteracoes) + Detector
//   Orquestracao: src/orquestrador (maquina de estados, regras, tempos)
//   Atuacao     : src/plataforma (audio, LED, registro, console) + src/iot
// =============================================================================
#include <Arduino.h>

#include "src/hardware/Config.h"
#include "src/hardware/HidraHardware.h"
#include "src/contrato/HidraEvent.h"
#include "src/iot/HidraIot.h"
#include "src/orquestrador/Parametros.h"
#include "src/orquestrador/Detector.h"
#include "src/orquestrador/Orquestrador.h"
#include "src/plataforma/AtuadoresHidra.h"
#include "src/plataforma/PersistenciaHidra.h"
#include "src/plataforma/RelogioAgenda.h"
#include "src/plataforma/SaidasFirmware.h"
#include "src/plataforma/ConsoleHidra.h"

// ---- Identificacao do dispositivo para a Etapa 3 -----------------------------
// Credenciais NAO devem ser versionadas (apostila, "Configuracao e diagnostico").
static const IotConfig CONFIG_IOT = {
  "hidra_01", // device_id
  "", "",     // SSID e senha (preencher localmente)
  "", "",     // URL do Realtime Database e chave do projeto
  "", ""      // e-mail e senha tecnicos do dispositivo
};

// ---- Objetos do sistema --------------------------------------------------------
ParametrosAtividade g_param;
PersistenciaHidra   g_pers;
FilaAudio           g_audio;
LedHidra            g_led;
Agenda              g_agenda;
SaidasFirmware      g_saidas(g_audio, g_pers, g_param);
Detector            g_detector(g_param);
Orquestrador        g_orq(g_param, g_saidas);
FilaOcorrencias     g_ocorrencias;
ConsoleHidra        g_console({&g_param, &g_pers, &g_audio, &g_detector, &g_orq, &g_saidas});

// ---- Eventos fisicos da Etapa 1 ------------------------------------------------
// O callback roda dentro de Hidra::atualizar(): apenas copia para um buffer;
// o processamento acontece depois, no loop (recomendacao 10.5 da Etapa 1).
namespace {
constexpr uint8_t CAP_FISICOS = 32;
Hidra::EventoFisico g_fisicos[CAP_FISICOS];
uint8_t g_fis_ini = 0, g_fis_n = 0;
uint32_t g_fis_perdidos = 0;
bool g_tara_suspeita_avisada = false;
}

void receberEventoFisico(const Hidra::EventoFisico& e) {
  if (g_fis_n == CAP_FISICOS) { ++g_fis_perdidos; return; }
  g_fisicos[(g_fis_ini + g_fis_n) % CAP_FISICOS] = e;
  ++g_fis_n;
}

static LeituraFisica converter(const Hidra::EventoFisico& e) {
  LeituraFisica l = {EntradaFisica::MassaAtualizada, e.instante_ms, e.massa_g};
  switch (e.tipo) {
    case Hidra::TipoEventoFisico::BotaoPressionado:     l.tipo = EntradaFisica::BotaoPressionado; break;
    case Hidra::TipoEventoFisico::BotaoLiberado:        l.tipo = EntradaFisica::BotaoLiberado; break;
    case Hidra::TipoEventoFisico::ChaveInicialAcionada: l.tipo = EntradaFisica::ChaveAcionada; break;
    case Hidra::TipoEventoFisico::ChaveInicialLiberada: l.tipo = EntradaFisica::ChaveLiberada; break;
    case Hidra::TipoEventoFisico::MassaAtualizada:      l.tipo = EntradaFisica::MassaAtualizada; break;
    case Hidra::TipoEventoFisico::MassaIndisponivel:    l.tipo = EntradaFisica::MassaIndisponivel; break;
  }
  return l;
}

void setup() {
  Serial.begin(HidraConfig::SERIAL_DIAGNOSTICO_BAUD);
  delay(200);
  Serial.println("\nHIDRA - firmware integrado (Etapas 1 + 2) v2.0");
  Serial.println("IMPORTANTE: plataforma VAZIA (tara) e copo na base durante a inicializacao.");

  // 1) Persistencia: boot_id, parametros do cuidador
  if (!g_pers.iniciar()) Serial.println("AVISO: NVS ou LittleFS indisponivel; registros/config podem nao persistir.");
  hidra_set_boot_counter(g_pers.incrementarBoot());
  if (!g_pers.carregarParametros(g_param)) {
    parametrosPadrao(g_param);
    g_pers.salvarParametros(g_param);
    Serial.println("Parametros padrao carregados.");
  }

  // 2) Hardware da Etapa 1 (inclui tara automatica) e atuadores
  Hidra::iniciar(receberEventoFisico);
  g_audio.iniciar(g_param.volume, millis());

  // 3) Modulo IoT (Etapa 3)
  iot_begin(CONFIG_IOT);

  // 4) Detector a partir do estado fisico atual
  const Hidra::LeituraHardware l = Hidra::obterLeitura();
  g_detector.iniciar(l.chave_inicial_acionada, l.botao_pressionado, millis());

  // 5) Sessao interrompida por queda de energia/reset => activity_aborted(restart)
  //    ANTES de aceitar uma nova atividade (apostila, "Ordenacao e reinicializacao").
  SessaoPersistida s;
  if (g_pers.lerSessaoAberta(s)) g_orq.recuperarSessao(s);

  g_saidas.registrarLocal("sistema iniciado");
  if (l.chave_inicial_acionada != g_param.chave_acionada_copo_na_base) {
    Serial.println("AVISO: o copo nao esta na base (microchave).");
  }
  g_console.ajuda();
}

void loop() {
  const uint32_t agora = millis();

  // Aquisicao (Etapa 1) -> eventos fisicos
  Hidra::atualizar();
  while (g_fis_n > 0) {
    const Hidra::EventoFisico e = g_fisicos[g_fis_ini];
    g_fis_ini = (uint8_t)((g_fis_ini + 1) % CAP_FISICOS);
    --g_fis_n;
    g_detector.entrada(converter(e), g_ocorrencias);
  }
  g_detector.tick(agora, g_ocorrencias);

  // Orquestracao
  Ocorrencia oc;
  while (g_ocorrencias.pop(oc)) g_orq.processar(oc, g_detector.estado());
  if (g_agenda.verificar(g_param, agora)) {
    g_orq.iniciarAtividade(StartTrigger::Schedule, g_detector.estado(), agora);
  }
  g_orq.tick(agora, g_detector.estado());

  // Atuacao
  g_audio.tick(agora);
  g_led.atualizar(g_orq.padraoLed(agora, g_detector.estado()), agora);

  if (g_detector.estado().tara_suspeita && !g_tara_suspeita_avisada) {
    g_tara_suspeita_avisada = true;
    g_saidas.registrarLocal("massa negativa estavel: tara feita com carga? usar 't' com a plataforma vazia");
  }
  if (g_fis_perdidos > 0) {
    Serial.printf("AVISO: %lu eventos fisicos descartados (buffer cheio)\n", (unsigned long)g_fis_perdidos);
    g_fis_perdidos = 0;
  }

  g_console.processar();
}
