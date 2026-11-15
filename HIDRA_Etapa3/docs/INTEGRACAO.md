# Integração da etapa 3

## Responsabilidades

O módulo `HIDRA_Etapa1/HidraHardware.h`, na raiz do repositório, disponibiliza `Hidra::LeituraHardware` e `Hidra::EventoFisico`. A massa em gramas só deve ser usada quando `massa_valida` for verdadeira; os instantes físicos são monotônicos desde o boot. A etapa 2 recebe essas observações e produz os eventos de atividade. A etapa 2 reconhece transições, executa áudio/sinalização e publica **acontecimentos processados**. A etapa 3 não estima ingestão a partir dos sensores e não determina a próxima instrução.

O código concentra os componentes internos do módulo em `firmware/iot/src/iot.cpp`, com funções separadas para fila, persistência, autenticação, cliente REST e sincronização. A interface pública está em `iot.h`.

## Ambientes de compilação

A etapa 1 usa Arduino IDE com core ESP32 3.3.12 ou compatível. A etapa 3 foi compilada separadamente com Arduino ESP32 2.0.17, fixado pela plataforma Espressif32 6.10.0 do PlatformIO. Os projetos permanecem independentes nesta branch; a migração não combina os dois programas nem comprova compatibilidade entre os cores. Um firmware único deve definir um único core, um único `setup` e um único `loop`.

O callback físico da etapa 1 não deve chamar rede ou persistência. A etapa 2 faz a conversão de observações físicas em `HidraEvent` e alimenta `iot_publish` e `iot_set_snapshot`.

## API

```cpp
void iot_begin(const IotConfig& config);
bool iot_publish(const HidraEvent& event);
IotStatus iot_get_status();
void iot_set_snapshot(const IotSnapshot& snapshot);
RecoveredSession iot_recovered_session();
const char* iot_boot_id();
```

`iot_begin` deve ser chamado uma única vez antes de iniciar as tarefas produtoras. Ponteiros da configuração precisam continuar válidos durante toda a execução (use strings estáticas). A inicialização monta o LittleFS e recupera arquivos existentes. A tarefa FreeRTOS de comunicação é criada com prioridade 1 e stack de 16 KiB.

`iot_publish` valida, copia uma estrutura de tamanho fixo e retorna sem rede nem Flash. `true` significa apenas aceitação na fila de 32 posições. A tarefa de comunicação transfere registros a um diário de até 200 eventos, protegidos por tamanho e CRC32. Registros ainda em RAM podem ser perdidos em queda de energia. Não trate `true` como confirmação de armazenamento remoto.

O produtor não deve reutilizar `sequence` para acontecimentos diferentes nem manter referências a dados externos. Um retorno `false` exige registrar diagnóstico local; não bloqueie áudio esperando rede. O contador de perdas inclui saturação da fila e falhas de persistência, quando observáveis; a perda de RAM em desligamento abrupto não é quantificável com garantia.

Exemplo de transição já reconhecida pela etapa 2:

```cpp
HidraEvent event;
strlcpy(event.session_id, sessionId, sizeof(event.session_id));
event.sequence = nextSequence;
event.type = EventType::StateChanged;
event.state = ActivityState::E3;
strlcpy(event.data.previous_state, "E2", sizeof(event.data.previous_state));
event.data.has_mass = true;
event.data.mass_g = 185.0f;
bool accepted = iot_publish(event);
// Tratar accepted=false sem bloquear o controle local.
```

Ao mudar o estado atual, a etapa 2 também chama `iot_set_snapshot`, que copia sessão/estado e captura o horário da observação. O status é publicado a cada 30 s quando a rede e a autenticação permitem. O aplicativo indica ausência de presença recebida após 90 s.

## Sessão aberta após reinicialização

Após `iot_begin`, consulte `iot_recovered_session`. Se `open` for verdadeiro, a etapa 2 deve produzir `activity_aborted`, com a sessão recuperada, sequência `last_sequence + 1`, estado recuperado e motivo `restart`, antes de abrir uma nova sessão. Depois, crie `session_id` com `iot_boot_id()` e contador local. Não retome automaticamente as ações do paciente.

Essa interface recupera a última referência **persistida**. Se o desligamento ocorrer antes da persistência de um evento, não existe garantia de recuperar esse evento ou seu identificador. Essa janela deve ser medida no ESP32 e registrada como limitação.

## Ensaio isolado do ESP32

O ambiente `esp32-synthetic` permite enviar uma sessão fictícia pela Serial, sem sensores:

```bash
pio run -d firmware -e esp32-synthetic
# Depois de configurar secrets.h e preparar a placa:
pio run -d firmware -e esp32-synthetic -t upload
pio device monitor -p /dev/ttyUSB0 -b 115200
```

Execute os comandos a partir de `HIDRA_Etapa3/` e ajuste a porta Serial conforme `pio device list`. Envie `n` na Serial. Use uma identidade separada da identidade final do equipamento e marque administrativamente `metadata/source` como `simulator`. O ensaio publica apenas acontecimentos fictícios; não mede reconhecimento nem latência de áudio. Não use esse ambiente como firmware final da etapa 2.
