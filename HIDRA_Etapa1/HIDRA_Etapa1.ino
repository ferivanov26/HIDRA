#include <Arduino.h>
#include <string.h>
#include "Config.h"
#include "HidraHardware.h"

// Driver de diagnostico. A Etapa 2 pode substituir este arquivo .ino e
// registrar seu proprio callback, sem alterar HidraHardware.h/.cpp.

void receberEventoFisico(const Hidra::EventoFisico& evento) {
  Serial.printf("[%lu ms] ", (unsigned long)evento.instante_ms);
  switch (evento.tipo) {
    case Hidra::TipoEventoFisico::BotaoPressionado:
      Serial.println("BOTAO_PRESSIONADO"); break;
    case Hidra::TipoEventoFisico::BotaoLiberado:
      Serial.println("BOTAO_LIBERADO"); break;
    case Hidra::TipoEventoFisico::ChaveInicialAcionada:
      Serial.println("CHAVE_INICIAL_ACIONADA"); break;
    case Hidra::TipoEventoFisico::ChaveInicialLiberada:
      Serial.println("CHAVE_INICIAL_LIBERADA"); break;
    case Hidra::TipoEventoFisico::MassaAtualizada:
      Serial.printf("MASSA_ATUALIZADA: %.2f g (valida=1)\n", evento.massa_g);
      break;
    case Hidra::TipoEventoFisico::MassaIndisponivel:
      Serial.println("MASSA_INDISPONIVEL"); break;
  }
}

void mostrarAjuda() {
  Serial.println("Comandos: p1..p8 = reproduzir; l1/l0 = LED; t = tara; s = estado; h = ajuda");
}

void executarComando(const char* comando) {
  if (comando[0] == 'p' && comando[1] >= '1' && comando[1] <= '8' && comando[2] == '\0') {
    const uint8_t faixa = (uint8_t)(comando[1] - '0');
    Serial.printf("MP3: solicitada faixa %u (%s)\n", faixa,
                  Hidra::reproduzirFaixa(faixa) ? "comando enviado" : "erro");
  } else if (strcmp(comando, "l1") == 0) {
    Hidra::definirLed(true);
    Serial.println("LED LIGADO");
  } else if (strcmp(comando, "l0") == 0) {
    Hidra::definirLed(false);
    Serial.println("LED DESLIGADO");
  } else if (strcmp(comando, "t") == 0) {
    Serial.println("Tara: deixe a plataforma VAZIA.");
    Serial.println(Hidra::realizarTara() ? "TARA OK" : "ERRO: HX711 nao disponivel");
  } else if (strcmp(comando, "s") == 0) {
    const Hidra::LeituraHardware l = Hidra::obterLeitura();
    Serial.printf("Botao=%u Chave=%u Massa_valida=%u Massa_g=%.2f Ultima_massa_ms=%lu\n",
                  l.botao_pressionado, l.chave_inicial_acionada,
                  l.massa_valida, l.massa_g,
                  (unsigned long)l.instante_massa_ms);
  } else if (strcmp(comando, "h") == 0) {
    mostrarAjuda();
  } else if (comando[0] != '\0') {
    Serial.println("Comando invalido. Digite h.");
  }
}

void lerComandosSerial() {
  static char buffer[16] = {};
  static uint8_t comprimento = 0;
  while (Serial.available()) {
    const char caractere = (char)Serial.read();
    if (caractere == '\r') continue;
    if (caractere == '\n') {
      buffer[comprimento] = '\0';
      executarComando(buffer);
      comprimento = 0;
    } else if (comprimento < sizeof(buffer) - 1) {
      buffer[comprimento++] = caractere;
    } else {
      comprimento = 0;
      Serial.println("Comando muito longo; descartado.");
    }
  }
}

void setup() {
  Serial.begin(HidraConfig::SERIAL_DIAGNOSTICO_BAUD);
  Serial.println("HIDRA - Firmware de aquisicao e atuacao v1");
  Serial.println("IMPORTANTE: deixar plataforma VAZIA durante tara inicial.");
  Hidra::iniciar(receberEventoFisico);
  mostrarAjuda();
}

void loop() {
  Hidra::atualizar();
  lerComandosSerial();
}
