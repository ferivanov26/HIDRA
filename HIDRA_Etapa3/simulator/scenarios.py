import time
import uuid
from .contract import validate


def scenario(name='normal', device='sim_hidra_01', boot=None, start_ms=None):
    boot = boot or uuid.uuid4().hex[:16]
    start_ms = int(time.time() * 1000) if start_ms is None else start_ms
    steps = [('activity_started', 'E0', {'trigger': 'button'})]
    if name == 'interrupted':
        steps += [('state_changed', 'E1', {'previous_state': 'E0'}),
                  ('instruction_played', 'E1', {'instruction_id': 'position_cup', 'attempt': 1}),
                  ('caregiver_alert', 'E1', {'alert_id': boot + '_a1', 'reason': 'timeout', 'attempt': 3}),
                  ('activity_aborted', 'E1', {'reason': 'local_cancel'})]
    else:
        for i in range(1, 6):
            data = {'previous_state': f'E{i-1}'}
            if i == 3:
                data['mass_g'] = 185.0
            steps.append(('state_changed', f'E{i}', data))
            if i == 2 and name == 'alert':
                steps += [('instruction_played', 'E2', {'instruction_id': 'fill_cup', 'attempt': 1}),
                          ('caregiver_alert', 'E2', {'alert_id': boot + '_a1', 'reason': 'timeout', 'attempt': 3}),
                          ('alert_resolved', 'E2', {'alert_id': boot + '_a1', 'reason': 'progress'})]
        steps.append(('activity_completed', 'E5', {}))
    return [validate(dict(schema_version=1, device_id=device, session_id=boot + '_s001',
                          event_id=f'{boot}_e{i:05}', sequence=i, type=kind, state=state,
                          boot_id=boot, uptime_ms=i*1000, occurred_at_ms=start_ms+i*1000,
                          **({'data': data} if data else {})))
            for i, (kind, state, data) in enumerate(steps, 1)]
