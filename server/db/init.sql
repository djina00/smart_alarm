-- Šema baze pametnog alarma (izvršava se samo pri prvom pokretanju baze)

CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Događaji sa uređaja: pokret dok je alarm uključen i promene stanja.
-- Ovo je vremenska serija: svaki red ima vreme kad se događaj desio.
CREATE TABLE IF NOT EXISTS alarm_events (
    time       TIMESTAMPTZ NOT NULL DEFAULT now(),
    device_id  TEXT        NOT NULL,
    type       TEXT        NOT NULL,   -- 'motion' ili 'state'
    state      TEXT        NOT NULL    -- 'DISARMED', 'ARMED' ili 'TRIGGERED'
);

-- Hypertable: TimescaleDB tabelu interno deli po vremenu (u delove po danima),
-- pa su upiti tipa "događaji u poslednja 24 h" brzi i kad podataka ima mnogo
SELECT create_hypertable('alarm_events', 'time',
                         chunk_time_interval => INTERVAL '1 day',
                         if_not_exists => TRUE);

CREATE INDEX IF NOT EXISTS alarm_events_device_time_idx
    ON alarm_events (device_id, time DESC);

-- Poslednje poznato stanje svakog uređaja (jedan red po uređaju)
CREATE TABLE IF NOT EXISTS device_status (
    device_id   TEXT        PRIMARY KEY,
    online      BOOLEAN     NOT NULL,
    state       TEXT,
    updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
