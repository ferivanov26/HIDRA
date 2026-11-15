from copy import deepcopy
import tempfile
import unittest
from pathlib import Path
from simulator.contract import summarize, validate
from simulator.scenarios import scenario
from simulator.store import Journal
from simulator.firebase_client import IntegrityError, RemoteError, sync_one


class MemoryFirebase:
    def __init__(self):
        self.events = {}
        self.summaries = {}
        self.fail_summary = False
        self.lose_response = False

    def publish_event(self, event):
        old = self.events.setdefault(event['event_id'], (deepcopy(event), 123))
        if old[0] != event:
            raise IntegrityError('Colisão')
        if self.lose_response:
            self.lose_response = False
            raise RemoteError(0)

    def publish_summary(self, event, summary):
        if self.fail_summary:
            self.fail_summary = False
            raise RemoteError(0)
        self.summaries[event['session_id']] = summary


class ContractTests(unittest.TestCase):
    def test_normal_session(self):
        result = summarize(scenario(boot='b1', start_ms=0), 1)
        self.assertEqual(result['result'], 'completed')
        self.assertEqual(result['last_sequence'], 7)
        self.assertFalse(result['history_incomplete'])

    def test_alert_resolution_and_replay(self):
        events = scenario('alert', boot='b1', start_ms=0)
        result = summarize(events + events, 1)
        self.assertEqual(result['alert_count'], 1)
        self.assertEqual(result['instruction_count'], 1)
        self.assertNotIn('active_alerts', result)

    def test_interrupted_session_preserves_active_alert(self):
        result = summarize(scenario('interrupted', boot='b1', start_ms=0), 1)
        self.assertEqual(result['result'], 'aborted')
        self.assertIn('b1_a1', result['active_alerts'])

    def test_no_utc_and_missing_event(self):
        events = scenario(boot='b1', start_ms=0)
        for event in events:
            event.pop('occurred_at_ms')
        result = summarize(events[:2] + events[3:], 1)
        self.assertTrue(result['history_incomplete'])
        self.assertNotIn('started_at_ms', result)

    def test_reject_invalid(self):
        event = scenario()[0]
        for field, value in [('sequence', True), ('sequence', 0), ('state', 'E8'),
                             ('extra', 'x'), ('device_id', '../escape'), ('schema_version', 2)]:
            invalid = deepcopy(event); invalid[field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                validate(invalid)
        invalid = deepcopy(event); invalid['data']['mass_g'] = 10
        with self.assertRaises(ValueError): validate(invalid)

    def test_sequence_collision(self):
        events = scenario()
        events[1]['sequence'] = 1
        with self.assertRaises(ValueError): summarize(events, 1)


class JournalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'journal.db'
        self.journal = Journal(self.path)
        self.remote = MemoryFirebase()

    def tearDown(self):
        self.journal.close()
        self.temp.cleanup()

    def test_reboot_and_offline_queue(self):
        events = scenario('alert')
        for event in events: self.journal.append(event)
        self.journal.close()
        self.journal = Journal(self.path)
        while sync_one(self.journal, self.remote): pass
        self.assertEqual(list(self.remote.events), [e['event_id'] for e in events])
        self.assertEqual(self.journal.pending_count(), 0)

    def test_lost_response_preserves_single_record(self):
        event = scenario()[0]
        self.journal.append(event)
        self.remote.lose_response = True
        with self.assertRaises(RemoteError): sync_one(self.journal, self.remote)
        self.assertEqual(self.journal.pending_count(), 1)
        sync_one(self.journal, self.remote)
        self.assertEqual(len(self.remote.events), 1)
        self.assertEqual(self.remote.events[event['event_id']][1], 123)

    def test_summary_failure_and_process_restart(self):
        for event in scenario('alert'): self.journal.append(event)
        sync_one(self.journal, self.remote)
        self.remote.fail_summary = True
        with self.assertRaises(RemoteError): sync_one(self.journal, self.remote)
        self.journal.close(); self.journal = Journal(self.path)
        while sync_one(self.journal, self.remote): pass
        result = next(iter(self.remote.summaries.values()))
        self.assertEqual(result['instruction_count'], 1)
        self.assertEqual(result['alert_count'], 1)

    def test_capacity(self):
        self.journal.capacity = 1
        events = scenario()
        self.journal.append(events[0])
        with self.assertRaises(OverflowError): self.journal.append(events[1])

    def test_corruption_is_quarantined(self):
        self.journal.append(scenario()[0])
        self.journal.db.execute("UPDATE events SET checksum='invalid'")
        self.journal.db.commit()
        with self.assertRaises(ValueError): self.journal.pending()
        self.assertEqual(self.journal.pending_count(), 0)

    def test_session_order_persists(self):
        first, second = scenario(), scenario()
        self.journal.append(first[0]); sync_one(self.journal, self.remote)
        self.journal.close(); self.journal = Journal(self.path)
        self.journal.append(second[0]); sync_one(self.journal, self.remote)
        self.assertLess(self.remote.summaries[first[0]['session_id']]['session_order'],
                        self.remote.summaries[second[0]['session_id']]['session_order'])

    def test_quarantined_record_does_not_block_next(self):
        events = scenario()
        for event in events: self.journal.append(event)
        self.remote.events[events[0]['event_id']] = ({'different': True}, 123)
        with self.assertRaises(IntegrityError): sync_one(self.journal, self.remote)
        while sync_one(self.journal, self.remote): pass
        self.assertTrue(self.remote.summaries[events[0]['session_id']]['history_incomplete'])

    def test_duplicate_sequence_is_isolated_without_stalling_queue(self):
        events = scenario()
        events[1]['sequence'] = events[0]['sequence']
        for event in events: self.journal.append(event)
        sync_one(self.journal, self.remote)
        with self.assertRaises(ValueError): sync_one(self.journal, self.remote)
        while sync_one(self.journal, self.remote): pass
        self.assertEqual(self.journal.pending_count(), 0)
        self.assertTrue(self.remote.summaries[events[0]['session_id']]['history_incomplete'])


if __name__ == '__main__': unittest.main()
