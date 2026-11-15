"""Diário SQLite exclusivo do simulador desktop; ESP32 usa LittleFS."""
import hashlib
import json
import sqlite3
from .contract import encode, validate


class Journal:
    def __init__(self, path, capacity=200):
        self.db = sqlite3.connect(path)
        self.capacity = capacity
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.executescript('''
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY, event_id TEXT UNIQUE, device TEXT, session TEXT,
                payload TEXT, checksum TEXT, state TEXT DEFAULT 'pending');
            CREATE TABLE IF NOT EXISTS sessions (
                session_order INTEGER PRIMARY KEY AUTOINCREMENT, device TEXT, session TEXT,
                incomplete INTEGER DEFAULT 0, UNIQUE(device, session));
        ''')

    def append(self, event):
        validate(event)
        payload = encode(event)
        old = self.db.execute('SELECT payload FROM events WHERE event_id=?', (event['event_id'],)).fetchone()
        if old:
            if old[0] != payload:
                raise ValueError('Colisão de event_id local')
            return
        if self.pending_count() >= self.capacity:
            raise OverflowError('Diário cheio')
        with self.db:
            self.db.execute('INSERT OR IGNORE INTO sessions(device,session) VALUES (?,?)',
                            (event['device_id'], event['session_id']))
            self.db.execute('INSERT INTO events(event_id,device,session,payload,checksum) VALUES (?,?,?,?,?)',
                            (event['event_id'], event['device_id'], event['session_id'], payload,
                             hashlib.sha256(payload.encode()).hexdigest()))

    def pending_count(self):
        return self.db.execute("SELECT count(*) FROM events WHERE state='pending'").fetchone()[0]

    def pending(self):
        row = self.db.execute("SELECT id,payload,checksum FROM events WHERE state='pending' ORDER BY id LIMIT 1").fetchone()
        if row is None:
            return None
        if hashlib.sha256(row[1].encode()).hexdigest() != row[2]:
            self.quarantine(row[0])
            raise ValueError('Integridade do diário inválida; registro isolado')
        try:
            return row[0], validate(json.loads(row[1]))
        except (ValueError, TypeError):
            self.quarantine(row[0])
            raise ValueError('Registro persistido inválido; isolado para diagnóstico') from None

    def session_snapshot(self, event):
        row = self.db.execute('SELECT session_order,incomplete FROM sessions WHERE device=? AND session=?',
                              (event['device_id'], event['session_id'])).fetchone()
        rows = self.db.execute("SELECT payload FROM events WHERE device=? AND session=? AND state='confirmed' ORDER BY id",
                               (event['device_id'], event['session_id'])).fetchall()
        return [json.loads(r[0]) for r in rows] + [event], row[0], bool(row[1])

    def confirm(self, row_id):
        with self.db:
            self.db.execute("UPDATE events SET state='confirmed' WHERE id=?", (row_id,))

    def quarantine(self, row_id):
        with self.db:
            self.db.execute('UPDATE sessions SET incomplete=1 WHERE (device,session) IN '
                            '(SELECT device,session FROM events WHERE id=?)', (row_id,))
            self.db.execute("UPDATE events SET state='quarantined' WHERE id=?", (row_id,))

    def close(self):
        self.db.close()
