import {readFile} from 'node:fs/promises';
import {before, after, beforeEach, test} from 'node:test';
import {initializeTestEnvironment, assertSucceeds, assertFails} from '@firebase/rules-unit-testing';
import {ref, set, get, remove, serverTimestamp} from 'firebase/database';
let env;
before(async () => {
  env = await initializeTestEnvironment({projectId: 'demo-hidra', database: {
    host: '127.0.0.1', port: 9000, rules: await readFile('database.rules.json', 'utf8')}});
});
after(async () => { await env?.cleanup(); });
beforeEach(async () => {
  await env.clearDatabase();
  await env.withSecurityRulesDisabled(async context => {
    await set(ref(context.database()), {deviceOwners: {sim_hidra_01: 'device1'},
      caregiverDevices: {caregiver1: {sim_hidra_01: true}},
      devices: {sim_hidra_01: {metadata: {label: 'SIMULADOR', schema_version: 1, source: 'simulator'}}}});
  });
});
const path = 'devices/sim_hidra_01/sessions/b1_s1/events/b1_e1';
const event = () => ({payload: {schema_version: 1, device_id: 'sim_hidra_01', session_id: 'b1_s1',
  event_id: 'b1_e1', sequence: 1, type: 'activity_started', state: 'E0', boot_id: 'b1',
  uptime_ms: 0, data: {trigger: 'button'}}, received_at_ms: serverTimestamp()});
const db = uid => env.authenticatedContext(uid).database();
test('Dispositivo cria evento; cuidador vinculado lê', async () => {
  await assertSucceeds(set(ref(db('device1'), path), event()));
  await assertSucceeds(get(ref(db('caregiver1'), path)));
});
test('Anônimo e cuidador sem vínculo não leem; leitura global negada', async () => {
  await assertFails(get(ref(env.unauthenticatedContext().database(), 'devices/sim_hidra_01')));
  await assertFails(get(ref(db('other'), 'devices/sim_hidra_01')));
  await assertFails(get(ref(db('caregiver1'), 'devices')));
});
test('Cuidador não publica, não altera vínculos nem metadados', async () => {
  await assertFails(set(ref(db('caregiver1'), path), event()));
  await assertFails(set(ref(db('caregiver1'), 'caregiverDevices/caregiver1/other'), true));
  await assertFails(set(ref(db('device1'), 'devices/sim_hidra_01/metadata/label'), 'changed'));
});
test('Evento é imutável, inclusive em reenvio; exclusão negada', async () => {
  const device = db('device1');
  await assertSucceeds(set(ref(device, path), event()));
  await assertFails(set(ref(device, path), event()));
  await assertFails(remove(ref(device, path)));
});
test('Recusa estados, sequências, campos e payloads inválidos', async () => {
  for (const change of [e => e.payload.state = 'E9', e => e.payload.sequence = 1.5,
    e => e.payload.sequence = -1, e => e.payload.device_id = 'other',
    e => e.payload.extra = 'x', e => e.extra = true, e => e.payload.data.mass_g = 1,
    e => delete e.payload.data.trigger, e => e.received_at_ms = 1]) {
    const e = event(); change(e); await assertFails(set(ref(db('device1'), path), e));
  }
});
test('Resumo não retrocede e não pode ser excluído', async () => {
  const r = ref(db('device1'), 'devices/sim_hidra_01/sessions/b1_s1/summary');
  const value = {session_order: 1, result: 'in_progress', last_sequence: 2, last_state: 'E1',
    instruction_count: 0, alert_count: 0, history_incomplete: false};
  await assertSucceeds(set(r, value));
  await assertSucceeds(set(r, value));
  await assertFails(set(r, {...value, last_sequence: 1}));
  await assertFails(set(r, {...value, session_order: 2}));
  await assertFails(remove(r));
});
test('Status só é escrito pelo dispositivo dono', async () => {
  const value = {boot_id: 'b1', state: 'E0', last_seen_ms: serverTimestamp(), pending_count: 0, dropped_count: 0};
  const status = 'devices/sim_hidra_01/status';
  await assertSucceeds(set(ref(db('device1'), status), value));
  await assertFails(set(ref(db('other'), status), value));
  await assertFails(remove(ref(db('device1'), status)));
});
test('Contrato da etapa 2 aceita session_order positivo somente no início', async () => {
  const valid = event(); valid.payload.data.session_order = 38;
  await assertSucceeds(set(ref(db('device1'), path), valid));
  for (const order of [0, -1, 1.5, '38']) {
    const invalid = event(); invalid.payload.data.session_order = order;
    invalid.payload.event_id = 'bad_order';
    await assertFails(set(ref(db('device1'), path.replace('b1_e1', 'bad_order')), invalid));
  }
  const invalid = event(); invalid.payload.type = 'state_changed';
  invalid.payload.data = {previous_state: 'E0', session_order: 38};
  invalid.payload.event_id = 'bad_type';
  await assertFails(set(ref(db('device1'), path.replace('b1_e1', 'bad_type')), invalid));
});
test('Snapshot do orquestrador preserva estado ocioso e diagnóstico físico', async () => {
  const status = 'devices/sim_hidra_01/status';
  const value = {boot_id: '10001', state: 'E5', last_seen_ms: serverTimestamp(),
    pending_count: 0, dropped_count: 0, activity_active: false, sequence: 9,
    attempt: 0, active_alert_count: 0, mass_available: true, mass_g: -0.5};
  await assertSucceeds(set(ref(db('device1'), status), value));
  await assertFails(set(ref(db('device1'), status), {...value, activity_active: 'false'}));
  await assertFails(set(ref(db('device1'), status), {...value, sequence: -1}));
  const extra = event(); extra.activity_active = false;
  await assertFails(set(ref(db('device1'), path), extra));
});
