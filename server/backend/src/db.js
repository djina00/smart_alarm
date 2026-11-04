// Rad sa bazom (TimescaleDB / PostgreSQL).
// Podaci za povezivanje se čitaju iz promenljivih okruženja PGHOST, PGUSER...
const { Pool } = require('pg');

const pool = new Pool();

async function insertEvent(deviceId, type, state) {
  await pool.query(
    'INSERT INTO alarm_events (device_id, type, state) VALUES ($1, $2, $3)',
    [deviceId, type, state]
  );
}

// Upisuje novo stanje uređaja, ili menja postojeće ako uređaj već postoji
async function upsertStatus(deviceId, online, state) {
  await pool.query(
    `INSERT INTO device_status (device_id, online, state, updated_at)
     VALUES ($1, $2, $3, now())
     ON CONFLICT (device_id) DO UPDATE
       SET online = EXCLUDED.online,
           state = COALESCE(EXCLUDED.state, device_status.state),
           updated_at = now()`,
    [deviceId, online, state]
  );
}

module.exports = { pool, insertEvent, upsertStatus };
