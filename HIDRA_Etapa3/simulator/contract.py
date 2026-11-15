"""Contrato v1 compartilhado conceitualmente por firmware, simulador e Android."""
import json
import math
import re

STATES = {f'E{i}' for i in range(6)}
FIELDS = {
    'activity_started': ({'trigger'}, {'session_order'}),
    'state_changed': ({'previous_state'}, {'mass_g'}),
    'instruction_played': ({'instruction_id', 'attempt'}, set()),
    'caregiver_alert': ({'alert_id', 'reason', 'attempt'}, set()),
    'alert_resolved': ({'alert_id', 'reason'}, set()),
    'activity_completed': (set(), {'estimated_intake_ml'}),
    'activity_aborted': ({'reason'}, set()),
}
REQUIRED = {'schema_version', 'device_id', 'session_id', 'event_id', 'sequence',
            'type', 'state', 'boot_id', 'uptime_ms'}
OPTIONAL = {'occurred_at_ms', 'data'}
KEY = re.compile(r'^[A-Za-z0-9_-]{1,63}$')


def natural(value, minimum=0):
    return type(value) is int and value >= minimum


def validate(event):
    if not isinstance(event, dict) or not REQUIRED <= event.keys() or event.keys() - REQUIRED - OPTIONAL:
        raise ValueError('Campos obrigatórios ausentes ou campos desconhecidos')
    if type(event['schema_version']) is not int or event['schema_version'] != 1:
        raise ValueError('schema_version deve ser 1')
    for field in ('device_id', 'session_id', 'event_id', 'boot_id'):
        if not isinstance(event[field], str) or not KEY.fullmatch(event[field]):
            raise ValueError(f'Identificador inválido: {field}')
    if not natural(event['sequence'], 1) or not natural(event['uptime_ms']):
        raise ValueError('Sequência ou uptime inválido')
    if 'occurred_at_ms' in event and not natural(event['occurred_at_ms']):
        raise ValueError('Horário inválido')
    if event['state'] not in STATES or event['type'] not in FIELDS:
        raise ValueError('Estado ou tipo inválido')
    required, optional = FIELDS[event['type']]
    data = event.get('data', {})
    if not isinstance(data, dict) or not required <= data.keys() or data.keys() - required - optional:
        raise ValueError('Dados incompatíveis com o tipo de evento')
    for field, value in data.items():
        if field in ('attempt', 'session_order') and not natural(value, 1):
            raise ValueError('Tentativa e ordem devem ser inteiros positivos')
        if field in ('mass_g', 'estimated_intake_ml') and (
            type(value) not in (int, float) or not math.isfinite(value) or value < 0
        ):
            raise ValueError('Medição inválida')
        if field in ('instruction_id', 'alert_id', 'reason') and (
            not isinstance(value, str) or not KEY.fullmatch(value)
        ):
            raise ValueError(f'Texto inválido: {field}')
    if 'previous_state' in data and data['previous_state'] not in STATES:
        raise ValueError('Estado anterior inválido')
    if event['type'] == 'activity_started' and (event['state'] != 'E0' or data['trigger'] not in ('button', 'schedule')):
        raise ValueError('Início inválido')
    if event['type'] == 'activity_completed' and event['state'] != 'E5':
        raise ValueError('Conclusão fora de E5')
    if event['type'] == 'activity_aborted' and data['reason'] not in ('restart', 'local_cancel'):
        raise ValueError('Motivo de interrupção inválido')
    if len(encode(event).encode()) > 1024:
        raise ValueError('Evento excede 1024 bytes')
    return event


def encode(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False, allow_nan=False)


def summarize(events, session_order, incomplete=False):
    """Recalcula snapshot; reenvios idênticos não incrementam contadores."""
    unique = {}
    for event in events:
        validate(event)
        key = event['event_id']
        if key in unique and unique[key] != event:
            raise ValueError('Colisão de event_id')
        unique[key] = event
    ordered = sorted(unique.values(), key=lambda e: e['sequence'])
    if not ordered:
        raise ValueError('Sessão vazia')
    if len({(e['device_id'], e['session_id']) for e in ordered}) != 1:
        raise ValueError('Mistura de sessões')
    sequences = [e['sequence'] for e in ordered]
    if len(set(sequences)) != len(sequences):
        raise ValueError('Sequência repetida com identificadores distintos')
    if not natural(session_order, 1):
        raise ValueError('session_order inválido')
    producer_order = next((e.get('data', {}).get('session_order') for e in ordered if e['type'] == 'activity_started' and 'session_order' in e.get('data', {})), None)
    if producer_order is not None:
        session_order = producer_order
    result = dict(session_order=session_order, result='in_progress',
                  last_sequence=ordered[-1]['sequence'], last_state=ordered[-1]['state'],
                  instruction_count=0, alert_count=0,
                  history_incomplete=incomplete or sequences != list(range(1, sequences[-1] + 1)))
    active = {}
    for e in ordered:
        kind, data = e['type'], e.get('data', {})
        if kind == 'activity_started' and 'occurred_at_ms' in e:
            result['started_at_ms'] = e['occurred_at_ms']
        if kind == 'instruction_played':
            result['instruction_count'] += 1
        if kind == 'caregiver_alert':
            result['alert_count'] += 1
            active[data['alert_id']] = {'reason': data['reason']}
            if 'occurred_at_ms' in e:
                active[data['alert_id']]['occurred_at_ms'] = e['occurred_at_ms']
        if kind == 'alert_resolved':
            active.pop(data['alert_id'], None)
        if kind in ('activity_completed', 'activity_aborted'):
            result['result'] = 'completed' if kind == 'activity_completed' else 'aborted'
            if 'occurred_at_ms' in e:
                result['ended_at_ms'] = e['occurred_at_ms']
            if 'estimated_intake_ml' in data:
                result['estimated_intake_ml'] = data['estimated_intake_ml']
    if active:
        result['active_alerts'] = active
    return result
