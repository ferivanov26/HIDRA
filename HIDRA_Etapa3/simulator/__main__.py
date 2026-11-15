import argparse
import json
import os
from pathlib import Path
import random
import time
from .contract import summarize
from .firebase_client import FirebaseClient, RemoteError, IntegrityError, sync_one
from .scenarios import scenario
from .store import Journal


def main():
    parser = argparse.ArgumentParser(description='Simulador HIDRA — somente dados fictícios')
    parser.add_argument('--scenario', choices=['normal', 'interrupted', 'alert'], default='normal')
    parser.add_argument('--device', default='sim_hidra_01')
    parser.add_argument('--output', default='.runtime/scenario.json')
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--resume', action='store_true', help='Enviar apenas pendências já persistidas')
    parser.add_argument('--journal', default='.runtime/simulator.db')
    args = parser.parse_args()
    events = [] if args.resume else scenario(args.scenario, args.device)
    if not args.resume:
        path = Path(args.output)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({'events': events, 'summary': summarize(events, 1)}, indent=2, ensure_ascii=False))
        print(f'Cenário fictício salvo em {path} ({len(events)} eventos)')
    if not args.publish:
        return
    names = ['HIDRA_DATABASE_URL', 'HIDRA_API_KEY', 'HIDRA_DEVICE_EMAIL', 'HIDRA_DEVICE_PASSWORD']
    if any(not os.environ.get(name) for name in names):
        parser.error('Defina ' + ', '.join(names) + '; não informe senhas na linha de comando')
    Path(args.journal).parent.mkdir(parents=True, exist_ok=True)
    journal = Journal(args.journal)
    for event in events:
        journal.append(event)
    client = FirebaseClient(*(os.environ[name] for name in names),
                            auth_emulator=os.environ.get('FIREBASE_AUTH_EMULATOR_HOST'))
    failures = 0
    try:
        while journal.pending_count():
            try:
                sync_one(journal, client)
                failures = 0
            except (RemoteError, IntegrityError, ValueError) as error:
                print(str(error))
                if isinstance(error, RemoteError) and error.status not in (400, 403):
                    time.sleep(min(60, 2 ** min(failures + 1, 6)) + random.random())
                    failures += 1
        if events:
            last = events[-1]
            client.publish_status(args.device, {'boot_id': last['boot_id'], 'session_id': last['session_id'],
                                  'state': last['state'], 'observed_at_ms': int(time.time()*1000),
                                  'pending_count': 0, 'dropped_count': 0})
        print('Pendências processadas. Verifique diagnósticos de registros isolados, se houver.')
    finally:
        journal.close()


if __name__ == '__main__':
    main()
