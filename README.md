# HIDRA
Hidratação Inteligente por Detecção e Resposta Automatizada — PCS3930, Grupo 5.

Projeto de hidratação assistida com ESP32, sensores, instruções em áudio e
acompanhamento pelo cuidador. O desenvolvimento é dividido em aquisição de
hardware, orquestração local e comunicação IoT/aplicativo.

## Organização

| Caminho | Conteúdo |
|---|---|
| [HIDRA_Etapa1](HIDRA_Etapa1/README.md) | Aquisição dos sensores e atuação local; projeto Arduino IDE |
| [HIDRA_Etapa2](HIDRA_Etapa2/README.md) | Hardware da etapa 1, orquestrador E0–E5 e interface IoT |
| `tests/` | Testes do orquestrador e firmware no computador |
| [HIDRA_Etapa3](HIDRA_Etapa3/README.md) | Módulo IoT, Firebase, simulador Python, aplicativo Android e testes |
| `Hardware/` | Programas de teste individual dos componentes |
| `Audios/` | Faixas de áudio do dispositivo |
| `Documentos/` | Documentos de referência do projeto |

## Documentação técnica

- [Relatório oficial do grupo](Documentos/README.md).

- [Hardware e firmware da etapa 1](documentacao_hardware.md).
- [Orquestrador da etapa 2](documentacao_orquestrador.md).
- [IoT, Firebase e aplicativo da etapa 3](documentacao_iot_firebase_mobile.md).
- [Contrato dos eventos remotos](HIDRA_Etapa3/docs/CONTRATO.md).
- [Interface da etapa 3 com o orquestrador](HIDRA_Etapa3/docs/INTEGRACAO.md).
- [Configuração do Firebase no plano gratuito Spark](HIDRA_Etapa3/docs/FIREBASE_SPARK.md).
- [Validação da etapa 3](HIDRA_Etapa3/docs/VALIDACAO.md).

Os eventos físicos da etapa 1 e os eventos de atividade publicados pela etapa 3
são interfaces diferentes. A etapa 2 interpreta as observações de hardware,
mantém os estados da atividade e entrega os eventos processados ao módulo IoT.

O ambiente `esp32-integrated` compila hardware, orquestrador e transporte Firebase em um único firmware, reutilizando o código da etapa 2:

```bash
pio run -d HIDRA_Etapa3/firmware -e esp32-integrated
```

Configuração, gravação e operação estão em [INTEGRACAO.md](HIDRA_Etapa3/docs/INTEGRACAO.md). Os ambientes de diagnóstico e a compilação Arduino IDE com stub permanecem disponíveis.
