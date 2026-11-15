# Validação da etapa 3

Revisão de 14/11/2026, branch `iot_firebase_mobile`, sobre a base `4c6ac66` com as alterações locais desta revisão. Os resultados abaixo correspondem a execuções realizadas; não representam homologação do conjunto das etapas 1, 2 e 3.

## Resultados

| Camada | Execução | Resultado |
|---|---|---|
| Contrato e diário SQLite | `python3 -m unittest tests.test_simulator -v` | 14 testes aprovados |
| Demonstração C++ real, com transporte substituído | `python3 -m unittest tests.test_firmware_demo -v` | 5 testes aprovados |
| Regras RTDB, clientes sem privilégio administrativo | `npm --prefix firebase test` | 7 testes aprovados |
| Auth + RTDB REST nos emuladores | Comando de `scripts/test_all.sh` | 4 testes aprovados |
| ViewModel Android | `:app:testDemoDebugUnitTest` | 4 testes, zero falhas/erros |
| Android lint | `:app:lintDemoDebug` | Zero erros; 5 avisos de versões de dependências |
| APKs | `demoDebug`, `emulatorDebug`, `cloudDebug` | Três APKs compilados |
| Firmware | `esp32dev`, `esp32-synthetic`, `esp32-network-probe` | Três compilações aprovadas |
| Imagem de filesystem | `pio run -d firmware -e esp32dev -t buildfs` | LittleFS gerado, sem gravação na placa |
| ESP32 física | Diagnóstico em `/dev/ttyACM0` | Wi-Fi, UTC e HTTPS com certificado aprovados |

Total: **34 testes automatizados aprovados**. Os cinco testes C++ compilam o `main.cpp` real com um substituto do transporte; não executam a implementação LittleFS/HTTPS de `iot.cpp` no computador.

## Ambiente efetivamente usado

- Python 3.10; G++ com C++17.
- Node.js 22.16.0; Firebase CLI 15.34.2; JDK 21 local.
- Gradle Wrapper 8.9; SDK Android 35; Build Tools 34.0.0.
- PlatformIO 6.2.0; Espressif32 6.10.0; Arduino ESP32 2.0.17; ArduinoJson 7.3.0.
- ESP32-D0WD-V3, revisão 3.1, Flash 4 MB.

O projeto Android tem alvo Java/Kotlin 17; a execução desta revisão utilizou JDK 21. Os APKs `cloud` foram compilados sem projeto real configurado, apresentando a mensagem de configuração pendente. Compilar essa variante não comprova acesso à nuvem.

## Evidência física

Trecho do diagnóstico real, sem credenciais:

```text
HIDRA NETWORK PROBE chip=ESP32-D0WD-V3 revision=3 flash=4194304
littlefs=não preparado (sem formatação automática)
wifi=ok rssi=-74
utc=ok
tls=ok http=404
Fim do diagnóstico; autenticação e RTDB exigem projeto configurado.
```

A resposta HTTP 404 foi recebida da raiz de `identitytoolkit.googleapis.com`, após conexão TLS validada. Ela comprova conectividade HTTPS, não login nem publicação de eventos. O LittleFS não montou e não foi formatado. A Flash original de 4 MB foi salva em `.runtime/esp32-antes-dos-ensaios.bin` antes do ensaio. A Flash foi restaurada após o diagnóstico, com verificação de hash; a execução está registrada em `.runtime/restauracao-esp32.txt`.

## Cobertura dos novos testes C++

- Sessão normal com sete eventos, sequências contíguas e identificadores de sessão distintos.
- Falha de inicialização bloqueia emissão mesmo quando existe `boot_id`.
- Sessão anterior aberta é interrompida antes de abrir nova atividade.
- Falha na emissão dessa interrupção impede nova sessão até a tentativa ser aceita.
- Recusa de publicação durante a sessão preserva evento/ordem e impede outra sessão concorrente.

## Limites desta validação

- Não existe projeto Firebase real provisionado nesta revisão. Não houve publicação em nuvem.
- Não houve teste de corte de energia ou recuperação do diário LittleFS na placa.
- Nenhum Android foi encontrado por `adb devices`; APKs não foram executados em aparelho nesta revisão.
- Não foram medidas atualização remota em até 5 s nem latência física de áudio inferior a 2 s.
- O firmware da etapa 1 e o módulo IoT ainda são projetos independentes; a máquina de estados da etapa 2 não foi encontrada na branch analisada.
- Os testes de falha de rede/reinício do simulador verificam SQLite e o algoritmo desktop. Seus resultados não substituem ensaios LittleFS/FreeRTOS/ESP32.
- Eventos aceitos na RAM ainda podem ser perdidos antes da persistência; a janela precisa ser medida fisicamente.

## Reprodução

Siga o README para selecionar Java/SDK e instalar dependências. A partir de `HIDRA_Etapa3/`:

```bash
bash scripts/test_all.sh
bash scripts/build_android.sh :app:testDemoDebugUnitTest :app:lintDemoDebug
pio run -d firmware -e esp32dev -e esp32-synthetic -e esp32-network-probe
pio run -d firmware -e esp32dev -t buildfs
```

Os arquivos de execução desta máquina ficam em `.runtime/` e os relatórios Android em `android/app/build/reports/`, ambos ignorados pelo Git. Configuração em nuvem e primeiro preparo da placa: [FIREBASE_SPARK.md](FIREBASE_SPARK.md).

## Validação complementar no Firebase real — 15/11/2026

Após o provisionamento do projeto e a correção do e-mail do cuidador, o teste REST no banco real aprovou 13 verificações. Essas verificações complementam os testes locais da revisão anterior; não são testes do firmware na placa ou das telas Android.

- Autenticação do simulador e do cuidador aprovada.
- Simulador acessa somente seu dispositivo; leitura do equipamento físico e listagem global recusadas.
- Cuidador consulta seus vínculos e lê a sessão autorizada.
- Dez eventos publicados em ordem e resumo `completed`, com uma instrução, um alerta e nenhum alerta ativo após resolução.
- Reenvio preserva o primeiro timestamp de recebimento; sobrescrita do evento é recusada.
- Presença e estado E5 do simulador confirmados; cuidador não pode alterar o estado.

Sessão desta repetição: `cloudcheck_7710484c8e9b44d4_s001`, em `devices/sim_hidra_01/sessions/`, com `session_order=2`. Os dados fictícios foram mantidos para inspeção no Console. Nenhum registro de `hidra_01` foi alterado.

O registro sanitizado da execução está em `.runtime/cloud-validation.json`. Credenciais e tokens não foram incluídos na documentação. A integração ESP32 → nuvem e a execução do APK continuam pendentes.

## Integração física ESP32 → Firebase — 15/11/2026

O firmware `esp32-synthetic` foi compilado e gravado na ESP32-D0WD-V3 em `/dev/ttyACM0`, utilizando a configuração local de `secrets.h`. A Flash anterior de 4 MiB foi preservada antes da instalação. O mapa anterior não possuía a partição LittleFS do projeto; a partição do novo firmware foi inicializada explicitamente. O dispositivo `hidra_01` foi marcado como `metadata/source=simulator` para identificar os eventos fictícios.

Foram verificadas na placa física conexão Wi-Fi, sincronização UTC, autenticação e publicação HTTPS no Realtime Database. Duas sessões de sete eventos foram recebidas e confirmadas com o cliente do cuidador.

| Ensaio | Sessão | Ordem | Resultado |
|---|---|---|---|
| Primeiro envio | `ac1b898113366d3d_s1` | 1 | 7 eventos, `completed`, E5 |
| Após reinício | `a1af85858944ff81_s1` | 2 | 7 eventos, `completed`, E5 |

As sequências foram de 1 a 7, com identificadores de inicialização distintos e horários UTC nos eventos. A ordem persistida da sessão avançou de 1 para 2 após reiniciar, sem formatar novamente o LittleFS. Em ambos os ensaios, o diagnóstico final foi:

```text
wifi=1 auth=1 pending=0 dropped=0 error=0
```

O banco confirmou a presença, o resumo completo e a leitura das sessões pela conta do cuidador. O resultado sanitizado e os logs estão em `.runtime/esp32-cloud/resultado.json` e `.runtime/esp32-cloud/*-serial.txt`. O backup está em `.runtime/esp32-cloud/flash-anterior.bin`, ignorado pelo Git. A placa permaneceu com o firmware de ensaio; não foi restaurada ao firmware anterior.

Este ensaio comprova ESP32 → Firebase → leitura autenticada pelo cuidador via REST. Não executa as telas Android, não utiliza sensores e não demonstra recuperação de eventos em queda de energia. O reinício foi realizado com a primeira sessão já confirmada. O teste de sessão aberta/pendente durante reinício permanece separado.

## Integração com a main — 15/11/2026

Base incorporada: `origin/main` em `3a9a5b5`, com o orquestrador da etapa 2. O merge local foi registrado em `3806284`; os resultados anteriores acima são históricos e as limitações superadas estão explicitadas nesta revisão.

| Verificação | Resultado |
|---|---|
| Contrato e diário do simulador | 14 testes aprovados |
| Programa C++ de demonstração | 5 testes aprovados |
| Adapter + orquestrador real | 4 testes aprovados |
| Regras Firebase | 9 testes aprovados |
| Auth/RTDB REST nos emuladores | 5 testes aprovados, incluindo eventos da etapa 2 |
| ViewModel Android | 4 testes aprovados |
| Android | Lint aprovado e três variantes APK compiladas |
| Firmware PlatformIO | `esp32dev`, `esp32-synthetic`, `esp32-network-probe` e `esp32-integrated` compilados |
| Etapa 2 na raiz | 73 assertivas em 14 cenários e teste de fumaça em duas fases aprovados |

Total da etapa 3: **41 testes automatizados aprovados**, além da suíte da etapa 2. A ponte cobre retorno à balança com volume estimado, conclusão pela base sem volume, alerta/resolução e reinício com continuação de sequência. Verifica também boot acima de 65535 e preservação da ordem NVS no adapter. O teste de fumaça valida a elevação do piso da ordem NVS sem regressão.

O projeto real está configurado, e as regras atualizadas foram publicadas pelo responsável. A configuração privada da ESP32 foi autenticada com sucesso pelo computador. A Flash de 4 MiB anterior à integração está preservada em `.runtime/merge-main/flash-before-integrated.bin`. Nenhuma credencial, token ou imagem de Flash integra o Git.

Para repetir: execute `bash tests/run_tests.sh` na raiz e `bash scripts/test_all.sh` na pasta da etapa 3. O firmware integrado usa `pio run -d firmware -e esp32-integrated`. Veja [INTEGRACAO.md](INTEGRACAO.md) para configuração, console e bancada.

Permanecem sem homologação a aquisição com copo real, áudio efetivamente audível, RNF_LATENCIA, atualização remota em até 5 s, execução das telas Android em aparelho e recuperação sob corte físico de energia. O retorno positivo da fila não elimina a janela de perda em RAM. A nona faixa sugerida depende de gravação e alteração do limite; o código continua com oito faixas.

### ESP32 integrada e Firebase real

A sessão `0002_s001` teve quatro eventos contíguos: início, instrução, E0→E1 e cancelamento local. O `session_order=3` continuou os ordinais 1/2 do firmware anterior; o boot `0002` coincidiu com o prefixo da sessão e os eventos. O resumo recebido foi `aborted`, sem lacuna, e a conta do cuidador leu os mesmos registros.

Uma falha inicial de conexão HTTPS impediu autenticar; os quatro eventos permaneceram no diário. Após reinício, a placa autenticou e drenou os registros. A presença confirmou `activity_active=false`, pendências zero e perdas zero. Isso valida esse caso de reconexão com diário persistido, sem caracterizar ensaio exaustivo de indisponibilidade ou corte de energia.

O ensaio de sessão aberta utilizou `0004_s001`, com `session_order=4`. Após três eventos no boot `0004`, o reinício controlado gerou `activity_aborted/restart` com sequência 4 no boot `0005`, preservando o identificador original da sessão. O banco e a leitura do cuidador confirmaram os quatro eventos contíguos, resumo `aborted` e histórico completo. A presença final informou:

```text
IoT: wifi=1 auth=1 pendentes=0 perdidos=0 recusados=0 erro=''
activity_active=false; session_id=0004_s001; sequence=4; boot_id=0005
```

A ESP32 permaneceu com `esp32-integrated`; `metadata/source=simulator` identifica estes ensaios comandados pela Serial. Não houve ensaio de beber água ou áudio medido na montagem. Os registros sanitizados estão em `.runtime/merge-main/verified-integrated.json`, `serial-integrated.txt` e `restart-serial.txt`, ignorados pelo Git.

## Android 0.2.0 — revisão da interface em 15/11/2026

As capturas enviadas pelo responsável demonstraram no telefone a seleção dos dispositivos vinculados, consulta de presença, histórico e detalhes das sessões reais dos ensaios. Os resultados, contagens e motivos de cancelamento/reinício eram coerentes com os registros anteriores. Essas capturas são da versão 0.1.0; não demonstram a nova interface.

A revisão 0.2.0 acrescenta temas claro/escuro, cartões de status, linha do tempo com descrições em português, detalhes técnicos recolhidos, troca de dispositivos sem logout, botão de atualização da lista e campos de acesso com teclado apropriado/controle de visibilidade da senha. As listas usam LazyColumn e respeitam as áreas do sistema e do teclado.

Correções funcionais:

- A conexão do telefone é acompanhada por NetworkCallback, registrado e removido conforme a coleta do fluxo no ciclo de vida; a indicação não depende de uma leitura isolada da rede.
- Após login bem-sucedido, falha na consulta dos dispositivos mantém a tela de dispositivos, permitindo repetir a consulta. Antes, o estado podia continuar em LOGIN e o botão de nova tentativa não executava essa consulta.
- O evento mantém seu horário de ocorrência quando conhecido. Se ausente, a tela identifica explicitamente o timestamp de recebimento como recebimento; não infere o horário do evento nem altera o banco.
- Resultados desconhecidos não são apresentados como atividade em andamento. Alertas e estados de comunicação desatualizados são identificados como últimos dados recebidos.

Validação: **11 testes JVM aprovados** (seis de ViewModel e cinco de apresentação), incluindo recuperação da lista após falha, limpeza de sessão na troca de dispositivos, motivo de reinício/cancelamento, resolução de alerta, volume ausente e distinção entre horário do evento e recebimento. Três variantes APK compiladas; lint sem erros. Execução: `bash scripts/build_android.sh :app:testDemoDebugUnitTest :app:lintDemoDebug`. Logs locais: `.runtime/android-ui-final.txt`; relatórios em `android/app/build/reports/`.

O APK cloud tem versionCode 2 e versionName 0.2.0, com o mesmo applicationId. A nova interface ainda precisa ser conferida no telefone: atualização sobre a versão anterior, temas claro/escuro, navegação, teclado, aumento de fonte, alerta ativo e perda/retorno de conexão. Nenhuma credencial ou arquivo de execução foi adicionado ao Git.
