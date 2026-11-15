"""Apresentação controlada de dados fictícios, exclusivamente nos emuladores locais."""
import argparse
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from simulator.firebase_client import FirebaseClient, sync_one
from simulator.scenarios import scenario
from simulator.store import Journal
from scripts.seed_emulator import PASSWORD


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', choices=['normal', 'alert', 'interrupted'], default='alert')
    parser.add_argument('--no-pause', action='store_true', help='Executar sem pausa para captura de tela')
    args = parser.parse_args()
    events = scenario(args.scenario)
    folder = Path(__file__).resolve().parents[1] / '.runtime'
    folder.mkdir(exist_ok=True)
    journal = Journal(folder / ('presentation-' + events[0]['boot_id'] + '.db'))
    client = FirebaseClient('http://127.0.0.1:9000?ns=demo-hidra-default-rtdb',
                            'demo-api-key', 'device@hidra.test', PASSWORD, '127.0.0.1:9099')
    try:
        for event in events:
            journal.append(event)
        for event in events:
            sync_one(journal, client)
            client.publish_status(event['device_id'], {
                'boot_id': event['boot_id'], 'session_id': event['session_id'],
                'state': event['state'], 'observed_at_ms': int(time.time() * 1000),
                'pending_count': journal.pending_count(), 'dropped_count': 0})
            print(f"{event['sequence']}: {event['type']} ({event['state']})", flush=True)
            if event['type'] == 'caregiver_alert' and not args.no_pause:
                input('Alerta ativo: capture a tela; pressione Enter para continuar. ')
        print('Sessão fictícia: ' + events[0]['session_id'])
    finally:
        journal.close()


if __name__ == '__main__':
    main()
