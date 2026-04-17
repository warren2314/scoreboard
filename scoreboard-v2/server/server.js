const express = require('express');
const path = require('path');
const https = require('https');
const fs = require('fs');
const { exec } = require('child_process');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

const app = express();
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

const HTTP_PORT = parseInt(process.env.HTTP_PORT || '80', 10);
const SERIAL_PATH = process.env.SERIAL_PORT || '/dev/ttyACM0';
const SERIAL_BAUD = parseInt(process.env.SERIAL_BAUD || '57600', 10);
const ADMIN_TOKEN = process.env.ADMIN_TOKEN || 'changeme';

// Current score state (kept in memory so /api/status can return it)
const state = {
  total: '--0',
  wickets: '0',
  overs: '-0',
  batsmanA: '--0',
  batsmanB: '--0',
  target: '--0',
  dls: '--0',
  serialConnected: false,
  lastResponse: null
};

// --- Serial port ---

let port = null;

function openSerial() {
  if (SERIAL_PATH === 'none') {
    console.log('Serial port disabled (SERIAL_PORT=none). Running in mock mode.');
    state.serialConnected = false;
    return;
  }

  // Close any existing port before reopening
  if (port) {
    try { port.close(); } catch (e) { /* already closed */ }
    port = null;
  }

  try {
    port = new SerialPort({ path: SERIAL_PATH, baudRate: SERIAL_BAUD });
    const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

    port.on('open', () => {
      console.log(`Serial port ${SERIAL_PATH} opened at ${SERIAL_BAUD} baud`);
      state.serialConnected = true;
      // Send initial score 2s after connect to let the Arduino finish booting
      setTimeout(() => {
        const cmd = `4,${state.batsmanA},${state.total},${state.batsmanB},${state.target},${state.wickets},${state.overs},${state.dls}#`;
        port.write(cmd);
        console.log(`Sent initial state to Arduino: ${cmd}`);
      }, 2000);
    });

    parser.on('data', (line) => {
      const trimmed = line.trim();
      console.log(`Arduino: ${trimmed}`);
      state.lastResponse = trimmed;
      // Broadcast to SSE clients
      for (const client of serialClients) {
        client.write(`data: ${trimmed}\n\n`);
      }
    });

    port.on('close', () => {
      console.log('Serial port closed. Reconnecting in 5s...');
      state.serialConnected = false;
      if (!port._reconnecting) {
        port._reconnecting = true;
        setTimeout(openSerial, 5000);
      }
    });

    port.on('error', (err) => {
      console.error(`Serial error: ${err.message}`);
      state.serialConnected = false;
      if (!port._reconnecting) {
        port._reconnecting = true;
        setTimeout(openSerial, 5000);
      }
    });
  } catch (err) {
    console.error(`Failed to open serial: ${err.message}. Retrying in 5s...`);
    setTimeout(openSerial, 5000);
  }
}

function sendToArduino(data) {
  if (SERIAL_PATH === 'none') {
    console.log(`[Mock serial] Would send: ${data}`);
    return true;
  }
  if (!port || !port.isOpen) {
    return false;
  }
  port.write(data);
  return true;
}

// --- Validation ---

// Each digit must be 0-9 or '-' (blank)
function isValidDigit(ch) {
  return (ch >= '0' && ch <= '9') || ch === '-';
}

function validateField(value, length) {
  if (typeof value !== 'string') return false;
  if (value.length !== length) return false;
  for (const ch of value) {
    if (!isValidDigit(ch)) return false;
  }
  return true;
}

// --- Play Cricket sync ---

const pcState = {
  matchId: null,
  apiToken: null,
  syncing: false,
  dryRun: false,
  lastSync: null,
  lastError: null,
  lastCmd: null,
  lastScore: null,
  intervalId: null,
  pollSeconds: parseInt(process.env.PC_POLL_SECONDS || '60', 10)
};

// Format a value from Play Cricket into a fixed-length scoreboard string.
// Leading positions are padded with '-' (blank); numeric value is right-aligned.
function fmtScore(n, len) {
  const val = parseInt(n, 10);
  if (isNaN(val) || val < 0) return '-'.repeat(len);
  const s = String(val);
  if (s.length >= len) return s.slice(-len);
  return s.padStart(len, '-');
}

function fetchPlayCricket(matchId, apiToken) {
  return new Promise((resolve, reject) => {
    const path = `/api/v2/match_detail.json?id=${encodeURIComponent(matchId)}&api_token=${encodeURIComponent(apiToken)}`;
    const req = https.request({ hostname: 'play-cricket.com', path }, (res) => {
      let data = '';
      res.on('data', chunk => { data += chunk; });
      res.on('end', () => {
        if (res.statusCode !== 200) {
          return reject(new Error(`Play Cricket API returned HTTP ${res.statusCode}`));
        }
        try { resolve(JSON.parse(data)); }
        catch (e) { reject(new Error('Invalid JSON from Play Cricket API')); }
      });
    });
    req.on('error', reject);
    req.end();
  });
}

function parsePlayCricketScore(json) {
  const match = json.match_details && json.match_details[0];
  if (!match) throw new Error('No match_details in response');
  const innings = match.innings;
  if (!innings || innings.length === 0) throw new Error('No innings data yet');

  // The current batting innings is the last one in the array
  const cur = innings[innings.length - 1];

  // Current batsmen have how_out === '' (empty string) in Play Cricket
  const notOut = (cur.bat || []).filter(b => !b.how_out || b.how_out.trim() === '');
  const batsmanA = notOut[0] ? fmtScore(notOut[0].runs, 3) : '---';
  const batsmanB = notOut[1] ? fmtScore(notOut[1].runs, 3) : '---';

  // Overs are returned as "32.4" (overs.balls); take only the whole-over part
  const oversWhole = cur.overs ? String(cur.overs).split('.')[0] : '0';

  // Target applies from the 2nd innings onward
  let target = '---';
  if (innings.length >= 2 && Number(cur.innings_number) > 1) {
    if (cur.revised_target_runs && cur.revised_target_runs !== '') {
      target = fmtScore(cur.revised_target_runs, 3);
    } else {
      const tgt = parseInt(innings[0].runs || '0', 10) + 1;
      target = fmtScore(String(tgt), 3);
    }
  }

  return {
    total:    fmtScore(cur.runs, 3),
    wickets:  fmtScore(cur.wickets, 1),
    overs:    fmtScore(oversWhole, 2),
    batsmanA,
    batsmanB,
    target,
    dls:      '---'
  };
}

async function doPlayCricketSync() {
  if (!pcState.syncing) return;
  try {
    const json = await fetchPlayCricket(pcState.matchId, pcState.apiToken);
    const score = parsePlayCricketScore(json);

    const cmd = `4,${score.batsmanA},${score.total},${score.batsmanB},${score.target},${score.wickets},${score.overs},${score.dls || '---'}#`;
    pcState.lastCmd   = cmd;
    pcState.lastScore = score;

    if (pcState.dryRun) {
      console.log(`[PlayCricket] DRY RUN - would send: ${cmd}`);
      console.log(`[PlayCricket] DRY RUN - score: ${JSON.stringify(score)}`);
    } else {
      // Update in-memory state so /api/status reflects the synced values
      state.total    = score.total;
      state.wickets  = score.wickets;
      state.overs    = score.overs;
      state.batsmanA = score.batsmanA;
      state.batsmanB = score.batsmanB;
      state.target   = score.target;
      state.dls      = score.dls || '---';
      sendToArduino(cmd);
      console.log(`[PlayCricket] Synced: ${JSON.stringify(score)}`);
    }

    pcState.lastSync  = new Date().toISOString();
    pcState.lastError = null;
  } catch (err) {
    pcState.lastError = err.message;
    console.error(`[PlayCricket] Sync error: ${err.message}`);
  }
}

function startPlayCricketSync(matchId, apiToken, dryRun = false) {
  stopPlayCricketSync();
  pcState.matchId   = matchId;
  pcState.apiToken  = apiToken;
  pcState.syncing   = true;
  pcState.dryRun    = dryRun;
  pcState.lastSync  = null;
  pcState.lastError = null;
  pcState.lastCmd   = null;
  doPlayCricketSync();
  pcState.intervalId = setInterval(doPlayCricketSync, pcState.pollSeconds * 1000);
  console.log(`[PlayCricket] Sync started for match ${matchId} (every ${pcState.pollSeconds}s, dryRun=${dryRun})`);
}

function stopPlayCricketSync() {
  if (pcState.intervalId) { clearInterval(pcState.intervalId); pcState.intervalId = null; }
  pcState.syncing = false;
  console.log('[PlayCricket] Sync stopped');
}

// --- Auth middleware ---

function requireAdmin(req, res, next) {
  const auth = req.headers.authorization;
  if (!auth || auth !== `Bearer ${ADMIN_TOKEN}`) {
    return res.status(401).json({ error: 'Unauthorized. Provide Authorization: Bearer <token>' });
  }
  next();
}

// --- Routes ---

// Verify a token without exposing it — used by the UI to show a live "connected" state
app.get('/api/auth/check', (req, res) => {
  const auth = req.headers.authorization;
  const valid = auth === `Bearer ${ADMIN_TOKEN}`;
  res.json({ valid });
});

app.post('/api/score', requireAdmin, (req, res) => {
  const { total, wickets, overs, batsmanA, batsmanB, target, dls } = req.body;

  // Validate field lengths: total(3), wickets(2), overs(2), batsmanA(3), batsmanB(3), target(3), dls(3)
  if (!validateField(total, 3)) return res.status(400).json({ error: 'Invalid total (3 digits)' });
  if (!validateField(wickets, 1)) return res.status(400).json({ error: 'Invalid wickets (1 digit)' });
  if (!validateField(overs, 2)) return res.status(400).json({ error: 'Invalid overs (2 digits)' });
  if (!validateField(batsmanA, 3)) return res.status(400).json({ error: 'Invalid batsmanA (3 digits)' });
  if (!validateField(batsmanB, 3)) return res.status(400).json({ error: 'Invalid batsmanB (3 digits)' });
  if (!validateField(target, 3)) return res.status(400).json({ error: 'Invalid target (3 digits)' });
  if (!validateField(dls, 3)) return res.status(400).json({ error: 'Invalid dls (3 digits)' });

  // Update state
  state.total = total;
  state.wickets = wickets;
  state.overs = overs;
  state.batsmanA = batsmanA;
  state.batsmanB = batsmanB;
  state.target = target;
  state.dls = dls;

  // Command format: 4,batA,total,batB,target,wickets,overs,dls#
  const cmd = `4,${batsmanA},${total},${batsmanB},${target},${wickets},${overs},${dls}#`;

  const sent = sendToArduino(cmd);
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }

  const summary = `Total: ${total} for ${wickets} wkts from ${overs} overs. ` +
    `Target: ${target}. Bat A: ${batsmanA}, Bat B: ${batsmanB}. DLS: ${dls}`;
  res.json({ ok: true, message: summary, command: cmd });
});

app.post('/api/test', (req, res) => {
  const sent = sendToArduino('5#');
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: 'Test mode activated' });
});

// Single-digit test endpoint
// POST /api/digit  body: { index: 0-17, glyph: "0"-"9" or "-" }
app.post('/api/digit', (req, res) => {
  const { index, glyph } = req.body;
  if (typeof index !== 'number' || index < 0 || index > 17) {
    return res.status(400).json({ error: 'index must be 0-17' });
  }
  const g = String(glyph);
  if (g.length !== 1 || (!(g >= '0' && g <= '9') && g !== '-')) {
    return res.status(400).json({ error: 'glyph must be 0-9 or -' });
  }
  const cmd = `digit,${index},${g}#`;
  const sent = sendToArduino(cmd);
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: `Sent: ${cmd}`, command: cmd });
});

app.post('/api/digit/delay', (req, res) => {
  const ms = req.body.ms || req.body.us;
  if (!Number.isInteger(ms) || ms < 0 || ms > 50000) {
    return res.status(400).json({ error: 'value must be an integer between 0 and 50000' });
  }
  const cmd = `delay,${ms}#`;
  const sent = sendToArduino(cmd);
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: `Sent: ${cmd}`, command: cmd });
});

app.post('/api/digit/walk', (req, res) => {
  const sent = sendToArduino('walk#');
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: 'Sent: walk#', command: 'walk#' });
});

app.post('/api/digit/scan', (req, res) => {
  const { index } = req.body;
  if (typeof index !== 'number' || index < 0 || index > 17) {
    return res.status(400).json({ error: 'index must be 0-17' });
  }
  const cmd = `scan,${index}#`;
  const sent = sendToArduino(cmd);
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: `Sent: ${cmd}`, command: cmd });
});

// Raw serial command — send any string directly to the Arduino
app.post('/api/serial', (req, res) => {
  const { command } = req.body;
  if (typeof command !== 'string' || command.length === 0 || command.length > 200) {
    return res.status(400).json({ error: 'command must be a non-empty string (max 200 chars)' });
  }
  // Append # terminator if not already present
  const cmd = command.endsWith('#') ? command : command + '#';
  const sent = sendToArduino(cmd);
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: `Sent: ${cmd}`, command: cmd });
});

// SSE stream of Arduino serial responses
const serialClients = [];

app.get('/api/serial/stream', (req, res) => {
  res.writeHead(200, {
    'Content-Type': 'text/event-stream',
    'Cache-Control': 'no-cache',
    'Connection': 'keep-alive'
  });
  res.write('data: connected\n\n');
  serialClients.push(res);
  req.on('close', () => {
    const idx = serialClients.indexOf(res);
    if (idx >= 0) serialClients.splice(idx, 1);
  });
});

// Clear all digits
app.post('/api/clear', (req, res) => {
  const sent = sendToArduino('clear#');
  if (!sent) {
    return res.status(503).json({ error: 'Serial port not connected' });
  }
  res.json({ ok: true, message: 'Display cleared' });
});

app.get('/api/status', (req, res) => {
  res.json({
    ...state,
    playCricket: {
      syncing:     pcState.syncing,
      dryRun:      pcState.dryRun,
      matchId:     pcState.matchId,
      lastSync:    pcState.lastSync,
      lastError:   pcState.lastError,
      lastScore:   pcState.lastScore,
      pollSeconds: pcState.pollSeconds
    }
  });
});

app.post('/api/playcricket/start', requireAdmin, (req, res) => {
  const { matchId, apiToken } = req.body;
  if (!matchId || !apiToken) {
    return res.status(400).json({ error: 'matchId and apiToken are required' });
  }
  const dryRun = req.body.dryRun === true;
  startPlayCricketSync(String(matchId), String(apiToken), dryRun);
  const mode = dryRun ? ' [DRY RUN - scoreboard will NOT be updated]' : '';
  res.json({ ok: true, message: `Play Cricket sync started for match ${matchId} (every ${pcState.pollSeconds}s)${mode}` });
});

app.post('/api/playcricket/stop', requireAdmin, (req, res) => {
  stopPlayCricketSync();
  res.json({ ok: true, message: 'Play Cricket sync stopped' });
});

app.post('/api/reboot', requireAdmin, (req, res) => {
  res.json({ ok: true, message: 'Rebooting...' });
  setTimeout(() => exec('shutdown -r now'), 500);
});

app.post('/api/shutdown', requireAdmin, (req, res) => {
  res.json({ ok: true, message: 'Shutting down...' });
  setTimeout(() => exec('shutdown now'), 500);
});

// Serve admin.html at /admin
app.get('/admin', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'admin.html'));
});

// Serve digit test page
app.get('/digit-test', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'digit-test.html'));
});

// Load Play Cricket config from USB stick
// Looks for scoreboard.json in common Pi USB mount paths
app.get('/api/playcricket/usb-config', (req, res) => {
  const searchRoots = ['/media/pi', '/media', '/mnt'];
  for (const root of searchRoots) {
    let entries;
    try { entries = fs.readdirSync(root); } catch { continue; }
    for (const entry of entries) {
      const candidate = path.join(root, entry, 'scoreboard.json');
      try {
        const raw = fs.readFileSync(candidate, 'utf8');
        const cfg = JSON.parse(raw);
        if (!cfg.matchId || !cfg.apiToken) {
          return res.status(400).json({ error: 'scoreboard.json must contain matchId and apiToken' });
        }
        console.log(`Loaded USB config from ${candidate}`);
        return res.json({ ok: true, matchId: String(cfg.matchId), apiToken: String(cfg.apiToken) });
      } catch { /* not found or invalid, keep scanning */ }
    }
  }
  res.status(404).json({ error: 'No scoreboard.json found on any USB drive. Make sure the USB is inserted and contains a scoreboard.json file.' });
});

// --- Start ---

openSerial();

app.listen(HTTP_PORT, () => {
  console.log(`Scoreboard server running on port ${HTTP_PORT}`);
  console.log(`Serial: ${SERIAL_PATH} @ ${SERIAL_BAUD} baud`);
});
