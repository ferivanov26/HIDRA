# HIDRA — Etapa 3: IoT, Firebase e Android

Módulo de comunicação e consulta dos eventos da atividade. O processamento dos
sensores e as decisões de áudio permanecem no orquestrador local. Esta pasta
segue a divisão por etapas do repositório e contém projetos independentes para
o ESP32, o computador e o Android.

## Estrutura

| Caminho | Responsabilidade |
|---|---|
| `firmware/iot/` | Interface C++, fila FreeRTOS, diário LittleFS e comunicação HTTPS |
| `firebase/` | Regras RTDB, índices, configuração dos emuladores e testes de acesso |
| `simulator/` | Contrato v1, cenários fictícios, diário SQLite e cliente REST |
| `android/` | Aplicativo Kotlin/Compose, repositórios, ViewModel e Gradle Wrapper |
| `tests/` | Testes Python e exemplos de sessões |
| `scripts/` | Compilação, testes, geração de regras e preparação do emulador |
| `docs/` | Contrato de dados e interface de integração |

## Ambiente

- Python 3.10 ou posterior: simulador sem dependências adicionais.
- Node.js 22 e JDK 21: Firebase CLI 15.34.2 e emuladores.
- JDK 17, SDK Android 35 e Build Tools 34.0.0: aplicativo Android, API mínima 26.
- PlatformIO 6.2.0: plataforma Espressif32 6.10.0 e ArduinoJson 7.3.0.
- G++ com C++17: cinco testes do programa de demonstração no computador.
- Graphviz: regeneração opcional dos diagramas técnicos.

Os scripts aceitam `HIDRA_FIREBASE_JAVA_HOME` e `HIDRA_ANDROID_JAVA_HOME` para
selecionar os JDKs. Comandos diretos de npm/Gradle usam `JAVA_HOME` e `PATH`.
Configure o SDK em `ANDROID_HOME` ou em `android/local.properties`.

**Todos os comandos abaixo partem desta pasta:**

```bash
cd HIDRA_Etapa3
```

Nesta máquina, as ferramentas utilizadas na revisão estão instaladas em
`.runtime/tools/`. Para selecioná-las em um novo terminal:

```bash
source scripts/activate_local_tools.sh
```

Em outro computador, instale as dependências acima. O script somente seleciona
ferramentas locais existentes; não instala o SDK nem aceita licenças.

## Simulador sem hardware

```bash
python3 -m unittest tests.test_simulator -v
python3 -m simulator --scenario normal
python3 -m simulator --scenario alert --output .runtime/alert.json
python3 -m simulator --scenario interrupted --output .runtime/interrupted.json
```

Os cenários produzem, respectivamente, sete, dez e cinco eventos. O resultado
final é `completed` nos dois primeiros e `aborted` no terceiro. São dados
fictícios, sem reconhecimento de sensores. A CLI grava arquivos de execução
em `.runtime/`, ignorado pelo Git.

## Firebase local

No terminal com JDK 21:

```bash
npm --prefix firebase ci
npm --prefix firebase run emulators -- \
  --export-on-exit "$PWD/.runtime/emulators"
```

O projeto fictício `demo-hidra` utiliza Auth na porta 9099, RTDB na 9000 e
interface local em `http://127.0.0.1:4000`. Em outro terminal, também nesta pasta:

```bash
python3 scripts/seed_emulator.py
export FIREBASE_AUTH_EMULATOR_HOST=127.0.0.1:9099
export HIDRA_DATABASE_URL='http://127.0.0.1:9000?ns=demo-hidra-default-rtdb'
export HIDRA_API_KEY=demo-api-key
export HIDRA_DEVICE_EMAIL=device@hidra.test
export HIDRA_DEVICE_PASSWORD=hidra-demo-123
python3 -m simulator --scenario alert --publish
```

Para uma apresentação com pausa no alerta ativo, execute
`python3 scripts/demo_emulator.py --scenario alert` após o provisionamento.
Pressione Enter no terminal para resolver o alerta e concluir a sessão.

O provisionamento é exclusivo do emulador e regrava a área fictícia. Execute-o
antes da sessão de teste. A conta de consulta é `caregiver@hidra.test`, com a
mesma senha fictícia. O publicador e o cuidador usam identidades comuns;
somente a preparação administrativa utiliza privilégios do emulador.

Para retomar o servidor após encerramento com Ctrl+C, preservando as contas
e os dados exportados:

```bash
npm --prefix firebase run emulators -- \
  --import "$PWD/.runtime/emulators" \
  --export-on-exit "$PWD/.runtime/emulators"
```

Com as variáveis anteriores, `python3 -m simulator --publish --resume` envia
somente pendências do diário. Use diários diferentes para projetos ou
identidades diferentes, por meio de `--journal`.

## Aplicativo Android

Abra `android/` no Android Studio ou compile com JDK 17:

```bash
bash scripts/build_android.sh
```

| Variante | Origem dos dados |
|---|---|
| `demoDebug` | Repositório fictício, sem conta Firebase |
| `emulatorDebug` | Auth e RTDB locais |
| `cloudDebug` | Projeto Firebase real, configurado pelo desenvolvedor |

Para usar o emulador, copie `android/firebase.example.properties` para
`android/firebase.local.properties`. O host padrão `10.0.2.2` atende ao Android
Emulator. Em telefone por USB, use `127.0.0.1`, recompile e configure:

```bash
adb reverse tcp:9099 tcp:9099
adb reverse tcp:9000 tcp:9000
```

Os APKs são gerados em `android/app/build/outputs/apk/<variante>/debug/`. Exemplo
de instalação da demonstração:

```bash
adb install -r android/app/build/outputs/apk/demo/debug/app-demo-debug.apk
adb shell am start -n br.usp.hidra.demo/br.usp.hidra.MainActivity
```

O aplicativo permite acompanhamento, histórico paginado e detalhes das sessões.
Alertas são consultados com o aplicativo aberto; não há push ou comandos
remotos. Conclusão observável e estimativa de volume não confirmam ingestão.

## ESP32

```bash
pio run -d firmware -e esp32dev
pio run -d firmware -e esp32-synthetic
pio run -d firmware -e esp32-network-probe
```

Copie `firmware/iot/include/secrets.example.h` para `secrets.h` e preencha a
rede, a identidade técnica e o Firebase. O bundle de certificados públicos
está em `firebase_ca.h`; o arquivo de credenciais permanece fora do Git.

O LittleFS precisa estar preparado; a montagem não formata automaticamente
registros existentes. O ambiente `esp32-synthetic` gera uma sessão fictícia
quando recebe `n` na Serial, em 115200 baud. O firmware utiliza HTTPS para a
nuvem; o fluxo com Firebase Emulator é executado pelo simulador Python.

Na demonstração, `s` apresenta conexão, autenticação, pendências e perdas.
Uma falha de inicialização ou recuperação impede abrir novas sessões. Se a
fila recusar um evento, o gerador conserva sessão e sequência e tenta novamente.
O snapshot é atualizado a cada evento aceito. O diagnóstico `esp32-network-probe`
verifica Wi-Fi, UTC, TLS e montagem do LittleFS sem publicar eventos.

A integração física com o Firebase foi validada em 15/11/2026: duas sessões
fictícias de sete eventos, incluindo reconexão após reinício, foram confirmadas
na nuvem e lidas com a conta do cuidador. A placa de bancada permanece com
`esp32-synthetic`; `hidra_01` está identificado como `simulator`. Não execute
novamente a preparação do LittleFS para repetir esse ensaio.

A configuração na nuvem, as contas, os vínculos e a preparação inicial do
LittleFS estão em [FIREBASE_SPARK.md](docs/FIREBASE_SPARK.md). O projeto pode
usar somente o plano gratuito Spark, sem vincular faturamento.

A [interface com o orquestrador](docs/INTEGRACAO.md) descreve inicialização,
publicação, snapshots e recuperação. A etapa 1 usa outro core ESP32; as duas
aplicações não são compiladas juntas automaticamente.

## Testes e regras

Com JDK 21, dependências npm instaladas e portas dos emuladores livres:

```bash
bash scripts/test_all.sh
```

O script executa 14 testes Python do simulador, cinco testes do programa C++
de demonstração, sete testes de regras e quatro testes REST.
Os testes REST preparam os dados locais novamente. Não execute outra
instância dos emuladores ou uma coleta de dados nessas portas ao mesmo tempo.

Com JDK 17:

```bash
bash scripts/build_android.sh :app:testDemoDebugUnitTest :app:lintDemoDebug
```

São quatro testes JVM de ViewModel. Relatórios ficam em
`android/app/build/reports/`. Esses testes não executam as telas em aparelho.

As regras versionadas podem ser regeneradas com:

```bash
python3 scripts/generate_rules.py
```

Após alterar o contrato ou as regras, execute as suítes correspondentes.
O [contrato de eventos](docs/CONTRATO.md) documenta os campos, os caminhos do
banco e o protocolo de sincronização.

Os [resultados da validação](docs/VALIDACAO.md) distinguem testes locais,
emuladores e diagnóstico físico. Os diagramas técnicos em `docs/diagramas/`
podem ser regenerados com `python3 scripts/render_diagrams.py`.
