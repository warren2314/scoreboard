// Score state — each field is a string of digits/dashes
const score = {
  batsmanA: ['−', '−', '0'],   // 3 digits
  total:    ['−', '−', '0'],   // 3 digits
  batsmanB: ['−', '−', '0'],   // 3 digits
  target:   ['−', '−', '0'],   // 3 digits
  wickets:  ['0'],               // 1 digit
  overs:    ['−', '0'],         // 2 digits
  dls:      ['−', '−', '0']    // 3 digits
};

// Map display dash to wire dash
function toWire(ch) { return ch === '−' ? '-' : ch; }
function fromWire(ch) { return ch === '-' ? '−' : ch; }

// Valid values per position: first digit(s) can be blank or 1-9, last digit is always 0-9
function getValues(fieldName, digitIndex, fieldLength) {
  const isLastDigit = digitIndex === fieldLength - 1;
  if (isLastDigit) {
    return ['0','1','2','3','4','5','6','7','8','9'];
  }
  return ['−','0','1','2','3','4','5','6','7','8','9'];
}

// Cycle a digit up or down
function cycleDigit(fieldName, digitIndex, direction) {
  const field = score[fieldName];
  const values = getValues(fieldName, digitIndex, field.length);
  const current = values.indexOf(field[digitIndex]);
  let next = current + direction;
  if (next < 0) next = values.length - 1;
  if (next >= values.length) next = 0;
  field[digitIndex] = values[next];
  renderField(fieldName);
  sendScore();
}

// Render a single field's digit displays
function renderField(fieldName) {
  const field = score[fieldName];
  const container = document.getElementById(`field-${fieldName}`);
  const displays = container.querySelectorAll('.digit-display');
  displays.forEach((el, i) => {
    const val = field[i];
    el.textContent = val === '−' ? '−' : val;
    el.classList.toggle('blank', val === '−');
  });
}

// Render all fields
function renderAll() {
  for (const name of Object.keys(score)) {
    renderField(name);
  }
}

// Build the digit columns for a field
function buildField(fieldName) {
  const field = score[fieldName];
  const container = document.getElementById(`field-${fieldName}`);
  const digitsDiv = container.querySelector('.digits');
  digitsDiv.innerHTML = '';

  field.forEach((_, i) => {
    const col = document.createElement('div');
    col.className = 'digit-col';

    const upBtn = document.createElement('button');
    upBtn.className = 'digit-btn';
    upBtn.textContent = '+';
    upBtn.addEventListener('click', () => cycleDigit(fieldName, i, 1));

    const display = document.createElement('div');
    display.className = 'digit-display';

    const downBtn = document.createElement('button');
    downBtn.className = 'digit-btn';
    downBtn.textContent = '−';
    downBtn.addEventListener('click', () => cycleDigit(fieldName, i, -1));

    col.appendChild(upBtn);
    col.appendChild(display);
    col.appendChild(downBtn);
    digitsDiv.appendChild(col);
  });
}

// Send current score to server
let sendTimeout = null;
function sendScore() {
  // Debounce rapid changes (50ms)
  clearTimeout(sendTimeout);
  sendTimeout = setTimeout(doSend, 50);
}

function getToken() {
  return document.getElementById('token').value;
}

async function doSend() {
  const token = getToken();
  if (!token) {
    const statusBar = document.getElementById('status');
    statusBar.textContent = 'Enter the scorer token first';
    statusBar.className = 'status-bar error';
    return;
  }

  const body = {
    total:    score.total.map(toWire).join(''),
    wickets:  score.wickets.map(toWire).join(''),
    overs:    score.overs.map(toWire).join(''),
    batsmanA: score.batsmanA.map(toWire).join(''),
    batsmanB: score.batsmanB.map(toWire).join(''),
    target:   score.target.map(toWire).join(''),
    dls:      score.dls.map(toWire).join('')
  };

  const statusBar = document.getElementById('status');
  try {
    const res = await fetch('/api/score', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'Authorization': `Bearer ${token}` },
      body: JSON.stringify(body)
    });
    const data = await res.json();
    if (res.status !== 401) {
      localStorage.setItem('authToken', token);
    }
    if (data.ok) {
      statusBar.textContent = data.message;
      statusBar.className = 'status-bar ok';
    } else {
      statusBar.textContent = data.error || 'Error';
      statusBar.className = 'status-bar error';
    }
  } catch (err) {
    statusBar.textContent = `Connection error: ${err.message}`;
    statusBar.className = 'status-bar error';
  }
}

// Poll serial connection status and restore score state from server
async function pollStatus() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();
    const dot = document.getElementById('connection-dot');
    const label = document.getElementById('connection-label');
    if (dot && label) {
      dot.classList.toggle('connected', data.serialConnected);
      label.textContent = data.serialConnected ? 'Arduino connected' : 'Arduino disconnected';
    }
    return data;
  } catch {
    // ignore
  }
}

// Restore score object from server state (wire format → display format)
function restoreFromServer(data) {
  const map = {
    batsmanA: data.batsmanA,
    total:    data.total,
    batsmanB: data.batsmanB,
    target:   data.target,
    wickets:  data.wickets,
    overs:    data.overs,
    dls:      data.dls
  };
  for (const [field, wire] of Object.entries(map)) {
    if (wire && wire.length === score[field].length) {
      score[field] = wire.split('').map(fromWire);
    }
  }
}

// Reset all scores to zero and send to board
function newInnings() {
  score.batsmanA = ['−', '−', '0'];
  score.total    = ['−', '−', '0'];
  score.batsmanB = ['−', '−', '0'];
  score.target   = ['−', '−', '0'];
  score.wickets  = ['0'];
  score.overs    = ['−', '0'];
  score.dls      = ['−', '−', '0'];
  renderAll();
  sendScore();
}

// Init
document.addEventListener('DOMContentLoaded', async () => {
  // Restore saved token so the scorer doesn't re-enter it every visit
  const saved = localStorage.getItem('authToken');
  if (saved) document.getElementById('token').value = saved;

  for (const name of Object.keys(score)) {
    buildField(name);
  }

  // Restore current score from server so a page refresh doesn't wipe the board
  const data = await pollStatus();
  if (data) restoreFromServer(data);
  renderAll();

  // New Innings button
  document.getElementById('btn-new-innings').addEventListener('click', () => {
    if (confirm('Reset all scores to zero and send to the board?')) {
      newInnings();
    }
  });

  setInterval(pollStatus, 5000);
});
