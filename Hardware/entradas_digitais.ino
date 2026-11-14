
#include <Arduino.h>

// Pinos dos dispositivos
const uint8_t PINO_BOTAO = 18;
const uint8_t PINO_PRESENCA = 19;

// Tempo de estabilizacao dos contatos, em milissegundos
const unsigned long DEBOUNCE_MS = 30;

struct EntradaDigital {
  uint8_t pino;
  bool estadoEstavel;
  bool ultimaLeitura;
  unsigned long instanteMudanca;
};

EntradaDigital botao = {PINO_BOTAO, HIGH, HIGH, 0};
EntradaDigital presenca = {PINO_PRESENCA, HIGH, HIGH, 0};

void atualizarEntrada(EntradaDigital &entrada, const char* nome) {
  bool leituraAtual = digitalRead(entrada.pino);
  unsigned long agora = millis();

  if (leituraAtual != entrada.ultimaLeitura) {
    entrada.instanteMudanca = agora;
  }

  if (agora - entrada.instanteMudanca >= DEBOUNCE_MS) {
    if (leituraAtual != entrada.estadoEstavel) {
      entrada.estadoEstavel = leituraAtual;

      Serial.print(nome);
      Serial.println(leituraAtual == LOW
                     ? " PRESSIONADO"
                     : " LIBERADO");
    }
  }

  entrada.ultimaLeitura = leituraAtual;
}

void setup() {
  Serial.begin(115200);

  pinMode(PINO_BOTAO, INPUT_PULLUP);
  pinMode(PINO_PRESENCA, INPUT_PULLUP);

  // Sincroniza os estados iniciais
  botao.estadoEstavel = botao.ultimaLeitura = digitalRead(PINO_BOTAO);
  presenca.estadoEstavel = presenca.ultimaLeitura = digitalRead(PINO_PRESENCA);

  Serial.println("HIDRA - Teste de entradas digitais");
}

void loop() {
  atualizarEntrada(botao, "BOTAO");
  atualizarEntrada(presenca, "PRESENCA");
}
