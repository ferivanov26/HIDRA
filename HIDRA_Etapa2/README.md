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
- `src/iot/` — interface `iot_*`; `HidraIotStub.cpp` imprime eventos no modo Arduino IDE; o ambiente integrado usa o adapter real da etapa 3.
- `src/orquestrador/` — C++ puro: Detector, regras E0–E5, temporizadores, alertas.
- `src/plataforma/` — áudio, LED, NVS/LittleFS, relógio/agenda, console.

## Testes no computador
`../tests/run_tests.sh` (g++): 14 cenários do orquestrador e teste de fumaça do firmware completo.

## Firmware com Firebase

Na raiz do repositório, configure `HIDRA_Etapa3/firmware/iot/include/secrets.h` e execute:

```bash
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated -t upload
```

O PlatformIO compila esta pasta como biblioteca local, desativa o stub e usa um único setup/loop. O LittleFS compartilhado não é formatado automaticamente. Não grave uma imagem de filesystem sobre os registros existentes. Veja [integração e operação de bancada](../HIDRA_Etapa3/docs/INTEGRACAO.md).

Calibre com o copo real (`copo`) e confirme a microchave (`set chave 0` se invertida). O TX do MP3 para GPIO25 pode permanecer desligado. A conclusão pela base não fornece volume; para estimar, devolva o copo à plataforma. Um áudio 9 específico ainda exige gravação e alteração do limite de faixas.
