"""Testes REST reais. Executar com os emuladores Auth e RTDB ativos."""
import os
import tempfile
import subprocess
import sys
import unittest
from pathlib import Path
from scripts.seed_emulator import seed, PASSWORD
from simulator.firebase_client import FirebaseClient, RemoteError, sync_one
from simulator.scenarios import scenario
from simulator.store import Journal


@unittest.skipUnless(os.environ.get('HIDRA_TEST_EMULATOR') == '1', 'Requer emuladores locais')
class EmulatorTests(unittest.TestCase):
    def setUp(self):
        seed()
        self.client = self.client_for('device')
        self.temp = tempfile.TemporaryDirectory()
        self.journal = Journal(Path(self.temp.name) / 'journal.db')

    def tearDown(self):
        self.journal.close(); self.temp.cleanup()

    def client_for(self, role):
        return FirebaseClient('http://127.0.0.1:9000?ns=demo-hidra-default-rtdb', 'demo-api-key',
                              role + '@hidra.test', PASSWORD, '127.0.0.1:9099')

    def test_publish_replay_and_summary(self):
        events = scenario('alert')
        for event in events: self.journal.append(event)
        while sync_one(self.journal, self.client): pass
        path = f"devices/sim_hidra_01/sessions/{events[0]['session_id']}"
        value, _ = self.client.call(path)
        first_received = value['events'][events[0]['event_id']]['received_at_ms']
        self.client.publish_event(events[0])
        second, _ = self.client.call(path)
        self.assertEqual(second['events'][events[0]['event_id']]['received_at_ms'], first_received)
        self.assertEqual(len(second['events']), len(events))
        self.assertEqual(second['summary']['instruction_count'], 1)
        self.assertEqual(second['summary']['alert_count'], 1)
        self.assertNotIn('active_alerts', second['summary'])
        self.client_for('caregiver').call(path)
        with self.assertRaises(RemoteError): self.client_for('other').call(path)

    def test_auth_refresh_and_presence(self):
        self.client.authenticate()
        self.client.expires = 0
        self.client.publish_status('sim_hidra_01', {'boot_id': 'test', 'state': 'E0', 'pending_count': 0, 'dropped_count': 0})
        value, _ = self.client.call('devices/sim_hidra_01/status')
        self.assertIsInstance(value['last_seen_ms'], int)

    def test_interrupted_session(self):
        events = scenario('interrupted')
        for event in events: self.journal.append(event)
        while sync_one(self.journal, self.client): pass
        summary, _ = self.client.call(f"devices/sim_hidra_01/sessions/{events[0]['session_id']}/summary")
        self.assertEqual(summary['result'], 'aborted')
        self.assertEqual(len(summary['active_alerts']), 1)

    def test_presentation_script_publishes_current_session(self):
        root = Path(__file__).resolve().parents[1]
        subprocess.run([sys.executable, str(root / 'scripts/demo_emulator.py'), '--no-pause'],
                       cwd=root, check=True, capture_output=True, text=True, timeout=30)
        status, _ = self.client.call('devices/sim_hidra_01/status')
        value, _ = self.client.call('devices/sim_hidra_01/sessions/' + status['session_id'])
        self.assertEqual(status['state'], 'E5')
        self.assertEqual(status['pending_count'], 0)
        self.assertEqual(len(value['events']), 10)
        self.assertEqual(value['summary']['result'], 'completed')
        self.assertNotIn('active_alerts', value['summary'])
