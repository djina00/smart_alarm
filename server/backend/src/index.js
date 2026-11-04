// Bekend pametnog alarma: prijava na MQTT teme uređaja i upis u bazu.
//   <koren>/<uređaj>/events  -> tabela alarm_events (vremenska serija)
//   <koren>/<uređaj>/status  -> tabela device_status (poslednje stanje)
const mqtt = require('mqtt');
const db = require('./db');

const MQTT_URL = process.env.MQTT_URL || 'mqtt://localhost:1883';
const TOPIC_ROOT = process.env.MQTT_TOPIC_ROOT || 'smart_alarm';

const VALID_TYPES = ['motion', 'state'];
const VALID_STATES = ['DISARMED', 'ARMED', 'TRIGGERED'];

function parseJson(text) {
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

async function handleEvent(deviceId, data) {
  if (!VALID_TYPES.includes(data.type) || !VALID_STATES.includes(data.state)) {
    console.warn(`[${deviceId}] neispravan dogadjaj, preskacem:`, data);
    return;
  }
  await db.insertEvent(deviceId, data.type, data.state);
  console.log(`[${deviceId}] upisan dogadjaj: ${data.type} (${data.state})`);
}

async function handleStatus(deviceId, data) {
  const online = data.online === true;
  // Last Will poruka ({"online":false}) nema stanje, pa ostaje staro
  const state = VALID_STATES.includes(data.state) ? data.state : null;
  await db.upsertStatus(deviceId, online, state);
  console.log(`[${deviceId}] status: ${online ? 'online' : 'offline'}${state ? `, ${state}` : ''}`);
}

const client = mqtt.connect(MQTT_URL, {
  clientId: `smart-alarm-backend-${Math.random().toString(16).slice(2, 8)}`,
  reconnectPeriod: 5000,
  // Javni test brokeri ponekad sporo odgovaraju, pa čekamo duže
  connectTimeout: 30000,
});

client.on('connect', () => {
  console.log(`Povezan na MQTT broker ${MQTT_URL}`);
  // + zamenjuje jedan deo teme, pa ovo hvata sve uređaje
  client.subscribe([`${TOPIC_ROOT}/+/events`, `${TOPIC_ROOT}/+/status`], { qos: 1 });
});

client.on('message', async (topic, payload) => {
  const [root, deviceId, kind] = topic.split('/');
  if (root !== TOPIC_ROOT || !deviceId) {
    return;
  }

  const data = parseJson(payload.toString());
  if (data === null) {
    console.warn(`[${deviceId}] poruka nije JSON: ${payload}`);
    return;
  }

  try {
    if (kind === 'events') {
      await handleEvent(deviceId, data);
    } else if (kind === 'status') {
      await handleStatus(deviceId, data);
    }
  } catch (err) {
    console.error(`[${deviceId}] greska pri upisu u bazu:`, err.message);
  }
});

client.on('reconnect', () => console.log('Ponovno povezivanje na MQTT broker...'));
client.on('error', (err) => console.error('MQTT greska:', err.message || String(err)));

// Uredno gašenje kad Docker zaustavi kontejner
process.on('SIGTERM', async () => {
  client.end();
  await db.pool.end();
  process.exit(0);
});
