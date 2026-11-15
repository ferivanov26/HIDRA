# Contrato HIDRA v1

Contrato de comunicação entre o orquestrador, o módulo IoT, o simulador e o aplicativo Android.

## Eventos

Campos obrigatórios: `schema_version=1`, `device_id`, `session_id`, `event_id`, `sequence` inteiro positivo, `type`, `state` em E0–E5, `boot_id` e `uptime_ms`. `occurred_at_ms` é omitido enquanto UTC não estiver válido. `data` contém somente os campos permitidos para o tipo. Nenhum campo opcional é gravado como `null`.

Identificadores e códigos textuais: 1 a 63 caracteres ASCII alfanuméricos, `_` ou `-`. Trata-se de um fechamento do contrato para impedir chaves inválidas no Firebase e manter buffers fixos. O firmware limita o JSON a 1024 bytes. A enumeração C++ é serializada nos códigos textuais definidos neste contrato.

| Tipo | Estado / dados |
|---|---|
| `activity_started` | E0; `trigger`: button ou schedule; `session_order` opcional, inteiro positivo da NVS |
| `state_changed` | `previous_state`; `mass_g` opcional, finita e não negativa |
| `instruction_played` | `instruction_id`, `attempt` inteiro positivo |
| `caregiver_alert` | `alert_id`, `reason`, `attempt` inteiro positivo |
| `alert_resolved` | `alert_id`, `reason` |
| `activity_completed` | E5; `estimated_intake_ml` opcional, finita e não negativa |
| `activity_aborted` | `reason`: restart ou local_cancel |

`session_id` e `sequence` vêm da etapa 2. No firmware integrado, `boot_id` é compartilhado com `hidra_boot_id()`; o adapter preserva `data.session_order` e o resumo usa esse mesmo ordinal. Publicadores legados sem o campo geram a ordem localmente. `event_id` e horários são acrescentados na entrada do módulo IoT, antes da persistência. Reenvios preservam o evento inteiro. Reiniciar o módulo não transforma uma sessão aberta em concluída.

## Caminhos e resumos

```text
caregiverDevices/{caregiver_uid}/{device_id}: true
deviceOwners/{device_id}: device_uid
devices/{device_id}/metadata
devices/{device_id}/status
devices/{device_id}/sessions/{session_id}/summary
devices/{device_id}/sessions/{session_id}/events/{event_id}/payload
devices/{device_id}/sessions/{session_id}/events/{event_id}/received_at_ms
```

O resumo contém `session_order`, `result`, `last_sequence`, `last_state`, `instruction_count`, `alert_count` e `history_incomplete`. Horários `started_at_ms` e `ended_at_ms` só são incluídos quando conhecidos. `estimated_intake_ml` só é incluído quando fornecido no encerramento. `active_alerts/{alert_id}` tem `reason` e `occurred_at_ms` opcional; remove-se o item ao receber a resolução explícita. Sem alertas ativos, omite-se o mapa.

`history_incomplete` sinaliza lacunas ou perdas identificadas pelo publicador. O resumo não confirma ingestão. `session_order` cresce por identidade de dispositivo; não apague o diário/metadata do publicador e continue publicando com a mesma identidade sem reprovisionamento.

`status` tem `boot_id`, `state`, `last_seen_ms`, `pending_count` e `dropped_count`. `session_id` é opcional enquanto nenhuma sessão está selecionada. `observed_at_ms` só aparece quando o horário da observação é conhecido. `last_seen_ms` e `received_at_ms` usam timestamp do servidor. O snapshot integrado acrescenta os campos opcionais `activity_active` (booleano), `sequence`, `attempt`, `active_alert_count` (inteiros não negativos), `mass_available` (booleano) e `mass_g` (número finito, admitindo negativo como diagnóstico de tara). Ausência desses campos mantém compatibilidade com os publicadores anteriores. O status vem de um snapshot atual separado; drenar histórico antigo não altera a sessão atual.

## Publicação e recuperação

1. Ler o caminho estável do evento com ETag.
2. Se vazio, criar por PUT condicional, com timestamp de servidor.
3. Se o payload já for idêntico, reconhecer confirmação anterior; se diferente, isolar por integridade.
4. Publicar resumo cumulativo por atribuição; `last_sequence` não retrocede.
5. Confirmar a pendência local somente depois do resumo.

Um único publicador mantém a ordem de cada identidade. As regras impedem edição/exclusão de eventos e regressão de resumo; não recalculam agregações nem executam a máquina de estados. O dispositivo autorizado continua responsável pela correção semântica do resumo.

No desktop, SQLite retém eventos confirmados para recalcular os snapshots e facilitar diagnóstico. No ESP32, o diário pendente é removido após confirmação e o snapshot cumulativo é mantido separadamente em LittleFS. Ambos preservam contagens em reenvios. No ESP32, o ordinal global do diário é reservado em Flash para ordenar registros entre reinicializações, mesmo sem UTC.
