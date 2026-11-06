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

async function getDevices() {
  const { rows } = await pool.query(
    'SELECT device_id, online, state, updated_at FROM device_status ORDER BY device_id'
  );
  return rows;
}

async function getEvents(deviceId, limit) {
  const { rows } = await pool.query(
    `SELECT time, type, state FROM alarm_events
     WHERE device_id = $1
     ORDER BY time DESC
     LIMIT $2`,
    [deviceId, limit]
  );
  return rows;
}

// time_bucket je TimescaleDB funkcija: grupiše vremena u intervale (ovde po 1 sat)
async function getHourlyMotion(deviceId, hours) {
  const { rows } = await pool.query(
    `SELECT time_bucket('1 hour', time) AS hour, count(*)::int AS motions
     FROM alarm_events
     WHERE device_id = $1
       AND type = 'motion'
       AND time > now() - make_interval(hours => $2)
     GROUP BY hour
     ORDER BY hour`,
    [deviceId, hours]
  );
  return rows;
}

module.exports = { pool, insertEvent, upsertStatus, getDevices, getEvents, getHourlyMotion };
