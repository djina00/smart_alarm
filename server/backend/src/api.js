// REST API koji koristi kontrolna tabla
const express = require('express');
const db = require('./db');
const mqtt = require('./mqtt');

const VALID_COMMANDS = ['arm', 'disarm'];

const router = express.Router();

// Spisak uređaja sa poslednjim stanjem
router.get('/devices', async (req, res, next) => {
  try {
    res.json(await db.getDevices());
  } catch (err) {
    next(err);
  }
});

// Poslednji događaji jednog uređaja (najnoviji prvi)
router.get('/devices/:id/events', async (req, res, next) => {
  const limit = Math.min(parseInt(req.query.limit, 10) || 50, 500);
  try {
    res.json(await db.getEvents(req.params.id, limit));
  } catch (err) {
    next(err);
  }
});

// Broj pokreta po satu za poslednjih N sati (za grafikon)
router.get('/devices/:id/stats/hourly', async (req, res, next) => {
  const hours = Math.min(parseInt(req.query.hours, 10) || 24, 168);
  try {
    res.json(await db.getHourlyMotion(req.params.id, hours));
  } catch (err) {
    next(err);
  }
});

// Komanda uređaju: { "cmd": "arm" } ili { "cmd": "disarm" }
router.post('/devices/:id/command', (req, res) => {
  const cmd = req.body && req.body.cmd;
  if (!VALID_COMMANDS.includes(cmd)) {
    return res.status(400).json({ error: 'cmd mora biti "arm" ili "disarm"' });
  }
  if (!mqtt.sendCommand(req.params.id, cmd)) {
    return res.status(503).json({ error: 'Nema veze sa MQTT brokerom' });
  }
  // 202: komanda je poslata, a uređaj će javiti novo stanje preko MQTT-a
  res.status(202).json({ sent: cmd });
});

module.exports = router;
