# HIDRA — Firmware integrado (Etapa 1 + Etapa 2)

Orquestrador de eventos e controle local do HIDRA sobre a camada de hardware da Etapa 1.
Documentação completa: [`../documentacao_orquestrador.md`](../documentacao_orquestrador.md).

## Compilar
1. Abrir `HIDRA_Etapa2.ino` na Arduino IDE (a pasta `src/` é compilada junto).
2. Placa **ESP32 Dev Module** (core Espressif 3.3.x), biblioteca **HX711 (bogde)**.
3. **Partition Scheme: "Default 4MB with spiffs"** (registro em LittleFS).
4. microSD com `/mp3/0001.mp3 … 0008.mp3`.
5. Ligar com a **plataforma vazia** e o **copo na base**. Serial 115200, "Nova linha", `h` = ajuda.

## Uso rápido
- Botão curto: inicia / repete a instrução / reconhece alerta. Botão longo (3 s): cancela.
- `demo` (tempos curtos para apresentação), `padrao` (tempos reais), `s` (estado), `log` (registro).
- `relogio 2026-11-14 19:30` + `horarios 08:00 14:00` para início por horário.

## Estrutura
- `src/hardware/` — Etapa 1, **cópia idêntica** de `HIDRA_Etapa1/` (alterar lá e copiar).
- `src/contrato/` — `HidraEvent` (contrato com a Etapa 3) e serialização JSON.
- `src/iot/` — interface `iot_*`; `HidraIotStub.cpp` imprime os eventos até a Etapa 3 existir.
- `src/orquestrador/` — C++ puro: Detector, regras E0–E5, temporizadores, alertas.
- `src/plataforma/` — áudio, LED, NVS/LittleFS, relógio/agenda, console.

## Testes no computador
`../tests/run_tests.sh` (g++): 14 cenários do orquestrador e teste de fumaça do firmware completo.
