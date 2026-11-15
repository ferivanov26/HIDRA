# Integração das etapas 1, 2 e 3

Roteiro de entrega para outro computador: [Gravação e testes na bancada](ENTREGA_BANCADA.md).

## Firmware único

O ambiente `esp32-integrated` executa o sketch real `HIDRA_Etapa2/HIDRA_Etapa2.ino`, sua camada de hardware e o transporte Firebase. A biblioteca local é referenciada por `symlink://../../HIDRA_Etapa2`; não há cópia do orquestrador na etapa 3. O Arduino IDE continua disponível para o ensaio local da etapa 2 com stub serial.

A compilação integrada fixa Arduino ESP32 2.0.17, Espressif32 6.10.0, C++17 e HX711 0.7.5. A compilação Arduino IDE com core 3.3.x é um ambiente separado. O código do hardware foi compilado no ambiente integrado; isso não substitui sua calibração na montagem física.

A partir de `HIDRA_Etapa3/`:

```bash
source scripts/activate_local_tools.sh  # opcional: ferramentas locais desta máquina
pio run -d firmware -e esp32-integrated
pio run -d firmware -e esp32-integrated -t upload --upload-port /dev/ttyACM0
pio device monitor -p /dev/ttyACM0 -b 115200
```

Configure antes `firmware/iot/include/secrets.h`, conforme o exemplo. A partição é `default.csv`, equivalente a **Default 4MB with spiffs**, usando LittleFS. A montagem integrada não formata automaticamente. Não execute `uploadfs` sobre um diário existente. O preparo inicial está em [FIREBASE_SPARK.md](FIREBASE_SPARK.md).

## Interface e responsabilidades

O contrato canônico do produtor está em `HIDRA_Etapa2/src/contrato/HidraEvent.h` e `src/iot/HidraIot.h`. `firmware/iot/src/orchestrator_adapter.cpp` converte os sete tipos de evento, enums, indicadores de presença e textos para o transporte em `namespace HidraIoT`. A separação evita colisões entre estruturas homônimas e mantém a interface da etapa 2.

```cpp
void iot_begin(const IotConfig&);
bool iot_publish(const HidraEvent&);
void iot_update_status(const HidraStatusSnapshot&);
IotStatus iot_get_status();
uint32_t iot_last_session_order();
IotRecoveredSession iot_recovered_session();
```

- **Aquisição:** o callback físico apenas copia observações; o loop as entrega ao detector e ao orquestrador.
- **Orquestração:** decide E0–E5, áudio, alertas, sequência e encerramento. Não depende da nuvem para continuar a atividade.
- **Transporte:** valida/copia para uma fila FreeRTOS de 32 posições. Uma tarefa separada mantém até 200 eventos pendentes em LittleFS, com tamanho, CRC32 e ordem persistente, autentica e publica via HTTPS.

`iot_publish=true` significa aceitação em RAM. Não confirma Flash nem Firebase. A etapa 2 contabiliza recusas sem bloquear áudio; não oferece garantia de entrega quando a fila ou o diário saturam. Registros ainda em RAM podem ser perdidos em desligamento abrupto. Configuração e ponteiros devem permanecer válidos durante toda a execução.

## Identidade, ordem e snapshot

O adapter recebe `hidra_boot_id()` da etapa 2 na inicialização do transporte. Sessão e eventos usam o mesmo identificador de boot. O contador NVS é usado integralmente, sem truncamento em 16 bits.

O `activity_started` inclui `data.session_order`, inteiro positivo vindo da NVS. O transporte usa esse valor no resumo e conserva o maior contador persistido. Antes de uma nova atividade, o sketch eleva o contador NVS ao piso recuperado do diário; isso permite migrar do firmware de ensaio sem repetir os ordinais já utilizados. Um publicador legado sem esse campo conserva a geração local de ordem. Um dispositivo deve ter somente um publicador ativo.

`iot_update_status` copia o snapshot sob seção crítica. Além de sessão/estado, a presença inclui `activity_active`, `sequence`, `attempt`, `active_alert_count`, `mass_available` e, quando disponível e finita, `mass_g`. É enviada a cada 30 s quando há comunicação. O Android mostra ocioso com `activity_active=false`; após 90 s sem presença recebida, indica comunicação desatualizada. Massa negativa no status é diagnóstico de tara; nos eventos de atividade, massa e volume precisam ser não negativos.

## Reinicialização

NVS retém a referência da sessão ativa; o diário IoT fornece sua última referência persistida. O sketch reconcilia a sequência da mesma sessão e chama a recuperação do orquestrador antes de aceitar atividades. A etapa 2 resolve os alertas conhecidos e emite `activity_aborted/restart` com sequência continuada e o boot atual. A atividade do paciente não é retomada automaticamente.

Há uma janela entre aceitação em RAM e persistência. A recuperação não restaura eventos que nunca chegaram à Flash. Uma recusa da fila continua sendo contabilizada; ensaios de saturação e corte de energia são necessários para quantificar esses limites.

## Operação de bancada

Use final de linha **Nova linha**, 115200 baud. `s` mostra hardware, estado, boot e `wifi/auth/pendentes/perdidos/recusados/erro`. `iniciar` abre a atividade; `cancelar` a interrompe. `demo` aplica tempos de demonstração; `padrao` restaura os tempos normais. Esses comandos operam o orquestrador real, sem gerar leituras físicas artificiais.

Ligue com plataforma vazia para a tara e copo na base. Ajuste os limiares com o copo real e registre sua massa com `copo`. Confirme a polaridade da microchave; use `set chave 0` se necessário. O fio **TX do MP3 → GPIO25 pode ficar desligado**: o orquestrador não usa esse retorno, evitando aplicar seus 5 V à entrada da ESP32.

A microchave indica presença na base e não mede massa. `estimated_intake_ml` só aparece quando o copo retorna à plataforma de pesagem. A conclusão pela base não inventa volume. A faixa atual `return_cup` permanece; um áudio específico “devolva o copo na balança” exige criar o MP3 e ampliar o limite de faixas no hardware/console. Nenhuma faixa 9 foi presumida ou adicionada.

`instruction_played` registra o comando de instrução emitido. Não comprova início físico do áudio. O filtro de massa leva aproximadamente 2 s; o atraso até a decisão e o atraso UART→som devem ser medidos separadamente para RNF_LATENCIA.

## Reprodução dos testes

Na raiz do repositório: `CCACHE_DISABLE=1 bash tests/run_tests.sh`. Na pasta da etapa 3:

```bash
python3 -m unittest tests.test_orchestrator_bridge -v
bash scripts/test_all.sh
bash scripts/build_android.sh :app:testDemoDebugUnitTest :app:lintDemoDebug
```

A ponte nativa executa o orquestrador real com entradas físicas controladas e transporte substituído: ciclo pela balança, alerta/resolução, reinício e conclusão pela base sem volume. O teste REST encaminha esses eventos reais aos emuladores Auth/RTDB e consulta o resultado como cuidador. Eles não medem sensores, áudio nem perda em corte de energia. Os resultados físicos estão em [VALIDACAO.md](VALIDACAO.md).
