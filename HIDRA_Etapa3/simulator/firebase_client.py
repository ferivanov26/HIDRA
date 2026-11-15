"""Cliente REST sem credenciais administrativas e sem dependências externas."""
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from .contract import encode, summarize


class RemoteError(Exception):
    def __init__(self, status):
        super().__init__(f'Operação remota recusada (HTTP {status})')
        self.status = status


class IntegrityError(Exception):
    pass


def request(url, method='GET', value=None, headers=None):
    data = None if value is None else encode(value).encode()
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={'Content-Type': 'application/json', **(headers or {})})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            body = response.read()
            return (json.loads(body) if body else None), response.headers
    except urllib.error.HTTPError as error:
        raise RemoteError(error.code) from None
    except (urllib.error.URLError, TimeoutError, OSError):
        # Não incluir URL: a query contém token de acesso.
        raise RemoteError(0) from None


class FirebaseClient:
    def __init__(self, database_url, api_key, email, password, auth_emulator=None):
        self.base = database_url.rstrip('/')
        if not self.base.startswith('https://') and not self.base.startswith('http://127.0.0.1:'):
            raise ValueError('Banco exige HTTPS ou emulador em loopback')
        self.key, self.email, self.password = api_key, email, password
        self.emulator = auth_emulator
        self.token = self.refresh = None
        self.expires = 0

    def authenticate(self):
        if self.token and time.monotonic() < self.expires:
            return
        if self.refresh:
            url = self._auth_url('securetoken.googleapis.com', '/v1/token')
            body = urllib.parse.urlencode({'grant_type': 'refresh_token', 'refresh_token': self.refresh}).encode()
            req = urllib.request.Request(url, data=body, headers={'Content-Type': 'application/x-www-form-urlencoded'})
            try:
                with urllib.request.urlopen(req, timeout=5) as response:
                    value = json.load(response)
                self.token, self.refresh = value['id_token'], value['refresh_token']
                self.expires = time.monotonic() + int(value['expires_in']) - 60
                return
            except (urllib.error.URLError, OSError, KeyError, ValueError):
                self.refresh = None
        try:
            value, _ = request(self._auth_url('identitytoolkit.googleapis.com', '/v1/accounts:signInWithPassword'),
                               'POST', {'email': self.email, 'password': self.password, 'returnSecureToken': True})
        except RemoteError as error:
            # Credenciais inválidas não tornam os eventos inválidos: preservar o diário.
            raise RemoteError(401 if error.status in (400, 401, 403) else error.status) from None
        self.token, self.refresh = value['idToken'], value['refreshToken']
        self.expires = time.monotonic() + int(value['expiresIn']) - 60

    def _auth_url(self, host, path):
        prefix = f'http://{self.emulator}/{host}' if self.emulator else f'https://{host}'
        return prefix + path + '?key=' + urllib.parse.quote(self.key)

    def call(self, path, method='GET', value=None, headers=None):
        self.authenticate()
        base = urllib.parse.urlsplit(self.base)
        query = urllib.parse.parse_qsl(base.query) + [('auth', self.token)]
        url = urllib.parse.urlunsplit((base.scheme, base.netloc, base.path.rstrip('/') + '/' + path + '.json',
                                      urllib.parse.urlencode(query), ''))
        try:
            return request(url, method, value, headers)
        except RemoteError as error:
            if error.status == 401:
                self.expires = 0
            raise

    def publish_event(self, event):
        path = f"devices/{event['device_id']}/sessions/{event['session_id']}/events/{event['event_id']}"
        old, headers = self.call(path, headers={'X-Firebase-ETag': 'true'})
        if old is not None:
            if old.get('payload') != event:
                raise IntegrityError('Colisão de event_id remoto')
            return
        self.call(path, 'PUT', {'payload': event, 'received_at_ms': {'.sv': 'timestamp'}},
                  {'if-match': headers['ETag']})

    def publish_summary(self, event, snapshot):
        self.call(f"devices/{event['device_id']}/sessions/{event['session_id']}/summary", 'PUT', snapshot)

    def publish_status(self, device, status):
        self.call(f'devices/{device}/status', 'PUT', {**status, 'last_seen_ms': {'.sv': 'timestamp'}})


def sync_one(journal, client):
    item = journal.pending()
    if item is None:
        return False
    row_id, event = item
    try:
        client.publish_event(event)
        events, order, incomplete = journal.session_snapshot(event)
        client.publish_summary(event, summarize(events, order, incomplete))
    except IntegrityError:
        journal.quarantine(row_id)
        raise
    except ValueError:
        journal.quarantine(row_id)
        raise
    except RemoteError as error:
        if error.status in (400, 403):
            journal.quarantine(row_id)
        raise
    journal.confirm(row_id)
    return True
