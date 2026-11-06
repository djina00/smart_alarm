// Bekend pametnog alarma:
//   - MQTT: prima događaje i stanje uređaja i upisuje ih u bazu (mqtt.js)
//   - HTTP: REST API i kontrolna tabla na http://localhost:3000 (api.js, public/)
const path = require('path');
const express = require('express');
const db = require('./db');
const mqtt = require('./mqtt');
const api = require('./api');

const PORT = parseInt(process.env.PORT, 10) || 3000;

const app = express();
app.use(express.json());
app.use('/api', api);

// Kontrolna tabla (statične stranice) i biblioteka za grafikone
app.use(express.static(path.join(__dirname, '..', 'public')));
app.use('/vendor/chart.js', express.static(path.join(__dirname, '..', 'node_modules', 'chart.js', 'dist')));

// Greške iz ruta: zapiši u log, a klijentu vrati kratku poruku
app.use((err, req, res, next) => {
  console.error('Greska u API-ju:', err.message);
  res.status(500).json({ error: 'Greska na serveru' });
});

mqtt.start();
const server = app.listen(PORT, () => {
  console.log(`Kontrolna tabla: http://localhost:${PORT}`);
});

// Uredno gašenje kad Docker zaustavi kontejner
process.on('SIGTERM', async () => {
  mqtt.stop();
  server.close();
  await db.pool.end();
  process.exit(0);
});
