// Kontrolna tabla: čita podatke iz REST API-ja i osvežava se na svakih 5 s

const REFRESH_MS = 5000;

const STATE_LABELS = {
  DISARMED: 'Isključen',
  ARMED: 'Uključen',
  TRIGGERED: 'ALARM',
};

const TYPE_LABELS = {
  motion: 'Pokret',
  state: 'Promena stanja',
};

const deviceSelect = document.getElementById('device-select');
const stateBadge = document.getElementById('state-badge');
const onlineBadge = document.getElementById('online-badge');
const updatedAt = document.getElementById('updated-at');
const eventsBody = document.getElementById('events-body');
const commandMsg = document.getElementById('command-msg');
const armBtn = document.getElementById('arm-btn');
const disarmBtn = document.getElementById('disarm-btn');

let chart = null;

// Vreme iz baze je u UTC-u; prikazujemo ga po lokalnom vremenu
function formatTime(iso) {
  return new Date(iso).toLocaleString('sr-RS');
}

async function getJson(url) {
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`${url}: ${res.status}`);
  }
  return res.json();
}

function selectedDevice() {
  return deviceSelect.value;
}

async function loadDevices() {
  const devices = await getJson('/api/devices');
  const current = selectedDevice();

  deviceSelect.replaceChildren(...devices.map((d) => new Option(d.device_id, d.device_id)));
  if (devices.some((d) => d.device_id === current)) {
    deviceSelect.value = current;
  }
  return devices;
}

function renderStatus(device) {
  if (!device) {
    stateBadge.textContent = 'Nema uređaja';
    stateBadge.className = 'badge';
    onlineBadge.textContent = '–';
    onlineBadge.className = 'badge';
    updatedAt.textContent = '–';
    return;
  }
  stateBadge.textContent = STATE_LABELS[device.state] || '–';
  stateBadge.className = `badge ${(device.state || '').toLowerCase()}`;
  onlineBadge.textContent = device.online ? 'Online' : 'Offline';
  onlineBadge.className = `badge ${device.online ? 'online' : 'offline'}`;
  updatedAt.textContent = formatTime(device.updated_at);
}

function renderEvents(events) {
  eventsBody.replaceChildren(...events.map((e) => {
    const row = document.createElement('tr');
    for (const text of [formatTime(e.time), TYPE_LABELS[e.type] || e.type, STATE_LABELS[e.state] || e.state]) {
      const cell = document.createElement('td');
      cell.textContent = text;
      row.appendChild(cell);
    }
    return row;
  }));
}

// Baza vraća samo sate u kojima je bilo pokreta; ostale popunjavamo nulom
function fillHours(rows) {
  const counts = new Map(rows.map((r) => [new Date(r.hour).getTime(), r.motions]));
  const hourMs = 60 * 60 * 1000;
  const lastHour = Math.floor(Date.now() / hourMs) * hourMs;
  const labels = [];
  const values = [];
  for (let i = 23; i >= 0; i--) {
    const t = lastHour - i * hourMs;
    labels.push(new Date(t).toLocaleTimeString('sr-RS', { hour: '2-digit', minute: '2-digit' }));
    values.push(counts.get(t) || 0);
  }
  return { labels, values };
}

function renderChart(rows) {
  const { labels, values } = fillHours(rows);
  if (!chart) {
    chart = new Chart(document.getElementById('hourly-chart'), {
      type: 'bar',
      data: { labels, datasets: [{ label: 'Broj pokreta', data: values, backgroundColor: '#2563eb' }] },
      options: {
        animation: false,
        aspectRatio: 3,
        plugins: { legend: { display: false } },
        scales: { y: { beginAtZero: true, ticks: { precision: 0 } } },
      },
    });
  } else {
    chart.data.labels = labels;
    chart.data.datasets[0].data = values;
    chart.update();
  }
}

async function refresh() {
  try {
    const devices = await loadDevices();
    const id = selectedDevice();
    renderStatus(devices.find((d) => d.device_id === id));
    armBtn.disabled = disarmBtn.disabled = !id;
    if (!id) {
      renderEvents([]);
      renderChart([]);
      return;
    }
    const [events, hourly] = await Promise.all([
      getJson(`/api/devices/${encodeURIComponent(id)}/events?limit=20`),
      getJson(`/api/devices/${encodeURIComponent(id)}/stats/hourly?hours=24`),
    ]);
    renderEvents(events);
    renderChart(hourly);
  } catch (err) {
    console.error(err);
    commandMsg.textContent = 'Server trenutno nije dostupan.';
  }
}

async function sendCommand(cmd) {
  const id = selectedDevice();
  if (!id) {
    return;
  }
  commandMsg.textContent = 'Šaljem komandu...';
  try {
    const res = await fetch(`/api/devices/${encodeURIComponent(id)}/command`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd }),
    });
    const body = await res.json();
    commandMsg.textContent = res.ok
      ? 'Komanda poslata. Stanje će se osvežiti kad uređaj potvrdi.'
      : `Greška: ${body.error}`;
  } catch (err) {
    commandMsg.textContent = 'Komanda nije poslata: server nije dostupan.';
  }
  // Uređaju treba trenutak da primi komandu i javi novo stanje
  setTimeout(refresh, 1500);
}

armBtn.addEventListener('click', () => sendCommand('arm'));
disarmBtn.addEventListener('click', () => sendCommand('disarm'));
deviceSelect.addEventListener('change', refresh);

refresh();
setInterval(refresh, REFRESH_MS);
