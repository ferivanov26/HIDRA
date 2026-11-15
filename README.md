# HIDRA
Repositório destinado para o projeto HIDRA

Projeto de hidratação assistida com ESP32, sensores, instruções em áudio e
acompanhamento pelo cuidador. O desenvolvimento é dividido em aquisição de
hardware, orquestração local e comunicação IoT/aplicativo.

## Organização

| Caminho | Conteúdo |
|---|---|
| [HIDRA_Etapa1](HIDRA_Etapa1/README.md) | Aquisição dos sensores e atuação local; projeto Arduino IDE |
| [HIDRA_Etapa3](HIDRA_Etapa3/README.md) | Módulo IoT, Firebase, simulador Python, aplicativo Android e testes |
| `Hardware/` | Programas de teste individual dos componentes |
| `Audios/` | Faixas de áudio do dispositivo |
| `Documentos/` | Documentos de referência do projeto |

## Documentação técnica

- [Hardware e firmware da etapa 1](documentacao_hardware.md).
- [IoT, Firebase e aplicativo da etapa 3](documentacao_iot_firebase_mobile.md).
- [Contrato dos eventos remotos](HIDRA_Etapa3/docs/CONTRATO.md).
- [Interface da etapa 3 com o orquestrador](HIDRA_Etapa3/docs/INTEGRACAO.md).

Os eventos físicos da etapa 1 e os eventos de atividade publicados pela etapa 3
são interfaces diferentes. A etapa 2 interpreta as observações de hardware,
mantém os estados da atividade e entrega os eventos processados ao módulo IoT.

O firmware de hardware e o de comunicação são projetos separados; cada um
possui seu próprio programa de diagnóstico e configuração de compilação.
