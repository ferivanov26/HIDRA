"""Provisionamento administrativo SOMENTE no emulador, nunca na nuvem."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from simulator.firebase_client import request, RemoteError

AUTH = 'http://127.0.0.1:9099/identitytoolkit.googleapis.com/v1/accounts:'
DATABASE = 'http://127.0.0.1:9000/.json?ns=demo-hidra-default-rtdb'
PASSWORD = 'hidra-demo-123'


def seed():
    users = {}
    for role in ('device', 'caregiver', 'other'):
        credentials = {'email': role + '@hidra.test', 'password': PASSWORD, 'returnSecureToken': True}
        try:
            user, _ = request(AUTH + 'signUp?key=demo-api-key', 'POST', credentials)
        except RemoteError:
            user, _ = request(AUTH + 'signInWithPassword?key=demo-api-key', 'POST', credentials)
        users[role] = user['localId']
    # Apenas a preparação usa privilégios de emulador; testes usam UIDs comuns.
    request(DATABASE, 'PATCH', {
        'deviceOwners': {'sim_hidra_01': users['device']},
        'caregiverDevices': {users['caregiver']: {'sim_hidra_01': True}},
        'devices': {'sim_hidra_01': {'metadata': {'label': 'HIDRA — simulador', 'schema_version': 1, 'source': 'simulator'}}}
    }, {'Authorization': 'Bearer owner'})
    return users


if __name__ == '__main__':
    seed()
    print('Emulador provisionado. Cuidador: caregiver@hidra.test; senha fictícia: hidra-demo-123')
