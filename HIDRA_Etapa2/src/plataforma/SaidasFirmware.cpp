#include "SaidasFirmware.h"
#include "RelogioAgenda.h"
#include "../iot/HidraIot.h"
#include <stdio.h>

void SaidasFirmware::tocarFaixa(uint8_t faixa, uint32_t t_gatilho_ms) {
  audio_.solicitar(faixa, t_gatilho_ms, millis());
}

void SaidasFirmware::publicarEvento(const HidraEvent& ev) {
  // 1) Registro local com data e hora (RF_REGISTRAR_EVENTOS) — independe da rede.
  char dados[320];
  if (hidra_event_data_to_json(ev.data, dados, sizeof(dados)) == 0) snprintf(dados, sizeof(dados), "{}");
  char quando[24];
  Relogio::formatarLocal(p_.fuso_minutos, quando, sizeof(quando));
  char linha[480];
  snprintf(linha, sizeof(linha), "%s;%s;%lu;%s;%lu;%s;%s;%s", quando, hidra_boot_id(),
           (unsigned long)millis(), ev.session_id, (unsigned long)ev.sequence,
           hidra_event_type_name(ev.type), hidra_state_name(ev.state), dados);
  pers_.registrar(linha);
  Serial.print("EVT ");
  Serial.println(linha);

  // 2) Entrega ao modulo IoT: copia para a fila e retorna imediatamente.
  //    A atividade local NUNCA depende desse retorno.
  if (!iot_publish(ev)) {
    ++recusados_iot_;
    Serial.println("AVISO: iot_publish recusou o evento (fila cheia ou registro invalido)");
  }
}

void SaidasFirmware::registrarLocal(const char* texto) {
  char quando[24];
  Relogio::formatarLocal(p_.fuso_minutos, quando, sizeof(quando));
  char linha[300];
  snprintf(linha, sizeof(linha), "%s;%s;%lu;-;-;local;-;%s", quando, hidra_boot_id(),
           (unsigned long)millis(), texto);
  pers_.registrar(linha);
  Serial.print("LOG ");
  Serial.println(texto);
}

void SaidasFirmware::statusAlterado(const HidraStatusSnapshot& s) {
  iot_update_status(s);
}
