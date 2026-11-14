# HIDRA — Documentação Técnica de Hardware e Firmware

**Projeto:** HIDRA — Hidratação Inteligente por Detecção e Resposta Automatizada  
**Disciplina:** PCS3930 — Projeto de Extensão III — Escola Politécnica da USP  
**Plataforma:** ESP32 DevKit (ESP-32D)  
**Linguagem:** C++  
**Ambiente de desenvolvimento:** Arduino IDE  
**Repositório:** https://github.com/ferivanov26/HIDRA  
**Escopo deste documento:** Etapa 1 — Hardware, sensores, aquisição de dados e interface com o orquestrador.

---

## 1. Visão geral do projeto

O HIDRA é um sistema embarcado desenvolvido para auxiliar pessoas idosas com Alzheimer na realização de uma atividade cotidiana: preparar um copo de água. O sistema utiliza sensores para identificar acontecimentos físicos durante a atividade e permite a reprodução de instruções verbais previamente gravadas, com o objetivo de orientar o usuário ao longo da tarefa.

A arquitetura do projeto está dividida em três etapas:

- **Etapa 1 — Hardware e aquisição:** montagem física, aquisição de sinais, calibração da célula de carga, processamento das entradas digitais e disponibilização de uma interface de hardware.
- **Etapa 2 — Orquestrador de eventos:** implementação da máquina de estados E0–E5, interpretação dos dados dos sensores, tratamento de temporizadores e desvios, seleção dos áudios e controle dos alertas.
- **Etapa 3 — IoT e aplicativo:** comunicação Wi-Fi, armazenamento de eventos no Firebase e disponibilização do histórico ao cuidador.

Este documento descreve a implementação da Etapa 1, incluindo a montagem do circuito, as bibliotecas utilizadas, a organização do código, o comportamento dos sensores, as funções disponíveis para integração e as limitações identificadas nos testes.

**Importante:** o firmware atual não implementa a máquina de estados da atividade. Ele fornece informações físicas e funções de atuação para que o responsável pelo orquestrador desenvolva a lógica de controle independentemente.

Por exemplo, o firmware informa que a massa medida aumentou de 150 g para 350 g. Cabe ao orquestrador decidir, considerando o estado atual da atividade, se esse aumento representa a adição de água ao copo.

## 2. Organização do repositório

A estrutura do repositório é:

```text
HIDRA/
│
├── Audios/
│   ├── 0001.mp3
│   ├── 0002.mp3
│   ├── 0003.mp3
│   ├── 0004.mp3
│   ├── 0005.mp3
│   ├── 0006.mp3
│   ├── 0007.mp3
│   └── 0008.mp3
│
├── Hardware/
│   ├── alto_falante.ino
│   ├── entradas_digitais.ino
│   ├── gravacao_8_audios.ino
│   ├── script_funcionamento_ESP.ino
│   └── tara_celua_de_carga.ino
│
├── HIDRA_Etapa1/
│   ├── HIDRA_Etapa1.ino
│   ├── Config.h
│   ├── HidraHardware.h
│   ├── HidraHardware.cpp
│   └── README.md
│
└── README.md
```

### 2.1. Diretório `Hardware`

Contém programas utilizados durante os testes individuais dos componentes.

Esses arquivos foram importantes para verificar separadamente o funcionamento do ESP32, dos botões, da célula de carga e do módulo de áudio.

**Não fazem parte do firmware integrado final.** São mantidos como referência para testes e depuração.

### 2.2. Diretório `Audios`

Contém as oito mensagens de voz gravadas para o projeto.

Os arquivos estão numerados de `0001.mp3` a `0008.mp3` e podem ser substituídos por novas gravações, desde que sejam preservados os nomes e a estrutura esperada pelo módulo MP3.

### 2.3. Diretório `HIDRA_Etapa1`

Contém o firmware integrado do ESP32.

Os arquivos possuem as seguintes responsabilidades:

| Arquivo | Responsabilidade |
|---|---|
| `HIDRA_Etapa1.ino` | Inicialização, loop principal, diagnóstico serial e impressão dos eventos |
| `Config.h` | Configuração dos GPIOs, parâmetros, calibração e volume |
| `HidraHardware.h` | Declaração das estruturas, eventos e funções disponibilizados ao orquestrador |
| `HidraHardware.cpp` | Implementação da leitura dos sensores e controle dos atuadores |
| `README.md` | Instruções resumidas de instalação e utilização |

Para implementar o orquestrador, o principal arquivo de referência é **`HidraHardware.h`**, pois ele define o contrato de comunicação com a camada de hardware.

---

## 3. Componentes utilizados

| Componente | Modelo / especificação | Função |
|---|---|---|
| Microcontrolador | ESP-32D | Executar o firmware |
| Botão | R13-507 | Inicialização manual |
| Microchave | KW11-7 | Detectar presença do copo na posição inicial |
| Célula de carga | 5 kg | Medir a massa sobre a plataforma |
| Conversor ADC | HX711, 24 bits | Digitalizar o sinal da célula de carga |
| Módulo de áudio | MP3-TF-1GP V3.0 | Reproduzir instruções sonoras |
| Cartão microSD | SanDisk Ultra, 32 GB | Armazenar os arquivos MP3 |
| Alto-falante | 8 Ω, 0,5 W | Reprodução sonora |
| LED | Vermelho | Sinalização visual |
| Resistores | 330 Ω e 47 Ω | Limitação de corrente e potência |
| Protoboard e jumpers | Diversos | Interconexão dos componentes |

O alto-falante de 0,5 W é provisório. Futuramente, poderá ser substituído por um modelo de 8 Ω e 3 W, mais adequado ao módulo amplificador, desde que a nova configuração seja novamente testada.

---

## 4. Montagem e pinagem do circuito

### 4.1. Mapeamento dos GPIOs

| GPIO do ESP32 | Componente | Função |
|---|---|---|
| 18 | Botão R13-507 | Entrada digital |
| 19 | Microchave KW11-7 | Entrada digital |
| 27 | HX711 — DT/DOUT | Recepção de dados da balança |
| 14 | HX711 — SCK | Clock da balança |
| 25 | MP3 — TX | Recepção de respostas do módulo MP3 |
| 26 | MP3 — RX | Envio de comandos ao módulo MP3 |
| 23 | LED vermelho | Saída digital |

Todos os módulos devem compartilhar uma referência de GND comum.

### 4.2. Botão de inicialização

O botão R13-507 foi conectado entre o GPIO 18 e o GND do ESP32.

A entrada utiliza o resistor interno de pull-up, configurado por meio de:

```cpp
pinMode(18, INPUT_PULLUP);
```

Consequentemente:

- `HIGH`: botão liberado.
- `LOW`: botão pressionado.

Não é necessário resistor externo de pull-up.

### 4.3. Microchave KW11-7

A microchave é utilizada para detectar mecanicamente a presença do copo em sua posição inicial.

Foram utilizados os terminais:

- **COM/C:** conectado ao GND.
- **NO:** conectado ao GPIO 19.

O contato normalmente fechado (NC) não foi utilizado.

A entrada também utiliza `INPUT_PULLUP`:

- `HIGH`: microchave não acionada.
- `LOW`: microchave acionada.

A microchave detecta apenas o acionamento físico de seu mecanismo. Ela não identifica pessoas, não reconhece objetos e não comprova que o usuário está presente.

### 4.4. Célula de carga e módulo HX711

A célula de carga possui quatro fios, soldados diretamente ao módulo HX711.

| Fio da célula | Terminal do HX711 |
|---|---|
| Vermelho | E+ |
| Preto | E− |
| Branco | A− |
| Verde | A+ |

A ligação do HX711 ao ESP32 foi realizada da seguinte forma:

| HX711 | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DT/DOUT | GPIO 27 |
| SCK | GPIO 14 |

Os terminais B+ e B− não são utilizados.

O HX711 recebe o sinal diferencial produzido pela ponte de Wheatstone da célula de carga e realiza sua conversão analógico-digital.

**Atenção:** a plataforma precisa estar mecanicamente estável. Esforços laterais, cabos tensionados, impactos ou alterações na fixação podem modificar as leituras da balança.

### 4.5. Módulo MP3-TF-1GP

A comunicação entre o módulo MP3 e o ESP32 é realizada por UART, a 9600 baud.

| MP3-TF-1GP | ESP32 |
|---|---|
| RX | GPIO 26 |
| TX | GPIO 25 |
| VCC | 5V |
| GND | GND |

A conexão entre TX e RX é cruzada: o TX do ESP32 transmite comandos para o RX do módulo, e o TX do módulo transmite respostas para o RX do ESP32.

**Pendência de segurança elétrica:** o módulo MP3 está alimentado em 5 V, mas ainda não foi medida a tensão lógica de sua saída TX. Como os GPIOs do ESP32 não toleram 5 V, é necessário verificar essa tensão ou utilizar adaptação de nível antes de manter a conexão direta ao GPIO 25. A reprodução dos áudios pode funcionar sem esse fio de retorno, utilizando apenas a comunicação do GPIO 26 para o RX do módulo.

### 4.6. Alto-falante

O alto-falante atual possui especificação nominal de 8 Ω e 0,5 W.

Sua ligação foi realizada da seguinte forma:

```text
MP3 SPK_1
    |
 Resistor 47 Ω
    |
 Alto-falante 8 Ω / 0,5 W
    |
MP3 SPK_2
```

O resistor de 47 Ω foi colocado em série para limitar a corrente e reduzir a potência fornecida ao alto-falante.

**Não conectar SPK_2 diretamente ao GND**, pois a saída do módulo utiliza os dois terminais destinados ao alto-falante.

Essa configuração foi utilizada nos testes de bancada, mas ainda não foi caracterizada quanto à potência real dissipada no resistor e no alto-falante.

### 4.7. LED vermelho

O LED vermelho está conectado ao GPIO 23.

A conexão é:

```text
GPIO 23
   |
Resistor 330 Ω
   |
Ânodo (+) do LED
   |
Cátodo (−) do LED
   |
  GND
```

Quando o GPIO 23 recebe `HIGH`, o LED acende.

Quando recebe `LOW`, o LED apaga.

A função de controle do LED é disponibilizada ao orquestrador, que poderá definir seu comportamento de acordo com os estados da atividade.

---

## 5. Ambiente de desenvolvimento e bibliotecas

O firmware foi desenvolvido utilizando a Arduino IDE, com as seguintes configurações:

- **Placa:** ESP32 Dev Module.
- **Pacote de placas:** esp32 by Espressif Systems, versão 3.3.12.
- **Linguagem:** C++.
- **Monitor Serial:** 115200 baud.
- **Comunicação com o MP3:** UART a 9600 baud.

### 5.1. Biblioteca HX711

Foi utilizada a biblioteca **HX711 de Bogdan Necula (bogde)**.

Essa biblioteca disponibiliza as operações necessárias para comunicação, tara e calibração da célula de carga.

Principais métodos utilizados:

| Método | Função |
|---|---|
| `begin()` | Inicializar o módulo HX711 |
| `is_ready()` | Verificar se existe uma conversão disponível |
| `wait_ready_timeout()` | Aguardar uma conversão por um intervalo limitado |
| `tare()` | Determinar o offset correspondente à plataforma vazia |
| `set_scale()` | Configurar o fator de calibração |
| `get_units()` | Obter a massa convertida em unidades calibradas |

A instalação pode ser realizada pelo Gerenciador de Bibliotecas da Arduino IDE.

### 5.2. HardwareSerial

A comunicação com o módulo MP3 utiliza a classe `HardwareSerial`, disponibilizada pelo core do ESP32.

A inicialização é equivalente a:

```cpp
HardwareSerial serialMp3(2);

serialMp3.begin(
    9600,
    SERIAL_8N1,
    25,
    26
);
```

Não foi necessária a instalação de uma biblioteca específica para o DFPlayer Mini.

O protocolo de comunicação é implementado diretamente em `HidraHardware.cpp`.

### 5.3. Compilação

Para compilar o projeto:

1. Abrir a Arduino IDE.
2. Selecionar `Arquivo → Abrir`.
3. Abrir `HIDRA_Etapa1/HIDRA_Etapa1.ino`.
4. Selecionar a placa `ESP32 Dev Module`.
5. Selecionar a porta USB do dispositivo.
6. Instalar a biblioteca HX711, caso necessário.
7. Executar a compilação.

Na compilação realizada em bancada, o firmware utilizou aproximadamente:

- **21% da memória de programa:** 275.728 bytes.
- **6% da memória RAM global:** 22.268 bytes.

Esses valores referem-se à implementação atual, sem o orquestrador e sem os módulos de comunicação IoT.

---

## 6. Configurações do firmware

O arquivo `Config.h` concentra os parâmetros do projeto.

As principais configurações são:

```cpp
constexpr uint8_t PINO_BOTAO_INICIO = 18;
constexpr uint8_t PINO_COPO_INICIAL = 19;

constexpr uint8_t PINO_HX711_DOUT = 27;
constexpr uint8_t PINO_HX711_SCK = 14;

constexpr uint8_t PINO_MP3_RX_ESP32 = 25;
constexpr uint8_t PINO_MP3_TX_ESP32 = 26;

constexpr uint8_t PINO_LED_VERMELHO = 23;

constexpr uint32_t DEBOUNCE_MS = 30;

constexpr uint32_t INTERVALO_MASSA_MS = 200;
constexpr uint32_t TIMEOUT_MASSA_MS = 1500;

constexpr float FATOR_HX711_CONTAGENS_POR_GRAMA = 425.631805f;

constexpr float FILTRO_MASSA_ALPHA = 0.35f;

constexpr uint8_t VOLUME_MP3 = 30;
```

### 6.1. Volume do módulo MP3 — Informação importante

**O volume utilizado e aprovado nos testes de bancada foi 30, valor máximo da escala do módulo MP3.**

A configuração é:

```cpp
constexpr uint8_t VOLUME_MP3 = 30;
```

Embora tenham sido testados volumes menores, o valor 30 foi o que proporcionou o resultado sonoro desejado na montagem atual.

Essa configuração está associada às seguintes condições:

- Alimentação do MP3-TF-1GP em 5 V.
- Alto-falante de 8 Ω e 0,5 W.
- Resistor de 47 Ω em série.
- Arquivos MP3 previamente normalizados.

**O responsável pelo orquestrador deve manter o volume 30 ao utilizar essa montagem**, salvo se houver necessidade de ajuste decorrente de novos testes.

Esse valor não deve ser interpretado como garantia de segurança elétrica para outros alto-falantes ou outras montagens.

Durante os testes iniciais, com o módulo alimentado em 3,3 V, foram observados volume insuficiente e instabilidade em algumas configurações. A alteração da alimentação para 5 V melhorou significativamente a reprodução.

---

## 7. Aquisição das entradas digitais

O firmware utiliza uma estrutura interna de debounce para monitorar o botão e a microchave.

O mecanismo de debounce considera uma mudança válida somente quando o sinal elétrico permanece estável durante pelo menos 30 ms.

Esse procedimento evita que oscilações mecânicas dos contatos sejam interpretadas como múltiplos acionamentos.

A implementação utiliza `millis()`, sem inserir atrasos artificiais com `delay()` na leitura das entradas.

Os eventos disponíveis são:

```cpp
BotaoPressionado
BotaoLiberado

ChaveInicialAcionada
ChaveInicialLiberada
```

O firmware informa o acontecimento físico, e não seu significado dentro da atividade.

Por exemplo, `ChaveInicialLiberada` significa que a microchave deixou de ser acionada. O orquestrador poderá interpretar esse evento como a retirada do copo da posição inicial se a montagem e o estado atual da atividade forem compatíveis com essa interpretação.

---

## 8. Célula de carga: calibração e funcionamento

### 8.1. Calibração

A calibração da célula de carga foi realizada utilizando uma lata de sardinha com massa de referência de 220 g, previamente medida em outra balança.

Inicialmente, foi realizada a tara com a plataforma vazia.

Em seguida, a massa de referência foi posicionada sobre a plataforma, permitindo calcular o fator de conversão entre as leituras do HX711 e a massa em gramas.

O fator experimental obtido foi:

```cpp
425.631805f
```

Sua unidade é aproximadamente contagens do conversor por grama.

Esse fator é aplicado por meio de:

```cpp
balanca.set_scale(
    HidraConfig::FATOR_HX711_CONTAGENS_POR_GRAMA
);
```

Após a calibração, foram obtidos resultados próximos das massas de referência de 220 g e 19 g.

A calibração não precisa ser refeita a cada inicialização, desde que as condições mecânicas permaneçam equivalentes. Entretanto, **a tara deve ser realizada com a plataforma vazia**.

### 8.2. Tara automática

Durante a inicialização do firmware, é executada uma tara automática para compensar o offset do sistema.

Por esse motivo, a plataforma deve estar vazia quando o ESP32 é ligado ou reiniciado.

A operação de tara utiliza dez amostras do HX711.

O firmware também disponibiliza a função:

```cpp
Hidra::realizarTara();
```

Essa função retorna um valor booleano:

- `true`: a rotina de tara foi concluída.
- `false`: o HX711 não ficou disponível dentro do tempo de espera.

**A operação de tara é bloqueante**, pois necessita adquirir múltiplas amostras. Portanto, não deve ser executada continuamente durante a atividade.

### 8.3. Problema encontrado na tara

Durante os testes integrados, foi identificado um erro na rotina de tara.

Inicialmente, o código utilizava:

```cpp
if (!balanca.is_ready()) return false;
```

Essa implementação verificava apenas se havia uma conversão disponível naquele instante.

Como o HX711 realiza conversões periodicamente, o sensor podia estar funcionando corretamente, mas ainda não ter concluído a próxima conversão.

Isso fazia o firmware informar incorretamente:

```text
ERRO: HX711 nao disponivel
```

A correção foi realizada substituindo a verificação imediata por:

```cpp
if (!balanca.wait_ready_timeout(2000)) {
    return false;
}
```

Dessa forma, o firmware aguarda até dois segundos pela disponibilidade de uma conversão antes de considerar a operação malsucedida.

A correção foi testada e permitiu executar novamente a tara.

### 8.4. Leitura periódica da massa

O firmware tenta adquirir uma nova amostra da célula de carga a cada 200 ms:

```cpp
constexpr uint32_t INTERVALO_MASSA_MS = 200;
```

A leitura somente é realizada quando o HX711 possui uma conversão disponível.

O valor obtido é convertido para gramas e submetido a um filtro digital.

A interface disponibiliza:

- Massa em gramas.
- Indicador de validade da massa.
- Instante da última medição válida.

**Uma massa não deve ser utilizada pelo orquestrador se `massa_valida` for `false`.**

### 8.5. Aumento gradual da massa — Comportamento importante

Durante os testes integrados, foi observado que a massa não muda instantaneamente quando um objeto é colocado ou retirado da plataforma.

Por exemplo, ao posicionar uma lata de aproximadamente 220 g, o valor informado aumenta progressivamente até se aproximar do valor final.

Essa estabilização foi percebida ao longo de aproximadamente dois segundos.

O comportamento está associado, ao menos em parte, ao filtro exponencial utilizado no firmware:

```cpp
constexpr float FILTRO_MASSA_ALPHA = 0.35f;
```

A atualização da massa filtrada utiliza:

\[
m_f[n] = 0{,}35m[n] + 0{,}65m_f[n-1]
\]

Em que:

- \(m_f[n]\): massa filtrada atual.
- \(m[n]\): nova amostra da célula de carga.
- \(m_f[n-1]\): massa filtrada anterior.

O filtro reduz pequenas oscilações e ruídos, mas introduz uma resposta gradual às alterações de massa.

A acomodação mecânica da plataforma também pode contribuir para esse comportamento, embora sua influência não tenha sido medida separadamente.

**Consequência para o orquestrador:** não é recomendado identificar imediatamente a conclusão de uma etapa com base em uma única medição.

Para detectar se o copo foi colocado, preenchido ou retirado da plataforma, recomenda-se considerar:

- Limiares de massa.
- Histerese para evitar oscilações entre condições.
- Estabilização temporal da leitura.
- Comparação com massas anteriormente registradas.

Esses critérios pertencem à lógica do orquestrador e ainda precisam ser definidos e testados.

### 8.6. Validade e indisponibilidade da massa

O código utiliza:

```cpp
constexpr uint32_t TIMEOUT_MASSA_MS = 1500;
```

Se não forem recebidas novas amostras válidas durante mais de 1500 ms, o firmware invalida a medição e pode emitir o evento:

```cpp
MassaIndisponivel
```

Isso permite ao orquestrador distinguir um valor de massa válido de uma medição antiga ou indisponível.

---

## 9. Reprodução dos áudios

### 9.1. Gravação e preparação

Foram produzidas oito mensagens de voz para orientar o usuário durante a atividade.

As mensagens foram gravadas utilizando um celular, processadas e separadas em arquivos individuais.

Os arquivos foram preparados com as seguintes características:

- Formato MP3.
- Áudio mono.
- Taxa de amostragem de 44,1 kHz.
- Bitrate de 128 kbps.
- Normalização de volume.

Os oito arquivos foram testados individualmente no módulo MP3-TF-1GP, e todos foram reproduzidos com sucesso.

### 9.2. Mensagens disponíveis

A tabela abaixo apresenta o roteiro utilizado na preparação dos áudios.

| Arquivo | Mensagem | Finalidade sugerida |
|---|---|---|
| `0001.mp3` | Olá! Vamos beber um pouco de água? | Início |
| `0002.mp3` | Pegue o copo que está na sua frente. | Orientar retirada do copo |
| `0003.mp3` | Coloque o copo na plataforma indicada. | Posicionar o copo |
| `0004.mp3` | Agora, coloque água no copo. | Enchimento |
| `0005.mp3` | Muito bem! Agora retire o copo. | Retirada do copo cheio |
| `0006.mp3` | Parabéns! Você concluiu a atividade. | Reforço positivo |
| `0007.mp3` | Vamos continuar? Ainda falta uma etapa. | Lembrete |
| `0008.mp3` | Se precisar de ajuda, chame alguém de confiança. | Orientação para solicitar ajuda |

A associação entre cada faixa e um estado da atividade é responsabilidade do orquestrador. A tabela representa as mensagens planejadas, e o conteúdo efetivo dos arquivos deve ser mantido correspondente a ela caso sejam substituídos.

### 9.3. Organização do microSD

O cartão microSD deve estar formatado em FAT32.

Os arquivos precisam ser armazenados na pasta `mp3`, localizada na raiz:

```text
microSD/
└── mp3/
    ├── 0001.mp3
    ├── 0002.mp3
    ├── 0003.mp3
    ├── 0004.mp3
    ├── 0005.mp3
    ├── 0006.mp3
    ├── 0007.mp3
    └── 0008.mp3
```

Essa organização é necessária porque o firmware utiliza o comando UART `0x12`, responsável por selecionar faixas numeradas dentro da pasta `mp3`.

O nome da pasta do repositório, `Audios`, não deve ser confundido com o caminho esperado pelo módulo no cartão.

### 9.4. Protocolo UART

O módulo MP3 recebe pacotes de dez bytes.

O formato geral é:

```text
7E FF 06 CMD 00 PH PL CH CL EF
```

Os campos representam:

| Campo | Significado |
|---|---|
| `7E` | Início do pacote |
| `FF` | Versão do protocolo |
| `06` | Comprimento |
| `CMD` | Código do comando |
| `00` | Indicador de feedback |
| `PH` e `PL` | Parâmetro de 16 bits |
| `CH` e `CL` | Checksum |
| `EF` | Fim do pacote |

Os comandos utilizados são:

| Comando | Função |
|---|---|
| `0x06` | Configuração do volume |
| `0x12` | Reprodução de uma faixa da pasta `mp3` |

### 9.5. Reprodução pelo firmware

O firmware disponibiliza a função:

```cpp
Hidra::reproduzirFaixa(numero);
```

Por exemplo:

```cpp
Hidra::reproduzirFaixa(1);
```

Solicita a reprodução de:

```text
/mp3/0001.mp3
```

E:

```cpp
Hidra::reproduzirFaixa(6);
```

Solicita:

```text
/mp3/0006.mp3
```

A função aceita valores de 1 a 8.

**Importante:** o retorno `true` significa apenas que o número informado foi aceito e o comando foi enviado pela UART. Não significa que o módulo confirmou a reprodução, nem que o áudio terminou.

O firmware atual não fornece callback de término de áudio. As respostas recebidas pelo UART do MP3 são descartadas na rotina de atualização. Caso o orquestrador precise detectar precisamente o término de uma mensagem, essa funcionalidade deverá ser implementada e validada separadamente.

### 9.6. Volume de reprodução

O valor utilizado na montagem foi:

```cpp
constexpr uint8_t VOLUME_MP3 = 30;
```

**Esse valor deve ser preservado como configuração inicial do protótipo**, pois foi o que funcionou adequadamente durante os testes das oito instruções.

A alimentação do módulo deve permanecer em 5 V na configuração atual.

Como o alto-falante possui potência nominal baixa, a montagem deve ser observada quanto a distorções, ruídos ou aquecimento. Uma eventual substituição do alto-falante exige nova avaliação.

---

## 10. Interface pública para o orquestrador

O arquivo `HidraHardware.h` define a interface que deverá ser utilizada pela Etapa 2.

O código utiliza o namespace:

```cpp
namespace Hidra
```

As principais funções são:

```cpp
void iniciar(ReceptorEvento receptor);

void atualizar();

LeituraHardware obterLeitura();

bool realizarTara();

void definirLed(bool ligado);

bool reproduzirFaixa(uint8_t numero);

void definirVolume(uint8_t nivel);
```

### 10.1. `Hidra::iniciar()`

Inicializa os periféricos e registra a função responsável por receber os eventos físicos.

Exemplo:

```cpp
Hidra::iniciar(receberEventoFisico);
```

Essa inicialização configura:

- Entradas digitais.
- Resistores internos de pull-up.
- Saída do LED.
- Comunicação UART com o MP3.
- Volume do áudio.
- Comunicação com o HX711.
- Fator de calibração.
- Tara inicial, quando o sensor estiver disponível.

**A plataforma precisa estar vazia durante essa chamada.**

### 10.2. `Hidra::atualizar()`

É a principal função de atualização dos periféricos.

Deve ser executada repetidamente no `loop()` do ESP32.

```cpp
void loop() {
    Hidra::atualizar();
}
```

Essa função:

- Monitora as entradas digitais.
- Processa o debounce.
- Identifica mudanças dos contatos.
- Consulta o HX711 periodicamente.
- Atualiza a massa filtrada.
- Verifica a validade da medição.
- Emite os eventos físicos correspondentes.
- Consome respostas seriais recebidas do módulo MP3.

**Não utilizar atrasos prolongados no loop principal**, pois isso prejudicará o acompanhamento dos sensores.

### 10.3. `Hidra::obterLeitura()`

Retorna um snapshot com as informações físicas mais recentes.

A estrutura é:

```cpp
struct LeituraHardware {
    bool botao_pressionado;
    bool chave_inicial_acionada;
    bool massa_valida;
    float massa_g;
    uint32_t instante_massa_ms;
};
```

Significado dos campos:

| Campo | Tipo | Significado |
|---|---|---|
| `botao_pressionado` | `bool` | Verdadeiro se o botão estiver pressionado |
| `chave_inicial_acionada` | `bool` | Verdadeiro se a microchave estiver acionada |
| `massa_valida` | `bool` | Verdadeiro se existir medição recente válida |
| `massa_g` | `float` | Massa filtrada em gramas |
| `instante_massa_ms` | `uint32_t` | Instante da última medição válida, em milissegundos desde o boot |

Exemplo:

```cpp
Hidra::LeituraHardware leitura = Hidra::obterLeitura();

if (leitura.massa_valida) {
    float massa = leitura.massa_g;
}
```

**Nunca interpretar `massa_g` como uma medição confiável quando `massa_valida` for `false`.**

O instante fornecido por `instante_massa_ms` vem de `millis()` e não representa data ou horário UTC.

### 10.4. Eventos físicos

Os eventos disponíveis são definidos pela enumeração:

```cpp
enum class TipoEventoFisico : uint8_t {
    BotaoPressionado,
    BotaoLiberado,
    ChaveInicialAcionada,
    ChaveInicialLiberada,
    MassaAtualizada,
    MassaIndisponivel
};
```

A estrutura correspondente é:

```cpp
struct EventoFisico {
    TipoEventoFisico tipo;
    uint32_t instante_ms;
    float massa_g;
    bool massa_valida;
};
```

O campo `tipo` identifica o acontecimento.

O campo `instante_ms` representa o instante do evento em milissegundos desde a inicialização do ESP32.

Os campos `massa_g` e `massa_valida` devem ser utilizados para interpretar os eventos de massa, especialmente `MassaAtualizada`.

Para eventos de botão ou microchave, o valor de `massa_g` não representa uma medição associada ao evento.

### 10.5. Callback de eventos

O firmware permite registrar uma função que será chamada quando um evento físico for identificado.

O tipo do callback é:

```cpp
using ReceptorEvento =
    void (*)(const EventoFisico& evento);
```

Um exemplo de integração:

```cpp
#include "HidraHardware.h"

void receberEventoFisico(
    const Hidra::EventoFisico& evento
) {
    switch (evento.tipo) {

        case Hidra::TipoEventoFisico::BotaoPressionado:
            // Informar ao orquestrador que o botao foi pressionado.
            break;

        case Hidra::TipoEventoFisico::ChaveInicialLiberada:
            // Informar que a microchave foi liberada.
            break;

        case Hidra::TipoEventoFisico::MassaAtualizada:
            // Disponibilizar evento.massa_g ao orquestrador.
            break;

        case Hidra::TipoEventoFisico::MassaIndisponivel:
            // Tratar indisponibilidade da medicao.
            break;

        default:
            break;
    }
}

void setup() {
    Hidra::iniciar(receberEventoFisico);
}

void loop() {
    Hidra::atualizar();
}
```

Esse exemplo demonstra apenas como consumir a interface. Não implementa a máquina de estados.

**O callback é executado durante `Hidra::atualizar()`.** Portanto, recomenda-se que seja curto e não realize operações bloqueantes, como chamadas de rede ou esperas prolongadas.

### 10.6. Controle do LED

```cpp
Hidra::definirLed(true);
```

Liga o LED.

```cpp
Hidra::definirLed(false);
```

Desliga o LED.

O firmware não estabelece automaticamente quando o LED deve ser acionado. Essa decisão cabe ao orquestrador.

### 10.7. Controle do volume

```cpp
Hidra::definirVolume(30);
```

Solicita o volume máximo do módulo MP3.

A função aceita valores de 0 a 30; valores acima de 30 são limitados a 30 pela implementação.

### 10.8. Tara

```cpp
bool sucesso = Hidra::realizarTara();
```

Executa a tara da plataforma.

Deve ser chamada somente com a plataforma vazia e fora de períodos críticos da atividade, pois é uma operação bloqueante.

---

## 11. Interface de diagnóstico pelo Monitor Serial

O arquivo `HIDRA_Etapa1.ino` contém um programa de diagnóstico utilizado durante os testes de bancada.

O Monitor Serial deve ser configurado em:

- **115200 baud.**
- **Final de linha: Nova linha.**

A configuração de final de linha é importante porque a versão atual do código somente executa comandos quando recebe o caractere `\n`.

Durante os testes, os comandos inicialmente não funcionavam porque o Monitor Serial não estava enviando esse caractere. Após selecionar `Nova linha`, todos passaram a responder.

### 11.1. Comandos disponíveis

| Comando | Função |
|---|---|
| `h` | Exibir ajuda |
| `s` | Consultar o estado dos sensores |
| `t` | Executar tara |
| `l1` | Ligar LED |
| `l0` | Desligar LED |
| `p1` | Reproduzir áudio 1 |
| `p2` | Reproduzir áudio 2 |
| `p3` | Reproduzir áudio 3 |
| `p4` | Reproduzir áudio 4 |
| `p5` | Reproduzir áudio 5 |
| `p6` | Reproduzir áudio 6 |
| `p7` | Reproduzir áudio 7 |
| `p8` | Reproduzir áudio 8 |

A letra utilizada nos comandos `l1` e `l0` é um **L minúsculo**, de LED, não a letra `i`.

### 11.2. Exemplos de eventos impressos

```text
BOTAO_PRESSIONADO
BOTAO_LIBERADO

CHAVE_INICIAL_ACIONADA
CHAVE_INICIAL_LIBERADA

MASSA_ATUALIZADA: 220.18 g (valida=1)
```

O comando `s` apresenta o estado atual das entradas e da balança.

O comando `t` executa a tara e informa `TARA OK` quando concluída.

**A interface serial é destinada exclusivamente a testes e depuração.** O orquestrador deverá utilizar diretamente as funções C++ de `HidraHardware.h`, sem depender do envio desses comandos pelo Monitor Serial.

---

## 12. Orientações para implementar o orquestrador

O responsável pela Etapa 2 tem liberdade para organizar o código e desenvolver a máquina de estados, desde que respeite o contrato de hardware existente.

A apostila define a seguinte sequência:

| Estado | Ação esperada |
|---|---|
| E0 | Iniciar a atividade |
| E1 | Pegar o copo da posição inicial |
| E2 | Posicionar o copo na plataforma de pesagem |
| E3 | Adicionar água |
| E4 | Retirar o copo cheio |
| E5 | Encerrar a atividade |

Os eventos físicos fornecidos pela Etapa 1 podem auxiliar no reconhecimento dessas ações.

### 12.1. Início da atividade

O evento `BotaoPressionado` pode ser utilizado como gatilho para iniciar a atividade.

O orquestrador também poderá implementar início por horário programado, conforme os requisitos do projeto.

### 12.2. Retirada do copo da posição inicial

A microchave KW11-7 permite identificar mudanças de contato.

Se a montagem estiver configurada para que o copo pressione a microchave na posição inicial, o evento `ChaveInicialLiberada` poderá representar a retirada do copo.

Essa correspondência deve ser confirmada fisicamente na montagem.

### 12.3. Posicionamento na plataforma

A célula de carga permite verificar a presença de uma massa sobre a plataforma.

O orquestrador deverá definir um limiar adequado para diferenciar a plataforma vazia de um copo posicionado.

É recomendável utilizar um limiar com margem e alguma forma de estabilidade temporal, evitando que oscilações causem mudanças indevidas de estado.

### 12.4. Adição de água

O firmware disponibiliza continuamente a massa filtrada.

O orquestrador poderá registrar a massa do copo vazio e detectar um aumento sustentado após a adição de água.

A diferença de massa poderá ser utilizada posteriormente para estimativas relacionadas ao volume de água, considerando as aproximações adotadas pelo projeto.

**Não assumir que uma diferença de massa comprova a ingestão da água pelo usuário.**

### 12.5. Retirada do copo

A retirada do copo da plataforma pode ser identificada quando a massa medida retorna a valores próximos à massa da plataforma vazia.

É necessário considerar a resposta gradual do filtro antes de confirmar essa transição.

### 12.6. Conclusão

Ao reconhecer a sequência observável prevista, o orquestrador poderá registrar a conclusão da atividade e solicitar a reprodução de `0006.mp3`.

A conclusão observável da sequência não significa necessariamente que o usuário ingeriu a água.

### 12.7. Temporizadores e desvios

A Etapa 2 deverá implementar:

- Identificação de não iniciação.
- Detecção de estagnação em uma etapa.
- Identificação de eventos incompatíveis com a sequência.
- Repetição de instruções.
- Contagem de tentativas.
- Sinalização de necessidade de ajuda.
- Encerramento ou interrupção da atividade.

Essas funcionalidades não estão implementadas no firmware atual.

### 12.8. Recomendações de integração

Para evitar problemas durante a implementação:

1. Inicializar os periféricos uma única vez.
2. Executar `Hidra::atualizar()` continuamente.
3. Utilizar os callbacks de eventos físicos ou consultar o snapshot quando necessário.
4. Verificar `massa_valida` antes de interpretar a massa.
5. Considerar o tempo de estabilização da célula de carga e do filtro.
6. Manter a lógica de estados separada das funções de aquisição.
7. Não executar a tara durante uma sessão de hidratação.
8. Não bloquear o loop principal durante a reprodução de áudio.
9. Não interpretar o retorno de `reproduzirFaixa()` como confirmação de término da reprodução.
10. Preservar a configuração de volume 30 na montagem atual, observando as limitações elétricas.

---

## 13. Diferença entre os eventos físicos e os eventos do Firebase

É importante diferenciar dois níveis de informação.

**Eventos físicos da Etapa 1:**

```text
BotaoPressionado
ChaveInicialLiberada
MassaAtualizada
MassaIndisponivel
```

São observações diretas dos sensores.

**Eventos interpretados pelas Etapas 2 e 3:**

```text
activity_started
state_changed
instruction_played
caregiver_alert
alert_resolved
activity_completed
activity_aborted
```

Esses eventos representam decisões e acontecimentos da atividade, sendo destinados ao registro e à comunicação IoT.

O orquestrador deverá converter observações físicas em transições de estado e, posteriormente, nos eventos apropriados para a camada de comunicação.

O módulo de hardware não deve publicar diretamente todas as leituras da célula de carga no Firebase.

Conforme a arquitetura prevista na apostila, o orquestrador entregará ao módulo IoT os eventos já interpretados. O contrato de publicação desse módulo utiliza funções como:

```cpp
void iot_begin(const IotConfig& config);

bool iot_publish(const HidraEvent& event);

IotStatus iot_get_status();
```

Essa interface é independente da interface local `HidraHardware.h`.

O responsável pela Etapa 2 deverá realizar a integração entre elas de acordo com os modelos de evento definidos pela Etapa 3.

---

## 14. Testes realizados e resultados

Durante o desenvolvimento, os componentes foram validados individualmente antes da integração.

| Subsistema | Teste realizado | Resultado |
|---|---|---|
| ESP32 | Gravação e Monitor Serial | Aprovado |
| Botão R13-507 | Pressionamento e liberação | Aprovado |
| Microchave KW11-7 | Acionamento e liberação | Aprovado |
| HX711 | Aquisição de leituras brutas | Aprovado |
| Célula de carga | Calibração com 220 g | Aprovado nos testes realizados |
| Célula de carga | Verificação com 19 g | Aprovado nos testes realizados |
| LED vermelho | Acionamento pelo GPIO 23 | Aprovado |
| MP3-TF-1GP | Reprodução de áudio | Aprovado |
| Biblioteca de áudios | Reprodução das oito faixas | Aprovado |
| Firmware integrado | Compilação na Arduino IDE | Aprovado |
| Firmware integrado | Comandos seriais | Aprovado |
| Firmware integrado | Entradas digitais | Aprovado |
| Firmware integrado | Correção da tara | Aprovado |
| Sistema completo | Execução prolongada e integração E0–E5 | Pendente |

### 14.1. Resultados da calibração

Nos testes individuais da balança, foram obtidas as seguintes medições:

| Massa de referência | Medições observadas |
|---|---|
| 220 g | 220,19 g; 222,93 g; 220,18 g |
| 19 g | 19,08 g; 19,11 g; 19,29 g; 19,03 g; 19,17 g |

A média aproximada das medições do objeto de 19 g foi de 19,14 g, correspondente a um erro relativo de aproximadamente 0,72%.

Esses resultados demonstraram funcionamento coerente nas condições testadas, mas não representam uma caracterização completa da precisão da célula em toda sua faixa nominal de 5 kg.

### 14.2. Problemas identificados durante o desenvolvimento

**Baixo volume do áudio:** inicialmente, a alimentação do módulo em 3,3 V produziu volume insuficiente. A alteração para 5 V e a utilização do resistor de 47 Ω melhoraram a reprodução. A configuração final de volume adotada foi 30.

**Instabilidade em testes de áudio:** foram observados ruídos e interrupções durante testes com configurações anteriores. A desconexão temporária do alto-falante auxiliou na investigação. A configuração final permitiu reproduzir as oito faixas.

**Comandos seriais aparentemente inoperantes:** o Monitor Serial estava configurado sem final de linha. Após selecionar `Nova linha`, os comandos passaram a funcionar.

**Offset na célula de carga:** durante os testes integrados, foram observadas leituras próximas de 5 g sem carga e 225 g para uma massa de referência de 220 g. Isso sugeriu deslocamento do zero e motivou nova tentativa de tara.

**Falha na rotina de tara:** o uso imediato de `is_ready()` causava mensagens de indisponibilidade mesmo quando o HX711 estava funcionando. A substituição por `wait_ready_timeout(2000)` corrigiu o problema observado.

**Estabilização gradual da massa:** a leitura se aproxima progressivamente do valor final, comportamento compatível com o filtro exponencial implementado e potencialmente com a acomodação mecânica da plataforma. Esse comportamento deve ser considerado na implementação do orquestrador.

---

## 15. Limitações e pendências

Embora a implementação principal da Etapa 1 esteja concluída, existem limitações importantes que devem ser consideradas:

**1. Compatibilidade elétrica da UART do MP3:** ainda é necessário verificar o nível lógico do sinal TX do módulo MP3 quando alimentado em 5 V. O ESP32 não deve receber diretamente sinais de 5 V.

**2. Alto-falante provisório:** o dispositivo atual possui potência nominal de 0,5 W. O uso de volume 30 foi aprovado funcionalmente em bancada, mas não houve caracterização elétrica completa da saída amplificada. A substituição futura por alto-falante de 3 W deverá ser acompanhada de nova avaliação.

**3. Resposta gradual da massa:** o orquestrador precisará considerar o filtro digital e a estabilização das leituras antes de reconhecer transições.

**4. Tara bloqueante:** a execução de `realizarTara()` pode interromper temporariamente o acompanhamento dos sensores.

**5. Ausência de confirmação de reprodução:** o firmware envia comandos ao módulo MP3, mas não fornece confirmação de início ou término do áudio.

**6. Ausência de máquina de estados:** os estados E0–E5 ainda não estão implementados. Eles pertencem à Etapa 2.

**7. Testes integrados prolongados:** ainda é necessário validar a estabilidade do sistema durante uma sessão completa de hidratação e em condições de uso contínuo.

**8. Limites de desempenho:** a apostila prevê erro máximo de ±10 g na massa e latência inferior a 2 segundos entre a identificação de um evento ou timeout e o início da instrução verbal correspondente. O segundo requisito exige medições envolvendo o orquestrador.

---

## 16. Procedimento recomendado para os próximos desenvolvedores

Para continuar o desenvolvimento do HIDRA, recomenda-se seguir esta sequência:

1. Clonar o repositório e abrir `HIDRA_Etapa1/HIDRA_Etapa1.ino` na Arduino IDE.
2. Instalar o pacote ESP32 da Espressif e a biblioteca HX711 de Bogdan Necula.
3. Conferir a pinagem e as condições de alimentação.
4. Verificar a segurança elétrica do sinal TX do MP3 antes de conectar o GPIO 25 diretamente.
5. Preparar o cartão microSD com os oito arquivos em `/mp3/`.
6. Deixar a balança vazia durante a inicialização.
7. Compilar e gravar o firmware.
8. Conferir as leituras e os eventos no Monitor Serial.
9. Testar o LED e a reprodução dos áudios.
10. Utilizar `HidraHardware.h` como interface de integração.
11. Implementar a máquina de estados E0–E5 em um módulo separado.
12. Definir critérios de massa, estabilidade, temporização e desvio.
13. Integrar o controle de áudio e LED às decisões do orquestrador.
14. Realizar testes de uma sessão completa.
15. Integrar os eventos interpretados à camada IoT.

O arquivo `.ino` atual poderá ser adaptado para incorporar o orquestrador. Recomenda-se preservar `Config.h`, `HidraHardware.h` e `HidraHardware.cpp` como a camada de abstração do hardware, realizando alterações nesses arquivos apenas quando necessárias e documentadas.

---

## 17. Considerações finais

A Etapa 1 do HIDRA fornece uma base funcional de hardware e firmware para o desenvolvimento do orquestrador de eventos. Foram implementadas e testadas as entradas digitais, a aquisição de massa pelo HX711, o controle de um LED e a reprodução de oito instruções sonoras armazenadas em cartão microSD.

A arquitetura do firmware separa a aquisição dos sensores das decisões sobre a atividade, permitindo que o responsável pela Etapa 2 desenvolva sua máquina de estados utilizando os eventos físicos e as funções públicas disponibilizadas em `HidraHardware.h`.

Os principais cuidados para a continuidade do projeto são preservar a montagem e os parâmetros validados, utilizar **volume 30 na configuração atual de áudio**, considerar a **resposta gradual das leituras de massa**, realizar a **tara com a plataforma vazia** e respeitar a distinção entre eventos físicos e eventos interpretados.

Com isso, o responsável pelo orquestrador poderá concentrar seu trabalho nas regras de decisão, temporizadores, transições E0–E5 e instruções ao usuário, sem precisar reimplementar a aquisição e o controle básico dos periféricos.

**Estado da entrega:** firmware da Etapa 1 implementado, compilado e com os principais subsistemas validados em bancada. Permanecem pendentes a validação elétrica definitiva da UART do MP3, os testes integrados completos e a homologação conjunta com o orquestrador.