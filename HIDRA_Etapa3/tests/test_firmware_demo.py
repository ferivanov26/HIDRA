"""Executa o main.cpp real com substitutos de Serial e transporte, sem hardware."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('g++'), 'Requer g++ para testar o firmware no computador')
class FirmwareDemoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.temp.name) / 'demo-test'
        command = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                   '-DHIDRA_SYNTHETIC_DEMO', '-I' + str(ROOT / 'tests/firmware'),
                   '-I' + str(ROOT / 'firmware/iot/include'),
                   str(ROOT / 'tests/firmware/demo_harness.cpp'),
                   str(ROOT / 'firmware/iot/src/main.cpp'), '-o', str(cls.binary)]
        subprocess.run(command, check=True, env={**os.environ, 'CCACHE_DISABLE': '1'})

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def check_case(self, name):
        subprocess.run([str(self.binary), name], check=True)

    def test_normal_session_and_unique_session_ids(self):
        self.check_case('normal')

    def test_initialization_failure_blocks_sessions(self):
        self.check_case('uninitialized')

    def test_recovered_session_is_aborted_before_new_session(self):
        self.check_case('recovery')

    def test_failed_recovery_blocks_new_session_until_retry(self):
        self.check_case('recovery_retry')

    def test_full_queue_preserves_session_and_sequence_until_retry(self):
        self.check_case('backpressure')
