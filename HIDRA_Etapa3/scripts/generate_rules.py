"""Gera regras RTDB v1: validação explícita e negação de campos extras."""
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from simulator.contract import FIELDS


def check(expression):
    return {'.validate': expression}


def integer(minimum=0):
    return f'newData.isNumber() && newData.val() >= {minimum} && newData.val() % 1 == 0'


text = "newData.isString() && newData.val().matches(/^[A-Za-z0-9_-]{1,63}$/)"
state = "newData.isString() && newData.val().matches(/^E[0-5]$/)"
owner = "auth != null && root.child('deviceOwners').child($device).val() == auth.uid"
read = "auth != null && (root.child('deviceOwners').child($device).val() == auth.uid || root.child('caregiverDevices').child(auth.uid).child($device).val() == true)"
kind = "newData.parent().child('type').val()"
variants = []
for name, (required, optional) in FIELDS.items():
    has = "newData.hasChildren(" + json.dumps(sorted(required)) + ")" if required else 'true'
    variants.append(f"({kind} == '{name}' && {has})")
data = {'.validate': ' || '.join(variants), '$other': check('false')}
for field in sorted(set().union(*(a | b for a, b in FIELDS.values()))):
    kinds = [f"{kind} == '{name}'" for name, (a, b) in FIELDS.items() if field in a | b]
    validation = integer(1) if field in ('attempt', 'session_order') else text
    if field == 'previous_state':
        validation = state
    if field in ('mass_g', 'estimated_intake_ml'):
        validation = 'newData.isNumber() && newData.val() >= 0'
    if field == 'trigger':
        validation = "newData.val() == 'button' || newData.val() == 'schedule'"
    if field == 'reason':
        validation += f" && ({kind} != 'activity_aborted' || newData.val() == 'restart' || newData.val() == 'local_cancel')"
    data[field] = check(('(' + ' || '.join(kinds) + ') && (' + validation + ')').replace("newData.parent().child('type')", "newData.parent().parent().child('type')"))
required = ['schema_version', 'device_id', 'session_id', 'event_id', 'sequence', 'type', 'state', 'boot_id', 'uptime_ms']
payload = {
    '.validate': 'newData.hasChildren(' + json.dumps(required) + ') && (' +
        " || ".join(f"newData.child('type').val() == '{t}'" for t in ('activity_completed',)) +
        " || newData.hasChild('data')) && (newData.child('type').val() != 'activity_started' || newData.child('state').val() == 'E0') && (newData.child('type').val() != 'activity_completed' || newData.child('state').val() == 'E5')",
    'schema_version': check('newData.isNumber() && newData.val() == 1'),
    'device_id': check(text + ' && newData.val() == $device'),
    'session_id': check(text + ' && newData.val() == $session'),
    'event_id': check(text + ' && newData.val() == $event'),
    'sequence': check(integer(1)),
    'type': check(' || '.join(f"newData.val() == '{name}'" for name in FIELDS)),
    'state': check(state), 'boot_id': check(text), 'uptime_ms': check(integer()),
    'occurred_at_ms': check(integer()), 'data': data, '$other': check('false'),
}
summary_required = ['session_order', 'result', 'last_sequence', 'last_state', 'instruction_count', 'alert_count', 'history_incomplete']
summary = {
    '.write': owner + ' && newData.exists()',
    '.validate': 'newData.hasChildren(' + json.dumps(summary_required) + ") && (!data.exists() || (newData.child('last_sequence').val() >= data.child('last_sequence').val() && newData.child('session_order').val() == data.child('session_order').val()))",
    'session_order': check(integer(1)), 'last_sequence': check(integer(1)),
    'last_state': check(state), 'instruction_count': check(integer()), 'alert_count': check(integer()),
    'result': check("newData.val() == 'in_progress' || newData.val() == 'completed' || newData.val() == 'aborted'"),
    'history_incomplete': check('newData.isBoolean()'),
    'started_at_ms': check(integer()), 'ended_at_ms': check(integer()),
    'estimated_intake_ml': check('newData.isNumber() && newData.val() >= 0'),
    'active_alerts': {'$alert': {'.validate': "newData.hasChildren(['reason'])",
                              'reason': check(text), 'occurred_at_ms': check(integer()), '$other': check('false')}},
    '$other': check('false'),
}
status = {'.write': owner + ' && newData.exists()',
          '.validate': "newData.hasChildren(['boot_id','state','last_seen_ms','pending_count','dropped_count'])",
          'boot_id': check(text), 'session_id': check(text), 'state': check(state),
          'last_seen_ms': check('newData.isNumber() && newData.val() == now'),
          'observed_at_ms': check(integer()), 'pending_count': check(integer()), 'dropped_count': check(integer()),
          'activity_active': check('newData.isBoolean()'),
          'sequence': check(integer()), 'attempt': check(integer()),
          'active_alert_count': check(integer()), 'mass_available': check('newData.isBoolean()'),
          'mass_g': check('newData.isNumber()'),
          '$other': check('false')}
rules = {'rules': {'.read': False, '.write': False,
         'caregiverDevices': {'$uid': {'.read': 'auth != null && auth.uid == $uid'}},
         'deviceOwners': {},
         'devices': {'$device': {'.read': read,
             'status': status,
             'sessions': {'.indexOn': ['summary/session_order'], '$session': {
                 'summary': summary,
                 'events': {'.indexOn': ['payload/sequence'], '$event': {
                     '.write': owner + ' && !data.exists() && newData.exists()',
                     '.validate': "newData.hasChildren(['payload','received_at_ms'])",
                     'payload': payload, 'received_at_ms': check('newData.isNumber() && newData.val() == now'),
                     '$other': check('false')}}}}}}}}
path = Path(__file__).resolve().parents[1] / 'firebase/database.rules.json'
path.write_text(json.dumps(rules, indent=2) + '\n')
print(path)
