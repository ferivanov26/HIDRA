#pragma once
#include <Arduino.h>

namespace Hidra {

// Os eventos abaixo sao observacoes de hardware, NAO transicoes E0..E5.
enum class TipoEventoFisico : uint8_t {
  BotaoPressionado,
  BotaoLiberado,
  ChaveInicialAcionada,
  ChaveInicialLiberada,
  MassaAtualizada,
  MassaIndisponivel
};

struct EventoFisico {
  TipoEventoFisico tipo;
  uint32_t instante_ms;       // millis() do ESP32, nao hora UTC
  float massa_g;              // Util APENAS se tipo == MassaAtualizada
  bool massa_valida;          // True APENAS se tipo == MassaAtualizada
};

struct LeituraHardware {
  bool botao_pressionado;     // R13-507 fisicamente pressionado
  bool chave_inicial_acionada; // KW11-7 fisicamente acionada; nao prova presenca humana
  bool massa_valida;          // Tara feita e HX711 fornecendo leituras recentes
  float massa_g;              // Massa filtrada; so consultar quando massa_valida == true
  uint32_t instante_massa_ms; // Timestamp da ultima amostra valida (millis)
};

// Callback opcional chamado durante atualizar(). Nao fazer trabalho bloqueante nele.
using ReceptorEvento = void (*)(const EventoFisico& evento);

// Uso: iniciar(receptor); chamar atualizar() continuamente dentro de loop().
void iniciar(ReceptorEvento receptor);
void atualizar();
LeituraHardware obterLeitura();

// Retorna false se HX711 nao estiver pronto. Fazer tara SOMENTE sem carga.
// Bloqueia durante aquisicao das amostras, portanto usar fora da rotina critica.
bool realizarTara();

// Comandos de atuacao: escolhidos pelo orquestrador, nunca automaticamente.
void definirLed(bool ligado);
bool reproduzirFaixa(uint8_t numero); // 1..8, /mp3/0001.mp3 etc.
void definirVolume(uint8_t nivel);    // 0..30; evitar niveis altos com falante de 0,5 W

} // namespace Hidra
