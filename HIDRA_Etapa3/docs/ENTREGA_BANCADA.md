# Gravação do firmware integrado e teste com o aplicativo

Este roteiro usa uma ESP32 clássica com 4 MB de Flash, compatível com a placa `esp32dev` e com a pinagem do projeto. Para ESP32-C3/S3 ou outra montagem, conferir placa e pinos antes de gravar.

## Conteúdo e responsabilidade de cada parte

O pacote contém `HIDRA_Etapa2/` e `HIDRA_Etapa3/firmware/`, com a mesma posição relativa do repositório. Não mover essas pastas: `platformio.ini` referencia a etapa 2 como biblioteca local. O ambiente **esp32-integrated** inclui o sketch da etapa 2, os drivers de sensores/atuadores, o orquestrador E0–E5 e o transporte Wi-Fi/Firebase da etapa 3. Basta gravar esse firmware único; gravar depois um sketch antigo da etapa 1 ou o stub da etapa 2 substitui a aplicação integrada.

O pacote distribui código-fonte, não um binário já configurado. Não contém senhas, conta do cuidador, dependências baixadas nem APK. O APK **cloud** é entregue separadamente. As oito faixas existentes são fornecidas em `Audios/`. Se o cartão ainda não estiver preparado, copiá-las para a pasta `/mp3/` do cartão do módulo, mantendo os nomes `0001.mp3` a `0008.mp3`. A gravação da ESP32 não copia esses áudios para o cartão.

## 1. Preparar o computador e a configuração

1. Instalar VS Code e a extensão PlatformIO IDE. Abrir o terminal do PlatformIO, onde o comando `pio` está disponível. Também é possível usar PlatformIO Core já instalado.
2. Extrair o ZIP e abrir a pasta `HIDRA_BANCADA`. Manter as duas pastas de etapas lado a lado. Todos os comandos abaixo partem dessa pasta principal. A primeira compilação precisa de internet para baixar as versões fixadas das dependências.
3. Copiar `HIDRA_Etapa3/firmware/iot/include/secrets.example.h` para `secrets.h` **na mesma pasta**. Editar somente os valores entre aspas:

| Campo | Valor necessário |
|---|---|
| `HIDRA_DEVICE_ID` | Identificador provisionado no banco; por exemplo, `hidra_01` |
| `HIDRA_SSID` | Nome da rede Wi-Fi 2,4 GHz disponível na bancada |
| `HIDRA_WIFI_PASSWORD` | Senha dessa rede |
| `HIDRA_DATABASE_URL` | URL completa do Realtime Database usado pelo APK |
| `HIDRA_API_KEY` | Chave Web API do mesmo projeto Firebase |
| `HIDRA_EMAIL` | E-mail da conta Firebase Authentication **do dispositivo** |
| `HIDRA_PASSWORD` | Senha dessa conta do dispositivo |

Manter a inclusão de `firebase_ca.h` e o certificado público. Não colocar a conta do cuidador na ESP32. Compartilhar a senha do dispositivo por um canal privado, separado do ZIP público; não enviar o arquivo preenchido ao Git. Caracteres `"` e `\` em valores precisam do escape correspondente em uma string C++.

O responsável pelo Firebase deve confirmar que a conta do dispositivo está habilitada, que `deviceOwners/{device_id}` contém seu UID e que `caregiverDevices/{uid_cuidador}/{device_id}` vale `true`. O cuidador entra no APK com sua própria conta. Se forem usados o banco e os vínculos existentes, não é necessário recriá-los nem importar dados. Para ensaios físicos, confirmar `devices/{device_id}/metadata/source = "hardware"`; nos ensaios fictícios anteriores, esse campo foi definido como `simulator`. A mudança é feita no Console Firebase. Manter as regras já publicadas.

Não executar simultaneamente outra ESP32 ou simulador publicando com a mesma identidade `hidra_01`. Para dois dispositivos simultâneos, provisionar IDs e contas distintos.

## 2. Compilar e identificar a porta

```text
pio device list
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated
```

A porta pode ser `COM5` no Windows ou `/dev/ttyUSB0` / `/dev/ttyACM0` no Linux. Substituir `COM5` nos próximos comandos pela porta real. Fechar o monitor serial antes de gravar. O PlatformIO seleciona automaticamente a tabela de partições `default.csv`, equivalente a **Default 4MB with spiffs**, e utiliza LittleFS. Não é preciso configurar essa opção manualmente na Arduino IDE.

## 3. LittleFS: somente na preparação inicial

O firmware não formata automaticamente o diário. Se a placa ainda não tiver LittleFS compatível e não houver registros a preservar, inicializar uma única vez:

```text
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated -t buildfs
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated -t uploadfs --upload-port COM5
```

**`uploadfs` substitui toda a partição de arquivos, incluindo diário pendente e logs.** Não executar como atualização de rotina. Se houver dados antigos relevantes, preservá-los antes; uma falha de montagem precisa ser diagnosticada, não resolvida apagando dados indiscriminadamente. Não usar `erase_flash`: ele também elimina parâmetros e identificadores persistidos em NVS.

## 4. Gravar a aplicação completa

```text
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Se a placa não entrar automaticamente no modo de gravação, seguir o procedimento BOOT/EN específico da placa. No console serial, enviar comandos terminados em nova linha; se necessário, usar um terminal serial que permita selecionar esse final de linha. Sair do monitor PlatformIO com Ctrl+C.

A gravação da aplicação não substitui o conteúdo do cartão MP3. Em atualizações normais, usar somente `upload`, sem `uploadfs` ou apagamento completo.

## 5. Conferir a montagem

| Componente | GPIO da ESP32 |
|---|---|
| Botão de início, ativo em LOW | 18 |
| Microchave da base, INPUT_PULLUP | 19 |
| HX711 DOUT / SCK | 27 / 14 |
| TX ESP32 → RX do módulo MP3 | 26 |
| LED vermelho | 23 |

O retorno TX do MP3 → GPIO25 pode ficar desligado: o orquestrador não usa esse sinal, evitando aplicar 5 V à entrada da ESP32. Conferir alimentação, GND comum e esquema da montagem na documentação de hardware incluída.

Ligar com a plataforma de pesagem vazia, para a tara, e o copo na base da microchave. O fator de calibração atual está em `HIDRA_Etapa2/src/hardware/Config.h` (`FATOR_HX711_CONTAGENS_POR_GRAMA`): verificar a leitura com uma massa conhecida. Se o conjunto célula/HX711 exigir outro fator ou outros GPIOs, ajustar esse arquivo e recompilar. O comando `copo` define a massa do copo vazio; ele não substitui a calibração do sensor.

Sem atividade em andamento:

1. Enviar `s` e `cfg`; conferir sensores e configuração.
2. Com a plataforma vazia, enviar `t` se for necessário refazer a tara.
3. Colocar o copo vazio na plataforma, aguardar estabilizar e enviar `copo`. Depois devolvê-lo à base.
4. Conferir a microchave. Se a lógica estiver invertida, enviar `set chave 0`.
5. Testar os áudios existentes com `p1` a `p8`. Nenhuma faixa 9 está incluída ou exigida pela implementação atual.
6. Aguardar Wi-Fi/autenticação e enviar `s`: procurar `wifi=1`, `auth=1` e verificar o campo de erro. Pendências devem diminuir após os envios. Falhas de rede não devem impedir a operação local.

## 6. Teste ponta a ponta com o APK cloud

Usar o APK configurado para o mesmo projeto Firebase. Entrar com a conta do cuidador e selecionar o dispositivo físico. A presença é enviada aproximadamente a cada 30 s; após 90 s sem atualização o app indica comunicação desatualizada. Usar `s` para diagnóstico imediato na placa.

Registrar a versão do pacote, montagem, calibração, horários e capturas do app/console. Os três cenários mínimos são:

1. **Ciclo completo:** enviar `iniciar` ou pressionar o botão, retirar o copo da base, colocá-lo vazio na balança, adicionar água sobre a balança, retirar o copo e devolvê-lo à balança com menos água. Conferir E0–E5, instruções, conclusão, histórico e volume estimado por variação de massa. Esperar a estabilização entre ações; o filtro de massa leva aproximadamente 2 s. A base com microchave não pesa: voltar apenas à base pode concluir sem estimativa de volume.
2. **Espera e alerta:** com a atividade parada, usar `demo` para tempos curtos, iniciar e aguardar instruções/repetições/alerta. Manter o APK aberto, conferir um alerta novo e depois progredir fisicamente para conferir a resolução. Finalizar e usar `padrao` para restaurar os tempos normais.
3. **Rede e reinício:** interromper somente o Wi-Fi durante uma sessão, continuar algumas ações e restabelecer a rede. Conferir o envio posterior e ausência de duplicatas. Em ensaio separado, reiniciar durante uma sessão e conferir a interrupção por reinício; a atividade não é retomada automaticamente. Não apagar LittleFS/NVS entre essas verificações.

O aplicativo consulta alertas enquanto está aberto; não presumir notificações push em segundo plano. Os registros não confirmam ingestão de água. `instruction_played` comprova o comando emitido, não que o áudio começou: medir o atraso físico na bancada para avaliar RNF_LATENCIA. A fila em RAM pode perder itens em um corte abrupto antes de persistir; esse limite deve ser registrado nos ensaios.

A compilação e os testes automatizados já exercitam a integração lógica. Calibração, fiação, reprodução real do áudio e os cenários completos com sensores precisam ser validados nesta montagem.
