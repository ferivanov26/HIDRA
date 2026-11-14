# HIDRA — Firmware integrado da Etapa 1 (Arduino IDE)

**Estado:** revisao 2, candidata para testes integrados em bancada; NAO validada fisicamente ainda.

## Abrir e compilar
1. Abra `HIDRA_Etapa1/HIDRA_Etapa1.ino` na Arduino IDE.
2. Selecione `ESP32 Dev Module` (core Espressif 3.3.12, ou compativel).
3. Instale a biblioteca `HX711` de **Bogdan Necula (bogde)**.
4. Mantenha as conexoes ja validadas. Inicie **com a plataforma vazia** para a tara.
5. Abra Monitor Serial em **115200 baud**, final de linha **Nova linha** ou **Ambos NL e CR**.

## Pinagem (3,3 V em GPIOs do ESP32)
- GPIO18: botao R13-507 → GND (INPUT_PULLUP; pressionado LOW).
- GPIO19: KW11-7 COM/NO → GND (INPUT_PULLUP; acionado LOW).
- HX711: DOUT→GPIO27; SCK→GPIO14; VCC→3V3; GND→GND.
- MP3-TF-1GP: modulo RX←GPIO26; modulo TX→GPIO25; modulo VCC→5V; GND comum.
- LED vermelho: GPIO23→resistor 330 ohms→anodo; catodo→GND.
- Alto-falante atual: SPK_1 → 47 ohms em serie → falante 8 ohms / 0,5 W → SPK_2.

**PENDENCIA DE SEGURANCA:** o nivel alto do TX do MP3, alimentado em 5 V,
nao foi medido. O ESP32 NAO tolera 5 V em suas entradas. Antes de manter
GPIO25 ligado diretamente ao TX do MP3, meca o sinal ou coloque um
conversor de nivel adequado. Nao presumir compatibilidade pelo teste funcional.

## Interface C++ para a Etapa 2 (orquestrador)
```cpp
Hidra::iniciar(callback);              // inicializa hardware, tara automatica
Hidra::atualizar();                    // chamar repetidamente em loop()
Hidra::LeituraHardware l = Hidra::obterLeitura();
Hidra::definirLed(true);               // liga ou desliga LED
Hidra::reproduzirFaixa(1);            // 1..8, arquivos /mp3/000N.mp3
Hidra::realizarTara();                // somente com plataforma vazia
```

`LeituraHardware::massa_g` em **gramas**; somente confiavel se `massa_valida` for `true`.
`instante_massa_ms` e `EventoFisico::instante_ms` sao valores monotonicamente relacionados a
`millis()` desde boot, **nao** horarios UTC. `botao_pressionado` e `chave_inicial_acionada`
sao estados fisicos de contatos com logica LOW ativo (expostos em booleano true quando acionados).
A microchave NAO comprova presenca de pessoa nem identifica o copo por si so.

O callback recebe `EventoFisico` com tipo `BotaoPressionado`, `BotaoLiberado`,
`ChaveInicialAcionada`, `ChaveInicialLiberada`, `MassaAtualizada`, `MassaIndisponivel`.
Apenas `MassaAtualizada` carrega `massa_g` significativo. Esta interface e **local em C++**;
NAO e o esquema `HidraEvent` do Firebase, que pertence as Etapas 2 e 3.

`reproduzirFaixa()` confirma apenas que o numero foi aceito e o pacote UART enviado;
NAO confirma o inicio da reproduçao. Nao ha callbacks de termino de audio neste driver.
`realizarTara()` e bloqueante (10 amostras), indicado somente para startup ou manutencao.
A entrada pode perder eventos durante uma tara. `atualizar()` usa leitura do HX711 a cada
200 ms, sem espera deliberada, e filtro exponencial alfa=0,35. O fluxo de massas nao
deve ser transmitido integralmente ao Firebase: o orquestrador escolhe snapshots relevantes.

## Faixas de audio
Coloque `/mp3/0001.mp3` ... `/mp3/0008.mp3` no cartao microSD FAT32.
O comando UART usado para selecionar faixas e `0x12`.

## Testes integrados, executar em ordem
1. Sem carga durante boot: esperar impressao de MASSA_ATUALIZADA proximo de 0 g.
2. Pressionar e soltar R13-507: eventos de botao uma vez por mudanca (debounce 30 ms).
3. Acionar e liberar KW11-7: eventos CHAVE_INICIAL correspondentes.
4. Aplicar uma massa conhecida (19 g ou 220 g): observar leituras em gramas e retirar.
5. Digitar `l1` e `l0`: LED acende/apaga.
6. Digitar `p1`, `p4` e `p8`: reproduz faixas correspondentes.
7. Digitar `s`: mostra snapshot dos sensores e validade de massa.
8. Digitar `t` SOMENTE com a balanca vazia: confirma TARA OK.
9. Testar uso simultaneo: pressionar botao/chave enquanto balanca le e solicitar MP3.
10. Se o HX711 falhar, verificar `massa_valida=0` e emissao de MASSA_INDISPONIVEL.

## Revisao 2
- Corrigido o temporizador que emite MassaIndisponivel quando a aquisicao se torna indisponivel por mais de TIMEOUT_MASSA_MS.

## Limites e ajustes futuros
- Fator de calibracao experimental: **425.631805 contagens ADC/grama**, obtido com 220 g.
  Calibracao e tara podem mudar com remontagem e temperatura.
- Meta da apostila: erro de massa dentro de +/-10 g e latencia de instrucao <2 s;
  a medicao completa de latencia exige a Etapa 2.
- O falante de 0,5 W com 47 ohms em serie e uma solucao provisoria. Nao elevar
  volume de maneira indiscriminada; observar aquecimento e distorcao.
- O arquivo `.ino` e um **programa de diagnostico**, nao orquestrador E0..E5.
- O modulo MP3 pode precisar de um pequeno tempo de inicializacao apos energizacao;
  se o primeiro comando de volume nao for aceito, reenviar volume depois do boot.
