"""Contrato e adaptador reais da etapa 2, compilados no computador."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from simulator.contract import validate, summarize

ROOT = Path(__file__).resolve().parents[1]
STAGE2 = ROOT.parent / 'HIDRA_Etapa2/src'


def compile_bridge(binary):
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-DHIDRA_INTEGRATED_ORCHESTRATOR', '-DHIDRA_IOT_STUB=0',
                    '-I' + str(ROOT / 'tests/firmware'),
                    '-I' + str(ROOT / 'firmware/iot/include'), '-I' + str(STAGE2),
                    str(ROOT / 'tests/firmware/orchestrator_bridge.cpp'),
                    str(ROOT / 'firmware/iot/src/orchestrator_adapter.cpp'),
                    str(STAGE2 / 'contrato/HidraEvent.cpp'),
                    *map(str, (STAGE2 / 'orquestrador').glob('*.cpp')),
                    '-o', str(binary)], check=True, env={**os.environ, 'CCACHE_DISABLE': '1'})


def bridge_events(binary, scenario):
    result = subprocess.run([str(binary), scenario], check=True, text=True, capture_output=True)
    return [validate(json.loads(line)) for line in result.stdout.splitlines()]


@unittest.skipUnless(shutil.which('g++'), 'Requer g++')
class OrchestratorBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temp.name) / 'bridge'
        compile_bridge(cls.binary)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_normal_cycle_preserves_order_and_optional_volume(self):
        events = bridge_events(self.binary, 'normal')
        summary = summarize(events, 1)
        self.assertEqual(summary['session_order'], 38)
        self.assertEqual(summary['result'], 'completed')
        self.assertEqual(summary['estimated_intake_ml'], 150)
        self.assertEqual(events[0]['boot_id'], '10001')
        self.assertTrue(all(e['session_id'].startswith(e['boot_id']) for e in events))

    def test_alert_is_resolved_by_real_orchestrator(self):
        events = bridge_events(self.binary, 'alert')
        summary = summarize(events, 1)
        self.assertGreater(summary['alert_count'], 0)
        self.assertNotIn('active_alerts', summary)
        self.assertEqual(summary['result'], 'completed')

    def test_restart_keeps_session_and_continues_sequence_with_new_boot(self):
        events = bridge_events(self.binary, 'restart')
        self.assertEqual(len({e['session_id'] for e in events}), 1)
        self.assertEqual(events[-1]['type'], 'activity_aborted')
        self.assertEqual(events[-1]['data']['reason'], 'restart')
        self.assertEqual(events[-1]['boot_id'], '10002')
        self.assertEqual([e['sequence'] for e in events], list(range(1, len(events) + 1)))

    def test_return_to_microswitch_base_does_not_invent_volume(self):
        events = bridge_events(self.binary, 'base')
        summary = summarize(events, 1)
        self.assertEqual(summary['result'], 'completed')
        self.assertNotIn('estimated_intake_ml', summary)
